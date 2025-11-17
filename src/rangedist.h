#ifndef RANGEDIST_H
#define RANGEDIST_H

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

/**
 * @class RangeDist
 * @brief Represents a discrete probability distribution over a range of integers.
 *
 * This class defines a probability distribution where the range of possible
 * outcomes is specified by `min` and `max`. The probabilities for each outcome
 * within the range are stored in the vector `p`.
 *
 */
class PartialRangeDist;

class RangeDist {
    const std::vector<float_t> p;
    const int32_t min;
    const int32_t max;

    // 64 ~ 128 is about where I notice crossover on my machine
    static constexpr size_t FFT_THRESHOLD = 64;

    RangeDist(int32_t min, int32_t max, const std::vector<float_t> &probabilities);

    static bool shouldUseFFT(const RangeDist &lhs, const RangeDist &rhs);
    static size_t nextPowerOfTwo(size_t value);
    static size_t repeatResultSize(size_t baseSize, uint32_t count);
    static RangeDist repeatByAddition(const RangeDist &dist, uint32_t count);

    static RangeDist convolveFFT(const RangeDist &lhs, const RangeDist &rhs);
    static RangeDist convolveNaive(const RangeDist &lhs, const RangeDist &rhs);

    static RangeDist convolvePowerFFT(const RangeDist &dist, uint32_t exponent);

  public:
    static RangeDist uniform(int32_t min, int32_t max);
    static RangeDist literal(int32_t value);

    const std::vector<float_t> &getProbabilities() const;
    int32_t getMin() const;
    int32_t getMax() const;

    RangeDist omit(const std::vector<int32_t> &items) const;

    RangeDist operator-() const;
    RangeDist operator*(int32_t constant) const;
    RangeDist operator+(const RangeDist &other) const;
    RangeDist operator+(int32_t constant) const;
    RangeDist operator-(int32_t constant) const;
    RangeDist operator-(const RangeDist &other) const;

    RangeDist repeat(uint32_t count) const;

    std::vector<int32_t> percentiles(const std::vector<float_t> &sortedPercentiles) const;

    RangeDist map(std::function<int32_t(int32_t)> func) const;
    RangeDist map(std::function<int32_t(int32_t)> func, int32_t newMin, int32_t newMax) const;

    RangeDist map(std::function<RangeDist(int32_t)> func) const;
    // RangeDist map(std::function<RangeDist(int32_t)> func, int32_t newMin, int32_t newMax) const;

    PartialRangeDist partialMap(std::function<std::optional<RangeDist>(int32_t)> func) const;

    friend class PartialRangeDist;
};

// just used for return type of partialMap, users are not expected to construct this directly
class PartialRangeDist {
  private:
    std::vector<std::optional<RangeDist>> parts;
    RangeDist original;

    PartialRangeDist(const RangeDist &original);

  public:
    // Delete copy constructor and copy assignment operator
    PartialRangeDist(const PartialRangeDist &) = delete;
    PartialRangeDist &operator=(const PartialRangeDist &) = delete;

    // Allow move constructor and move assignment operator
    PartialRangeDist(PartialRangeDist &&) noexcept = default;
    PartialRangeDist &operator=(PartialRangeDist &&) noexcept = default;

    PartialRangeDist partialMap(std::function<std::optional<RangeDist>(int32_t)> func);
    RangeDist finalize() const;

    friend class RangeDist;
};

#endif // RANGEDIST_H