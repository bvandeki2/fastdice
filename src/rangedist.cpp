#include "rangedist.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

RangeDist RangeDist::uniform(int32_t min, int32_t max) {
    auto distOp = std::make_shared<fastdice::graph::Uniform>(min, max);
    return RangeDist(std::move(distOp));
}

RangeDist::RangeDist(std::shared_ptr<fastdice::graph::DistOp> distOp)
    : distOp(std::move(distOp)) {
    this->distOp->calcProbabilities(); // eager evaluation
}

RangeDist::RangeDist(int32_t value)
    : distOp(std::make_shared<fastdice::graph::Uniform>(value, value)) {
    distOp->calcProbabilities(); // eager evaluation
}

const std::vector<float_t>& RangeDist::calcProbabilities() const {
    return distOp->calcProbabilities();
}
int32_t RangeDist::getMin() const { return distOp->getMin(); }
int32_t RangeDist::getMax() const { return distOp->getMax(); }

RangeDist RangeDist::operator-() const {
    auto negOp = std::make_shared<fastdice::graph::Negate>(distOp);
    return RangeDist(std::move(negOp));
}

RangeDist RangeDist::operator*(const RangeDist& other) const {
    auto mulOp = std::make_shared<fastdice::graph::Multiply>(distOp, other.distOp);
    return RangeDist(std::move(mulOp));
}

RangeDist RangeDist::operator+(const RangeDist& other) const {
    auto addOp = std::make_shared<fastdice::graph::Add>(distOp, other.distOp);
    return RangeDist(std::move(addOp));
}

RangeDist RangeDist::operator-(const RangeDist& other) const { return (*this) + (-other); }

RangeDist RangeDist::repeat(uint32_t count) const {
    auto repeatOp = std::make_shared<fastdice::graph::Repeat>(distOp, static_cast<size_t>(count));
    return RangeDist(std::move(repeatOp));
}

RangeDist RangeDist::maximum(std::vector<RangeDist> dists) {
    std::vector<fastdice::graph::DistOpPtr> distOps;
    distOps.reserve(dists.size());
    for (const auto& dist : dists) {
        distOps.push_back(dist.distOp);
    }
    auto maxOp = std::make_shared<fastdice::graph::Keep>(
        std::move(distOps), dists.size() - 1, dists.size()
    );
    return RangeDist(std::move(maxOp));
}

RangeDist RangeDist::minimum(std::vector<RangeDist> dists) {
    std::vector<fastdice::graph::DistOpPtr> distOps;
    distOps.reserve(dists.size());
    for (const auto& dist : dists) {
        distOps.push_back(dist.distOp);
    }
    auto minOp = std::make_shared<fastdice::graph::Keep>(std::move(distOps), 0, 1);
    return RangeDist(std::move(minOp));
}
RangeDist RangeDist::map(
    std::vector<int32_t> splits,
    std::vector<size_t> branchIds,
    std::vector<std::function<RangeDist(RangeDist)>> funcs
) const {
    auto partitionOp = std::make_shared<fastdice::graph::Partition>(
        distOp, std::move(splits), std::move(branchIds)
    );
    partitionOp->calcProbabilities();

    // now, for each function, create a BranchInput from the above and apply the function
    std::vector<fastdice::graph::DistOpPtr> branchOutputs;
    branchOutputs.reserve(funcs.size());

    for (size_t funcIdx = 0; funcIdx < funcs.size(); ++funcIdx) {
        auto branchInputOp = std::make_shared<fastdice::graph::BranchInput>(partitionOp, funcIdx);
        RangeDist branchInputDist(std::move(branchInputOp));

        RangeDist branchOutputDist = funcs[funcIdx](branchInputDist);

        branchOutputs.push_back(branchOutputDist.distOp);
    }

    // finally, merge the branches back together
    auto mergeOp = std::make_shared<fastdice::graph::Merge>(partitionOp, std::move(branchOutputs));

    return RangeDist(std::move(mergeOp));
}
RangeDist RangeDist::clipMin(int32_t newMin) const {
    // we can piggyback off of partitioning, should be same time complexity
    std::vector<int32_t> splits = { newMin };
    std::vector<size_t> branchIds = { 0, 1 }; // below, above
    auto partitionOp = std::make_shared<fastdice::graph::Partition>(
        distOp, std::move(splits), std::move(branchIds)
    );

    auto newMinOp = std::make_shared<fastdice::graph::Uniform>(newMin, newMin);
    auto aboveInputOp = std::make_shared<fastdice::graph::BranchInput>(partitionOp, 1);
    std::vector<fastdice::graph::DistOpPtr> branches = { newMinOp, aboveInputOp };

    auto mergeOp = std::make_shared<fastdice::graph::Merge>(partitionOp, std::move(branches));

    return RangeDist(std::move(mergeOp));
}

RangeDist RangeDist::clipMax(int32_t newMax) const {
    // we can piggyback off of partitioning, should be same time complexity
    std::vector<int32_t> splits = { newMax + 1 };
    std::vector<size_t> branchIds = { 0, 1 }; // below, above
    auto partitionOp = std::make_shared<fastdice::graph::Partition>(
        distOp, std::move(splits), std::move(branchIds)
    );

    auto belowInputOp = std::make_shared<fastdice::graph::BranchInput>(partitionOp, 0);
    auto newMaxOp = std::make_shared<fastdice::graph::Uniform>(newMax, newMax);
    std::vector<fastdice::graph::DistOpPtr> branches = { belowInputOp, newMaxOp };

    auto mergeOp = std::make_shared<fastdice::graph::Merge>(partitionOp, std::move(branches));

    return RangeDist(std::move(mergeOp));
}

/**
 * @brief Computes the indices corresponding to the given sorted percentiles.
 *
 * This function takes a vector of sorted percentiles (values between 0.0 and 1.0)
 * and calculates the indices in the underlying data distribution that correspond
 * to these percentiles. The input percentiles must be sorted in ascending order.
 *
 * @param sortedPercentiles A vector of floating-point values representing the
 *        percentiles, sorted in ascending order. Each value should be in the
 *        range [0.0, 1.0].
 * @return A vector of 32-bit integers representing the indices corresponding to
 *         the input percentiles.
 */
std::vector<int32_t> RangeDist::percentiles(std::vector<float_t> percentiles) const {
    std::sort(percentiles.begin(), percentiles.end());

    auto probs = this->calcProbabilities();

    // we don't worry about precision in most places, but with CDF calculations, something like
    // 0.999f could realistically be used. So we "normalize" to avoid precision issues, adding
    // in the *same order* we'll use to compute the CDF...
    float_t total = 0.0f;
    for (float_t p_value : probs) {
        total += p_value;
    }
    // new "max percentile" is total, i.e. 1.0 -> total
    for (float_t& perc : percentiles) {
        perc *= total;
    }

    std::vector<int32_t> results;
    results.reserve(percentiles.size());

    float_t accumulatedCdf = 0.0f;
    size_t percentileIndex = 0;

    for (size_t i = 0; i < probs.size() && percentileIndex < percentiles.size(); ++i) {
        accumulatedCdf += probs[i];
        while (percentileIndex < percentiles.size()
               && accumulatedCdf >= percentiles[percentileIndex]) {
            results.push_back(static_cast<int32_t>(i + this->getMin()));
            ++percentileIndex;
        }
    }

    return results;
}