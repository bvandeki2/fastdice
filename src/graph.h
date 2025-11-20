#ifndef FASTDICE_GRAPH_H
#define FASTDICE_GRAPH_H

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace fastdice::graph {
class Node;

class FunctionParam { };

class Uniform {
    int32_t min;
    int32_t max;

public:
    Uniform(int32_t min, int32_t max);

    friend class Node;
};

class Sum {
    std::vector<std::shared_ptr<Node>> args;

public:
    explicit Sum(std::vector<std::shared_ptr<Node>> args);

    friend class Node;
};

class Negate {
    std::shared_ptr<Node> arg;

public:
    explicit Negate(std::shared_ptr<Node> arg);

    friend class Node;
};

class NthSmallest {
    std::vector<std::shared_ptr<Node>> args;
    std::shared_ptr<Node> n;

public:
    NthSmallest(std::vector<std::shared_ptr<Node>> args, std::shared_ptr<Node> n);

    friend class Node;
};

class Repeat {
    std::shared_ptr<Node> arg;
    std::shared_ptr<Node> count;

public:
    Repeat(std::shared_ptr<Node> arg, std::shared_ptr<Node> count);

    friend class Node;
};

class Partition {
    std::shared_ptr<Node> arg;
    /**
     * sequence of ints, sorted, that partition integers [-inf, splits[0], ..., splits[n], inf]
     * lower side inclusive, upper side exclusive
     */
    std::vector<int32_t> splits;
    /**
     * branch IDs corresponding to each partition
     * by (enforced) convention, this should not "skip" entries; i.e., skimming from left to right,
     * when we see a parition with branchID x, we should have seen a branchIDs < x already
     *
     * this is so that any partitions that behave the same can be readily identified as such
     */
    std::vector<uint32_t> branchIds;
    std::vector<std::shared_ptr<Node>> branchNodes;

public:
    Partition(
        std::shared_ptr<Node> arg,
        std::vector<int32_t> splits,
        std::vector<uint32_t> branchIds,
        std::vector<std::shared_ptr<Node>> branchNodes
    );

    friend class Node;
};

class Node {
    std::variant<FunctionParam, Uniform, Sum, Negate, NthSmallest, Repeat, Partition> op;

    // computed bounds for each, not initialized until calculateBounds is called
    int32_t min;
    int32_t max;

public:
    Node(std::variant<FunctionParam, Uniform, Sum, Negate, NthSmallest, Repeat, Partition> op);
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = default;
    Node& operator=(Node&&) = default;

    int32_t getMin() const { return min; }
    int32_t getMax() const { return max; }
    bool isConstant() const { return min == max; }

    static std::shared_ptr<Node> createFunctionParam();
    static std::shared_ptr<Node> createUniform(int32_t min, int32_t max);
    static std::shared_ptr<Node> createSum(std::vector<std::shared_ptr<Node>> args);
    static std::shared_ptr<Node> createNegate(std::shared_ptr<Node> arg);
    static std::shared_ptr<Node> createNthSmallest(
        std::vector<std::shared_ptr<Node>> args, std::shared_ptr<Node> n
    );
    static std::shared_ptr<Node> createRepeat(
        std::shared_ptr<Node> arg, std::shared_ptr<Node> count
    );
    static std::shared_ptr<Node> createPartition(
        std::shared_ptr<Node> arg,
        std::vector<int32_t> splits,
        std::vector<uint32_t> branchIds,
        std::vector<std::shared_ptr<Node>> branchNodes
    );

    void calculateBounds(
        const std::optional<std::pair<int32_t, int32_t>>& paramBounds = std::nullopt
    );
    void simplify();
};
} // namespace fastdice::graph

#endif // GRAPH_H