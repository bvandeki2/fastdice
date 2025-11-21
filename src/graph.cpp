#include "graph.hpp"

#include "fft.hpp"
#include <numeric>
#include <stdexcept>

namespace fastdice::graph {

void naiveConvolve(
    std::vector<float_t>::const_iterator lhsBegin,
    std::vector<float_t>::const_iterator lhsEnd,
    std::vector<float_t>::const_iterator rhsBegin,
    std::vector<float_t>::const_iterator rhsEnd,
    std::vector<float_t>::iterator resultBegin
) {
    size_t lhsSize = std::distance(lhsBegin, lhsEnd);
    size_t rhsSize = std::distance(rhsBegin, rhsEnd);

    for (size_t i = 0; i < lhsSize; ++i) {
        for (size_t j = 0; j < rhsSize; ++j) {
            *(resultBegin + i + j) += *(lhsBegin + i) * *(rhsBegin + j);
        }
    }
}

DistOp::DistOp(int32_t min, int32_t max, std::vector<std::shared_ptr<DistOp>> args)
    : min(min)
    , max(max)
    , args(std::move(args))
    , p(std::nullopt) {
    if (min > max) {
        throw std::invalid_argument("Distribution operation min must be leq max.");
    }
}

const std::vector<float_t>& DistOp::calcProbabilities() {
    if (!p.has_value()) {
        this->calculate();
    }
    return p.value();
}

Uniform::Uniform(int32_t min, int32_t max)
    : DistOp(min, max, {}) {
    if (min > max) {
        throw std::invalid_argument("Uniform distribution min must be less than or equal to max.");
    }
}

void Uniform::calculate() {
    size_t size = this->size();
    std::vector<float_t> probabilities(size, 1.0f / size);
    this->p = std::move(probabilities);
}

Add::Add(DistOpPtr lhs, DistOpPtr rhs)
    : DistOp(lhs->getMin() + rhs->getMin(), lhs->getMax() + rhs->getMax(), { lhs, rhs }) { }

void Add::calculate() {
    const auto& lhsProb = args[0]->calcProbabilities();
    const auto& rhsProb = args[1]->calcProbabilities();

    // handle addition by constants
    if (lhsProb.size() == 1) {
        this->p = rhsProb;
        return;
    }
    if (rhsProb.size() == 1) {
        this->p = lhsProb;
        return;
    }

    size_t resultSize = this->size();
    std::vector<float_t> result(resultSize, 0.0f);

    // TODO: an actual strategy to choose between naive and FFT
    if (lhsProb.size() >= 64 && rhsProb.size() >= 64) {
        convolveFFT(lhsProb.begin(), lhsProb.end(), rhsProb.begin(), rhsProb.end(), result.begin());
    } else {
        naiveConvolve(
            lhsProb.begin(), lhsProb.end(), rhsProb.begin(), rhsProb.end(), result.begin()
        );
    }

    this->p = std::move(result);
}

Negate::Negate(DistOpPtr operand)
    : DistOp(-operand->getMax(), -operand->getMin(), { operand }) { }

void Negate::calculate() {
    const auto& operandProb = args[0]->calcProbabilities();
    size_t size = this->size();
    std::vector<float_t> result(size, 0.0f);

    for (size_t i = 0; i < operandProb.size(); ++i) {
        result[size - i - 1] = operandProb[i];
    }

    this->p = std::move(result);
}

Multiply::Multiply(DistOpPtr lhs, DistOpPtr rhs)
    : DistOp(lhs->getMin() * rhs->getMin(), lhs->getMax() * rhs->getMax(), { lhs, rhs }) { }

void Multiply::calculate() {
    const auto& lhsProb = args[0]->calcProbabilities();
    const auto& rhsProb = args[1]->calcProbabilities();

    size_t resultSize = this->size();
    std::vector<float_t> result(resultSize, 0.0f);

    for (size_t i = 0; i < lhsProb.size(); ++i) {
        for (size_t j = 0; j < rhsProb.size(); ++j) {
            int32_t product = (i + args[0]->getMin()) * (j + args[1]->getMin());
            result[product - this->min] += lhsProb[i] * rhsProb[j];
        }
    }

    this->p = std::move(result);
}

Keep::Keep(std::vector<DistOpPtr> dists, size_t begin, size_t end)
    : DistOp(dists.front()->getMin(), dists.front()->getMax(), dists)
    , keepBegin(begin)
    , keepEnd(end) {
    if (begin >= end) {
        throw std::invalid_argument("Keep operation begin must be less than end.");
    }
}

void Keep::calculate() {
    // placeholder: we only support largest and smallest for now
    if (keepBegin == 0 && keepEnd == 1) {
        // minimum of independent dists
        const size_t distCount = args.size();
        std::vector<std::vector<float_t>> suffixes;
        suffixes.reserve(distCount);

        for (const auto& dist : args) {
            const auto& probs = dist->calcProbabilities();
            std::vector<float_t> suffix(probs.size() + 1, 0.0f);
            float_t running = 0.0f;
            for (size_t i = probs.size(); i-- > 0;) {
                running += probs[i];
                suffix[i] = running;
            }
            suffixes.push_back(std::move(suffix));
        }

        const auto tailProb = [&](size_t distIdx, int32_t value) -> float_t {
            const auto& dist = args[distIdx];
            const int32_t distMin = dist->getMin();
            const int32_t distMax = dist->getMax();
            if (value <= distMin) {
                return 1.0f;
            }
            if (value >= distMax) {
                return 0.0f;
            }
            const size_t offset = static_cast<size_t>(value - distMin);
            return suffixes[distIdx][offset];
        };

        std::vector<float_t> result(this->size(), 0.0f);
        for (size_t idx = 0; idx < result.size(); ++idx) {
            const int32_t value = this->min + static_cast<int32_t>(idx);
            float_t geValue = 1.0f;
            float_t geNext = 1.0f;
            for (size_t distIdx = 0; distIdx < distCount; ++distIdx) {
                geValue *= tailProb(distIdx, value);
                geNext *= tailProb(distIdx, value + 1);
            }
            float_t mass = geValue - geNext;
            if (mass < 0.0f) {
                mass = 0.0f;
            }
            result[idx] = mass;
        }

        this->p = std::move(result);
    } else if (keepBegin == args.size() - 1 && keepEnd == args.size()) {
        // maximum of independent dists
        const size_t distCount = args.size();
        std::vector<std::vector<float_t>> prefixes;
        prefixes.reserve(distCount);

        for (const auto& dist : args) {
            const auto& probs = dist->calcProbabilities();
            std::vector<float_t> prefix(probs.size() + 1, 0.0f);
            float_t running = 0.0f;
            for (size_t i = 0; i < probs.size(); ++i) {
                running += probs[i];
                prefix[i + 1] = running;
            }
            prefixes.push_back(std::move(prefix));
        }

        const auto cdf = [&](size_t distIdx, int32_t value) -> float_t {
            const auto& dist = args[distIdx];
            const int32_t distMin = dist->getMin();
            const int32_t distMax = dist->getMax();
            if (value < distMin) {
                return 0.0f;
            }
            if (value >= distMax) {
                return 1.0f;
            }
            const size_t offset = static_cast<size_t>(value - distMin + 1);
            return prefixes[distIdx][offset];
        };

        std::vector<float_t> result(this->size(), 0.0f);
        for (size_t idx = 0; idx < result.size(); ++idx) {
            const int32_t value = this->min + static_cast<int32_t>(idx);
            float_t leValue = 1.0f;
            float_t lePrev = 1.0f;
            for (size_t distIdx = 0; distIdx < distCount; ++distIdx) {
                leValue *= cdf(distIdx, value);
                lePrev *= cdf(distIdx, value - 1);
            }
            float_t mass = leValue - lePrev;
            if (mass < 0.0f) {
                mass = 0.0f;
            }
            result[idx] = mass;
        }

        this->p = std::move(result);
    } else {
        throw std::runtime_error("Keep operation with arbitrary ranges not implemented.");
    }
}

Repeat::Repeat(DistOpPtr dist, size_t times)
    : DistOp(dist->getMin() * times, dist->getMax() * times, { dist })
    , times(times) { }

void Repeat::calculate() {
    if (times == 0) {
        // degenerate distribution at 0
        std::vector<float_t> result(1, 1.0f);
        this->p = std::move(result);
        return;
    }
    if (times == 1) {
        this->p = args[0]->calcProbabilities();
        return;
    }

    const auto& distProb = args[0]->calcProbabilities();

    size_t resultSize = this->size();
    std::vector<float_t> result(resultSize, 0.0f);

    if (times == 2 && distProb.size() < 64) {
        naiveConvolve(
            distProb.begin(), distProb.end(), distProb.begin(), distProb.end(), result.begin()
        );
    } else {
        nConvolveFFT(distProb.begin(), distProb.end(), times, result.begin());
    }

    this->p = std::move(result);
}
Partition::Partition(DistOpPtr dist, std::vector<int32_t> splits, std::vector<size_t> branchIds)
    : DistOp(0, 0, { std::move(dist) })
    , splits(std::move(splits))
    , branchIds(std::move(branchIds)) {
    for (size_t i = 1; i < this->splits.size(); ++i) {
        if (this->splits[i] <= this->splits[i - 1]) {
            throw std::invalid_argument("Partition splits must be strictly increasing.");
        }
    }

    if (this->branchIds.size() != this->splits.size() + 1) {
        throw std::invalid_argument("Mismatch between number of partition branches and splits.");
    }

    size_t expected = 0;
    for (size_t id : this->branchIds) {
        if (id > expected) {
            throw std::invalid_argument("Partition branch IDs must not skip values.");
        }
        if (id == expected) {
            expected++;
        }
    }

    this->min = 0;
    this->max = static_cast<int32_t>(expected - 1);
}
void Partition::calculate() {
    const auto& distProb = args[0]->calcProbabilities();
    const int32_t distMin = args[0]->getMin();
    const int32_t distMax = args[0]->getMax();

    size_t resultSize = this->size();

    std::vector<float_t> result(resultSize, 0.0f);

    for (size_t i = 0; i < this->branchIds.size(); ++i) {
        // `splits` separates the set of integers into `splits.size() + 1` spans
        // defined on the half-open intervals of pairwise:
        // [-inf, splits[0]), [splits[0], splits[1]), ..., [splits[n-1], +inf)
        // of course, we limit to the actual distribution range in implementation
        int32_t spanMin = (i == 0) ? distMin : splits[i - 1];
        int32_t spanMax = (i == splits.size()) ? distMax : splits[i] - 1;
        if (spanMin > spanMax) {
            // empty branch, shouldn't happen with well-specified splits
            continue;
        }
        size_t spanSize = static_cast<size_t>(spanMax - spanMin + 1);
        size_t branchId = branchIds[i];
        for (size_t j = 0; j < spanSize; ++j) {
            int32_t outcome = spanMin + static_cast<int32_t>(j);
            result[branchId] += distProb[static_cast<size_t>(outcome - distMin)];
        }
    }

    this->p = std::move(result);
}

std::tuple<int32_t, int32_t> Partition::getBranchRange(size_t branchId) {
    int32_t branchMin = std::numeric_limits<int32_t>::max();
    int32_t branchMax = std::numeric_limits<int32_t>::min();

    for (size_t i = 0; i < this->branchIds.size(); ++i) {
        if (this->branchIds[i] == branchId) {
            int32_t spanMin = (i == 0) ? args[0]->getMin() : splits[i - 1];
            int32_t spanMax = (i == splits.size()) ? args[0]->getMax() : splits[i];
            branchMin = std::min(branchMin, spanMin);
            branchMax = std::max(branchMax, spanMax);
        }
    }

    if (branchMin > branchMax) {
        throw std::out_of_range("Branch does not exist or has no support.");
    }

    return { branchMin, branchMax };
}

std::vector<float_t> Partition::calculateConditionalDistribution(
    size_t branchId, int32_t branchMin, int32_t branchMax
) {
    const auto& distProb = args[0]->calcProbabilities();

    size_t branchSize = static_cast<size_t>(branchMax - branchMin + 1);
    std::vector<float_t> conditionalDist(branchSize, 0.0f);

    const int32_t distMin = args[0]->getMin();
    for (int32_t value = branchMin; value <= branchMax; ++value) {
        conditionalDist[static_cast<size_t>(value - branchMin)]
            = distProb[static_cast<size_t>(value - distMin)];
    }

    // Normalize the distribution
    float_t total = std::accumulate(conditionalDist.begin(), conditionalDist.end(), 0.0f);
    for (auto& prob : conditionalDist) {
        prob /= total;
    }

    return conditionalDist;
}

BranchInput::BranchInput(DistOpPtr partitionDist, size_t branchId)
    : DistOp(0, 0, { std::move(partitionDist) })
    , branchId(branchId) {
    auto partitionPtr = std::dynamic_pointer_cast<Partition>(args[0]);
    if (!partitionPtr) {
        throw std::invalid_argument("BranchInput requires a Partition node as its argument.");
    }

    auto [branchMin, branchMax] = partitionPtr->getBranchRange(branchId);
    this->min = branchMin;
    this->max = branchMax;
}

void BranchInput::calculate() {
    // static cast is probably safe too
    auto partitionPtr = std::dynamic_pointer_cast<Partition>(args[0]);
    if (!partitionPtr) {
        throw std::runtime_error("BranchInput argument is not a Partition node.");
    }

    std::vector<float_t> conditionalDist
        = partitionPtr->calculateConditionalDistribution(branchId, this->min, this->max);

    this->p = std::move(conditionalDist);
}

Merge::Merge(DistOpPtr idxDist, std::vector<DistOpPtr> branchDists)
    : DistOp(0, 0, std::vector<DistOpPtr> { std::move(idxDist) }) {
    // determine min and max from branches
    // constructor will yell at us if we tried to do this above
    this->min = std::numeric_limits<int32_t>::max();
    this->max = std::numeric_limits<int32_t>::min();

    for (auto& dist : branchDists) {
        if (dist->getMin() < this->min) {
            this->min = dist->getMin();
        }
        if (dist->getMax() > this->max) {
            this->max = dist->getMax();
        }
        this->args.push_back(std::move(dist));
    }
}

void Merge::calculate() {
    const auto& idxProb = args[0]->calcProbabilities();
    size_t resultSize = this->size();
    std::vector<float_t> result(resultSize, 0.0f);

    for (size_t branchId = 0; branchId < args.size() - 1; ++branchId) {
        const auto& branchProb = args[branchId + 1]->calcProbabilities();
        int32_t branchMin = args[branchId + 1]->getMin();
        for (size_t i = 0; i < branchProb.size(); ++i) {
            int32_t outcome = branchMin + static_cast<int32_t>(i);
            result[static_cast<size_t>(outcome - this->min)] += idxProb[branchId] * branchProb[i];
        }
    }

    this->p = std::move(result);
}

} // namespace fastdice::graph