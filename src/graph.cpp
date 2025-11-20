#include "graph.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace fastdice::graph {

using NodeOp = std::variant<FunctionParam, Uniform, Sum, Negate, NthSmallest, Repeat, Partition>;

Node::Node(NodeOp op)
    : op(std::move(op))
    , min(0)
    , max(0) { }

std::shared_ptr<Node> Node::createFunctionParam() {
    return std::make_shared<Node>(NodeOp(FunctionParam {}));
}

std::shared_ptr<Node> Node::createUniform(int32_t min, int32_t max) {
    return std::make_shared<Node>(NodeOp(Uniform(min, max)));
}

std::shared_ptr<Node> Node::createSum(std::vector<std::shared_ptr<Node>> args) {
    return std::make_shared<Node>(NodeOp(Sum(std::move(args))));
}

std::shared_ptr<Node> Node::createNegate(std::shared_ptr<Node> arg) {
    return std::make_shared<Node>(NodeOp(Negate(std::move(arg))));
}

std::shared_ptr<Node> Node::createNthSmallest(
    std::vector<std::shared_ptr<Node>> args, std::shared_ptr<Node> n
) {
    return std::make_shared<Node>(NodeOp(NthSmallest(std::move(args), std::move(n))));
}

std::shared_ptr<Node> Node::createRepeat(std::shared_ptr<Node> arg, std::shared_ptr<Node> count) {
    return std::make_shared<Node>(NodeOp(Repeat(std::move(arg), std::move(count))));
}

std::shared_ptr<Node> Node::createPartition(
    std::shared_ptr<Node> arg,
    std::vector<int32_t> splits,
    std::vector<uint32_t> branchIds,
    std::vector<std::shared_ptr<Node>> branchNodes
) {
    return std::make_shared<Node>(NodeOp(
        Partition(std::move(arg), std::move(splits), std::move(branchIds), std::move(branchNodes))
    ));
}

void Node::calculateBounds(const std::optional<std::pair<int32_t, int32_t>>& paramBounds) {
    std::visit(
        [this, &paramBounds](auto&& op) {
            using T = std::decay_t<decltype(op)>;

            if constexpr (std::is_same_v<T, Uniform>) {
                min = op.min;
                max = op.max;
            } else if constexpr (std::is_same_v<T, Sum>) {
                int32_t totalMin = 0;
                int32_t totalMax = 0;
                for (const auto& arg : op.args) {
                    arg->calculateBounds(paramBounds);
                    totalMin += arg->min;
                    totalMax += arg->max;
                }
                min = totalMin;
                max = totalMax;
            } else if constexpr (std::is_same_v<T, Negate>) {
                op.arg->calculateBounds(paramBounds);
                min = -op.arg->max;
                max = -op.arg->min;
            } else if constexpr (std::is_same_v<T, FunctionParam>) {
                if (paramBounds.has_value()) {
                    min = paramBounds->first;
                    max = paramBounds->second;
                } else {
                    throw std::runtime_error(
                        "Cannot calculate bounds for FunctionParam nodes; is it not inside a "
                        "Partition "
                        "branch?"
                    );
                }
            } else if constexpr (std::is_same_v<T, NthSmallest>) {
                for (const auto& arg : op.args) {
                    arg->calculateBounds(paramBounds);
                }
                op.n->calculateBounds(paramBounds);
                if (op.n->min < 0 || op.n->max >= static_cast<int32_t>(op.args.size())) {
                    throw std::runtime_error(
                        "NthSmallest node has n out of bounds of its args size"
                    );
                }
                // smallest case is when: each roll was minimum (so arg->min for each), and n is
                // min, too
                std::vector<int32_t> vals;
                vals.reserve(op.args.size());
                for (const auto& arg : op.args) {
                    vals.push_back(arg->min);
                }
                std::nth_element(vals.begin(), vals.begin() + op.n->min, vals.end());
                min = vals[op.n->min];

                // largest case is when: each roll was maximum (so arg->max for each), and n is max,
                // too
                vals.clear();
                for (const auto& arg : op.args) {
                    vals.push_back(arg->max);
                }

                std::nth_element(
                    vals.begin(), vals.begin() + op.n->max, vals.end(), std::greater<int32_t>()
                );
                max = vals[op.n->max];
            } else if constexpr (std::is_same_v<T, Repeat>) {
                op.arg->calculateBounds(paramBounds);
                op.count->calculateBounds(paramBounds);
                min = op.arg->min * op.count->min;
                max = op.arg->max * op.count->max;
            } else if constexpr (std::is_same_v<T, Partition>) {
                op.arg->calculateBounds(paramBounds);

                // doing it this way for (theoretically) easier templating later
                constexpr auto INF_MIN = std::numeric_limits<int32_t>::min();
                constexpr auto INF_MAX = std::numeric_limits<int32_t>::max();

                std::vector<std::pair<int32_t, int32_t>> branchBounds(
                    op.branchNodes.size(), { INT_MAX, INT_MIN }
                );
                for (size_t i = 0; i < op.branchIds.size(); ++i) {
                    // inclusive lower, exclusive upper
                    int32_t spanMin = (i == 0) ? op.arg->min : op.splits[i - 1];
                    int32_t spanMax = (i == op.splits.size()) ? op.arg->max : op.splits[i] - 1;

                    if (spanMin >= spanMax) {
                        // empty span, warn and skip...
                        // TODO: proper logging
                        std::cerr << "[warn] empty partition span detected in calculateBounds"
                                  << std::endl;
                        continue;
                    }

                    // branches may apply to many spans
                    std::pair<int32_t, int32_t>& bMinMax = branchBounds[op.branchIds[i]];
                    if (spanMin < bMinMax.first) {
                        bMinMax.first = spanMin;
                    }
                    if (spanMax > bMinMax.second) {
                        bMinMax.second = spanMax;
                    }
                }

                for (size_t i = 0; i < op.branchNodes.size(); ++i) {
                    // look, it's the whole reason paramBounds exists!
                    const auto& branchParamBounds = branchBounds[i];
                    if (branchParamBounds.first == INT_MAX && branchParamBounds.second == INT_MIN) {
                        // empty branch? spans are one thing, but this we don't run this branch at
                        // all
                        std::cerr << "[warn] empty partition branch detected in calculateBounds"
                                  << std::endl;
                        continue;
                    }
                    op.branchNodes[i]->calculateBounds(branchParamBounds);
                }

                // overall min/max is min/max of each branch node's min/max
                min = INT_MAX;
                max = INT_MIN;
                for (const auto& branchNode : op.branchNodes) {
                    if (branchNode->min < min) {
                        min = branchNode->min;
                    }
                    if (branchNode->max > max) {
                        max = branchNode->max;
                    }
                }
            } else {
                throw std::runtime_error(
                    "[unreachable] calculateBounds not implemented for this node type yet?"
                );
            }
        },
        op
    );
}

void Node::simplify() { }

Uniform::Uniform(int32_t min, int32_t max)
    : min(min)
    , max(max) {
    if (min > max) {
        throw std::invalid_argument("Uniform distribution min must be <= max");
    }
}

Sum::Sum(std::vector<std::shared_ptr<Node>> args)
    : args(std::move(args)) { }

Negate::Negate(std::shared_ptr<Node> arg)
    : arg(std::move(arg)) { }

NthSmallest::NthSmallest(std::vector<std::shared_ptr<Node>> args, std::shared_ptr<Node> n)
    : args(std::move(args))
    , n(std::move(n)) { }

Repeat::Repeat(std::shared_ptr<Node> arg, std::shared_ptr<Node> count)
    : arg(std::move(arg))
    , count(std::move(count)) { }

Partition::Partition(
    std::shared_ptr<Node> arg,
    std::vector<int32_t> splits,
    std::vector<uint32_t> branchIds,
    std::vector<std::shared_ptr<Node>> branchNodes
)
    : arg(std::move(arg))
    , splits(std::move(splits))
    , branchIds(std::move(branchIds))
    , branchNodes(std::move(branchNodes)) {
    if (this->splits.size() + 1 != this->branchIds.size()) {
        throw std::invalid_argument("Partition must have exactly one more branch ID than splits");
    }
    // branch ids doesn't skip entries
    uint32_t expectedId = 0;
    for (uint32_t id : this->branchIds) {
        if (id > expectedId) {
            throw std::invalid_argument(
                "Partition branch IDs must not skip entries; found out-of-order IDs"
            );
        }
        if (id == expectedId) {
            expectedId = id + 1;
        }
    }

    if (expectedId != this->branchNodes.size()) {
        throw std::invalid_argument(
            "Partition must have exactly one branch node for each unique branch ID"
        );
    }
}
} // namespace fastdice::graph
