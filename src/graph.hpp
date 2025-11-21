#ifndef FASTDICE_GRAPH_HPP
#define FASTDICE_GRAPH_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace fastdice::graph {

class DistOp {
protected:
    // if calculated, probabilities are stored here
    std::optional<std::vector<float_t>> p;

    // range of the distribution, closed [min, max]
    int32_t min;
    int32_t max;

    // arguments/dependencies of this operation
    std::vector<std::shared_ptr<DistOp>> args;

    /**
     * Calculate the probabilities for this distribution operation.
     * Should only be called once, when p is not yet calculated.
     */
    virtual void calculate() = 0;

public:
    DistOp(int32_t min, int32_t max, std::vector<std::shared_ptr<DistOp>> args);
    virtual ~DistOp() = default;
    const std::vector<float_t>& calcProbabilities();

    int32_t getMin() const { return min; }
    int32_t getMax() const { return max; }
    size_t size() const { return static_cast<size_t>(max - min + 1); }
};

using DistOpPtr = std::shared_ptr<DistOp>;

class Uniform : public DistOp {
public:
    explicit Uniform(int32_t min, int32_t max);
    void calculate() override;
};

class Add : public DistOp {
public:
    Add(DistOpPtr lhs, DistOpPtr rhs);
    void calculate() override;
};

class Negate : public DistOp {
public:
    explicit Negate(DistOpPtr operand);
    void calculate() override;
};

class Multiply : public DistOp {
public:
    Multiply(DistOpPtr rhs, DistOpPtr lhs);
    void calculate() override;
};

/**
 * @brief Represents a distribution operation that sorts, filters, and sums the given distributions.
 */
class Keep : public DistOp {
    size_t keepBegin;
    size_t keepEnd;

public:
    Keep(std::vector<DistOpPtr> dists, size_t begin, size_t end);
    void calculate() override;
};

/**
 * @brief Represents a distribution operation that repeats the given distribution a specified number
 * of times. For large numbers of repetitions, this is much more efficient than adding the
 * distribution to itself multiple times.
 */
class Repeat : public DistOp {
    size_t times;

public:
    Repeat(DistOpPtr dist, size_t times);
    void calculate() override;
};

class Partition : public DistOp {
    std::vector<int32_t> splits;
    std::vector<size_t> branchIds;

    // half-open range of the given branch
    std::tuple<int32_t, int32_t> getBranchRange(size_t branchId);

    // takes the subset, trims, and normalizes the probabilities for the given branch.
    // accepts the branch min and max to avoid recomputing them; caller's responsibility to ensure
    // they are for this branch ID.
    std::vector<float_t> Partition::calculateConditionalDistribution(
        size_t branchId, int32_t branchMin, int32_t branchMax
    );

public:
    Partition(DistOpPtr dist, std::vector<int32_t> splits, std::vector<size_t> branchIds);
    void calculate() override;

    friend class BranchInput;
};

/**
 * @brief Represents a distribution operation that selects the conditional branch distribution
 * based on the provided ``Partition`` distop.
 */
class BranchInput : public DistOp {
    size_t branchId;

public:
    BranchInput(DistOpPtr partitionDist, size_t branchId);
    void calculate() override;
};

/**
 * @brief Represents a distribution operation that merges multiple branch distributions based on
 * an index distribution.
 *
 * The typical use case is to handle recombining partitioned distributions after conditional
 * branches, but this supports the general case of calculating a mixture of distributions.
 */
class Merge : public DistOp {
public:
    Merge(DistOpPtr idxDist, std::vector<DistOpPtr> branchDists);
    void calculate() override;
};

} // namespace fastdice::graph

#endif // FASTDICE_GRAPH_HPP