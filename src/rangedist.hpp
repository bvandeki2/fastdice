#ifndef RANGEDIST_H
#define RANGEDIST_H

#include "graph.hpp"
#include <cstdint>
#include <functional>
#include <vector>

/**
 * @class RangeDist
 * @brief Represents a discrete probability distribution over a range of integers.
 *
 * This class is a utility wrapper around the underlying distribution operation graph.
 * It evaluates eagerly and provides methods for mapping functions over the distribution,
 * repeating the distribution, and computing percentiles.
 */
class RangeDist {
    std::shared_ptr<fastdice::graph::DistOp> distOp;

    RangeDist(std::shared_ptr<fastdice::graph::DistOp> distOp);

public:
    static RangeDist uniform(int32_t min, int32_t max);

    RangeDist(int32_t value);

    const std::vector<float_t>& calcProbabilities() const;
    int32_t getMin() const;
    int32_t getMax() const;

    RangeDist operator-() const;
    RangeDist operator*(const RangeDist& other) const;
    RangeDist operator+(const RangeDist& other) const;
    RangeDist operator-(const RangeDist& other) const;
    RangeDist repeat(uint32_t count) const;

    static RangeDist maximum(std::vector<RangeDist> dists);
    static RangeDist minimum(std::vector<RangeDist> dists);

    // ranges are inclusive, for ease of use here
    RangeDist map(
        std::vector<int32_t> splits,
        std::vector<size_t> branchIds,
        std::vector<std::function<RangeDist(RangeDist)>> funcs
    ) const;

    RangeDist clipMin(int32_t newMin) const;
    RangeDist clipMax(int32_t newMax) const;

    std::vector<int32_t> percentiles(std::vector<float_t> percentiles) const;
};

#endif // RANGEDIST_H