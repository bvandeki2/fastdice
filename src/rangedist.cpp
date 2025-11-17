#include "rangedist.h"

#define MEOW_FFT_IMPLEMENTATION
#include "meow_fft.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

/**
 * @brief Computes the power of a complex number raised to an integer exponent.
 *
 * This function calculates the result of raising a complex number (`base`)
 * to a non-negative integer power (`exponent`) using an iterative method
 * based on exponentiation by squaring.
 *
 * @param base The base complex number of type `Meow_FFT_Complex`.
 * @param exponent The non-negative integer exponent.
 * @return A `Meow_FFT_Complex` representing the result of base^exponent.
 *
 * @note The function assumes that the `meow_mul` function is available
 *       to perform complex number multiplication.
 */
Meow_FFT_Complex complexPow(Meow_FFT_Complex base, uint32_t exponent) {
    Meow_FFT_Complex result{1.0f, 0.0f};

    while (exponent > 0) {
        if (exponent & 1U) {
            result = meow_mul(result, base);
        }
        base = meow_mul(base, base);
        exponent >>= 1U;
    }

    return result;
}

bool RangeDist::shouldUseFFT(const RangeDist &lhs, const RangeDist &rhs) {
    return lhs.p.size() >= FFT_THRESHOLD && rhs.p.size() >= FFT_THRESHOLD;
}

size_t RangeDist::nextPowerOfTwo(size_t value) {
    if (value == 0) {
        return 1;
    }

    size_t pow2 = 1;
    while (pow2 < value) {
        pow2 <<= 1;
    }
    return pow2;
}

RangeDist RangeDist::convolveNaive(const RangeDist &lhs, const RangeDist &rhs) {
    int32_t newMin = lhs.min + rhs.min;
    int32_t newMax = lhs.max + rhs.max;
    size_t newSize = static_cast<size_t>(newMax - newMin + 1);

    std::vector<float_t> newProbabilities(newSize, 0.0f);

    for (size_t i = 0; i < lhs.p.size(); ++i) {
        for (size_t j = 0; j < rhs.p.size(); ++j) {
            int32_t outcome = static_cast<int32_t>(i + lhs.min + j + rhs.min);
            newProbabilities[outcome - newMin] += lhs.p[i] * rhs.p[j];
        }
    }

    return RangeDist(newMin, newMax, newProbabilities);
}

RangeDist RangeDist::convolveFFT(const RangeDist &lhs, const RangeDist &rhs) {
    int32_t newMin = lhs.min + rhs.min;
    int32_t newMax = lhs.max + rhs.max;
    size_t resultSize = static_cast<size_t>(newMax - newMin + 1);

    size_t fftSize = nextPowerOfTwo(resultSize);
    if (fftSize < 4) {
        fftSize = 4;
    }
    if (fftSize % 2 != 0) {
        fftSize <<= 1;
    }

    if (fftSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("FFT size exceeds supported range");
    }

    const int fftLength = static_cast<int>(fftSize);

    std::vector<float> lhsPadded(fftSize, 0.0f);
    std::vector<float> rhsPadded(fftSize, 0.0f);

    for (size_t i = 0; i < lhs.p.size(); ++i) {
        lhsPadded[i] = lhs.p[i];
    }
    for (size_t i = 0; i < rhs.p.size(); ++i) {
        rhsPadded[i] = rhs.p[i];
    }

    size_t worksetBytes = meow_fft_generate_workset_real(fftLength, nullptr);
    if (worksetBytes == 0) {
        throw std::runtime_error("Failed to prepare FFT workset");
    }

    std::vector<uint8_t> worksetStorage(worksetBytes);
    auto *workset = reinterpret_cast<Meow_FFT_Workset_Real *>(worksetStorage.data());
    meow_fft_generate_workset_real(fftLength, workset);

    std::vector<Meow_FFT_Complex> lhsFreq(fftSize);
    std::vector<Meow_FFT_Complex> rhsFreq(fftSize);

    meow_fft_real(workset, lhsPadded.data(), lhsFreq.data());
    meow_fft_real(workset, rhsPadded.data(), rhsFreq.data());

    for (size_t i = 0; i < fftSize; ++i) {
        lhsFreq[i] = meow_mul(lhsFreq[i], rhsFreq[i]);
    }

    std::vector<Meow_FFT_Complex> temp(fftSize);
    std::vector<float> convolved(fftSize, 0.0f);
    meow_fft_real_i(workset, lhsFreq.data(), temp.data(), convolved.data());

    const float scale = 1.0f / static_cast<float>(fftSize);
    std::vector<float_t> newProbabilities(resultSize, 0.0f);
    float_t sum = 0.0f;

    for (size_t i = 0; i < resultSize; ++i) {
        float value = convolved[i] * scale;
        if (value < 0.0f) {
            value = 0.0f;
        }
        newProbabilities[i] = static_cast<float_t>(value);
        sum += newProbabilities[i];
    }

    if (sum > 0.0f) {
        for (float_t &prob : newProbabilities) {
            prob /= sum;
        }
    }

    return RangeDist(newMin, newMax, newProbabilities);
}

size_t RangeDist::repeatResultSize(size_t baseSize, uint32_t count) {
    if (baseSize == 0) {
        return 0;
    }

    const size_t span = baseSize - 1;
    const size_t multiplier = static_cast<size_t>(count);

    if (span > 0) {
        const size_t maxMultiplier = (std::numeric_limits<size_t>::max() - 1) / span;
        if (multiplier > maxMultiplier) {
            throw std::overflow_error("repeat result size exceeds supported range");
        }
    }

    return span * multiplier + 1;
}

RangeDist RangeDist::repeatByAddition(const RangeDist &dist, uint32_t count) {
    if (count == 1) {
        return dist;
    }

    if ((count & 1U) == 0U) {
        RangeDist half = repeatByAddition(dist, count / 2);
        return RangeDist::convolveNaive(half, half);
    }

    RangeDist half = repeatByAddition(dist, count / 2);
    RangeDist doubled = half + half;
    return RangeDist::convolveNaive(doubled, dist);
}

RangeDist RangeDist::convolvePowerFFT(const RangeDist &dist, uint32_t exponent) {
    auto computeBounded = [](int32_t value, uint32_t times) -> int32_t {
        const int64_t product = static_cast<int64_t>(value) * static_cast<int64_t>(times);
        if (product < std::numeric_limits<int32_t>::min()
            || product > std::numeric_limits<int32_t>::max()) {
            throw std::overflow_error("repeat range exceeds int32_t bounds");
        }
        return static_cast<int32_t>(product);
    };

    const int32_t newMin = computeBounded(dist.min, exponent);
    const int32_t newMax = computeBounded(dist.max, exponent);

    const size_t resultSize = repeatResultSize(dist.p.size(), exponent);

    size_t fftSize = nextPowerOfTwo(resultSize);
    if (fftSize < 4) {
        fftSize = 4;
    }
    if (fftSize % 2 != 0) {
        fftSize <<= 1;
    }

    if (fftSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("FFT size exceeds supported range");
    }

    const int fftLength = static_cast<int>(fftSize);

    std::vector<float> padded(fftSize, 0.0f);
    for (size_t i = 0; i < dist.p.size(); ++i) {
        padded[i] = dist.p[i];
    }

    size_t worksetBytes = meow_fft_generate_workset_real(fftLength, nullptr);
    if (worksetBytes == 0) {
        throw std::runtime_error("Failed to prepare FFT workset");
    }

    std::vector<uint8_t> worksetStorage(worksetBytes);
    auto *workset = reinterpret_cast<Meow_FFT_Workset_Real *>(worksetStorage.data());
    meow_fft_generate_workset_real(fftLength, workset);

    std::vector<Meow_FFT_Complex> freq(fftSize);
    meow_fft_real(workset, padded.data(), freq.data());

    for (size_t i = 0; i < fftSize; ++i) {
        freq[i] = complexPow(freq[i], exponent);
    }

    std::vector<Meow_FFT_Complex> temp(fftSize);
    std::vector<float_t> powered(fftSize, 0.0f);
    meow_fft_real_i(workset, freq.data(), temp.data(), powered.data());

    const float scale = 1.0f / static_cast<float_t>(fftSize);
    std::vector<float_t> probabilities(resultSize, 0.0f);
    float_t sum = 0.0f;

    for (size_t i = 0; i < resultSize; ++i) {
        float_t value = powered[i] * scale;
        if (value < 0.0f) {
            value = 0.0f;
        }
        probabilities[i] = static_cast<float_t>(value);
        sum += probabilities[i];
    }

    if (sum > 0.0f) {
        for (float_t &prob : probabilities) {
            prob /= sum;
        }
    }

    return RangeDist(newMin, newMax, probabilities);
}

RangeDist::RangeDist(int32_t min, int32_t max, const std::vector<float_t> &probabilities)
    : min(min), max(max), p(probabilities) {
    if (min > max) {
        throw std::invalid_argument("min must be less than or equal to max.");
    }
    if (probabilities.size() != static_cast<size_t>(max - min + 1)) {
        throw std::invalid_argument("Size of probabilities vector must match the range.");
    }
    float_t sum = 0.0f;
    for (float_t prob : probabilities) {
        sum += prob;
    }
    if (std::abs(sum - 1.0f) > 1e-4) {
        throw std::invalid_argument("Sum of probabilities must be close to 1.");
    }
}

RangeDist RangeDist::uniform(int32_t min, int32_t max) {
    if (min > max) {
        throw std::invalid_argument("min must be less than or equal to max.");
    }

    size_t size = static_cast<size_t>(max - min + 1);
    std::vector<float_t> probabilities(size, 1.0f / size);
    return RangeDist(min, max, probabilities);
}

RangeDist RangeDist::literal(int32_t value) {
    return RangeDist(value, value, std::vector<float_t>{1.0f});
}

const std::vector<float_t> &RangeDist::getProbabilities() const { return p; }
int32_t RangeDist::getMin() const { return min; }
int32_t RangeDist::getMax() const { return max; }

RangeDist RangeDist::omit(const std::vector<int32_t> &items) const {
    std::vector<float_t> newProbabilities = this->p;
    float_t totalOmittedProb = 0.0f;

    for (int32_t item : items) {
        if (item < this->min || item > this->max) {
            throw std::out_of_range("Item to omit is out of the distribution range.");
        }
        size_t index = static_cast<size_t>(item - this->min);
        totalOmittedProb += newProbabilities[index];
        newProbabilities[index] = 0.0f;
    }

    float_t factor = 1.0f - totalOmittedProb;

    if (factor < 1e-7f) {
        throw std::invalid_argument("Omitting these items would eliminate all probability mass.");
    }

    for (float_t &prob : newProbabilities) {
        prob /= factor;
    }

    return RangeDist(this->min, this->max, newProbabilities);
}

RangeDist RangeDist::operator-() const {
    int32_t newMin = -this->max;
    int32_t newMax = -this->min;
    std::vector<float_t> newProbabilities(this->p.rbegin(), this->p.rend());
    return RangeDist(newMin, newMax, newProbabilities);
}

RangeDist RangeDist::operator*(int32_t constant) const {
    if (constant == 0) {
        return RangeDist::uniform(0, 0);
    }

    int32_t newMin = (constant > 0) ? this->min * constant : this->max * constant;
    int32_t newMax = (constant > 0) ? this->max * constant : this->min * constant;

    std::vector<float_t> newProbabilities(static_cast<size_t>(newMax - newMin + 1), 0.0f);

    for (size_t i = 0; i < this->p.size(); ++i) {
        int32_t outcome = static_cast<int32_t>(i + this->min) * constant;
        newProbabilities[outcome - newMin] = this->p[i];
    }

    return RangeDist(newMin, newMax, newProbabilities);
}

RangeDist RangeDist::operator+(const RangeDist &other) const {
    if (shouldUseFFT(*this, other)) {
        return convolveFFT(*this, other);
    }

    return convolveNaive(*this, other);
}

RangeDist RangeDist::operator+(int32_t constant) const {
    int32_t newMin = this->min + constant;
    int32_t newMax = this->max + constant;
    return RangeDist(newMin, newMax, this->p);
}

RangeDist RangeDist::operator-(int32_t constant) const { return (*this) + (-constant); }

RangeDist RangeDist::operator-(const RangeDist &other) const { return (*this) + (-other); }

RangeDist RangeDist::repeat(uint32_t count) const {
    if (count == 0) {
        throw std::invalid_argument("repeat count must be positive");
    }

    if (count == 1) {
        return *this;
    }

    const size_t resultSize = repeatResultSize(this->p.size(), count);

    if (resultSize < FFT_THRESHOLD) {
        return repeatByAddition(*this, count);
    }

    return convolvePowerFFT(*this, count);
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
std::vector<int32_t> RangeDist::percentiles(const std::vector<float_t> &percentiles) const {
    std::vector<float_t> sortedPercentiles = percentiles;
    std::sort(sortedPercentiles.begin(), sortedPercentiles.end());

    // we don't worry about precision in most places, but with CDF calculations, something like
    // 0.999f could realistically be used. So we "normalize" to avoid precision issues, adding
    // in the *same order* we'll use to compute the CDF...
    float_t total = 0.0f;
    for (float_t p_value : this->p) {
        total += p_value;
    }
    // new "max percentile" is total, i.e. 1.0 -> total
    for (float_t &perc : sortedPercentiles) {
        perc *= total;
    }

    std::vector<int32_t> results;
    results.reserve(sortedPercentiles.size());

    float_t accumulatedCdf = 0.0f;
    size_t percentileIndex = 0;

    for (size_t i = 0; i < p.size() && percentileIndex < percentiles.size(); ++i) {
        accumulatedCdf += p[i];
        while (percentileIndex < percentiles.size()
               && accumulatedCdf >= percentiles[percentileIndex]) {
            results.push_back(static_cast<int32_t>(i + min));
            ++percentileIndex;
        }
    }

    return results;
}

RangeDist RangeDist::map(std::function<int32_t(int32_t)> func) const {
    std::vector<std::pair<int32_t, float_t>> mappedOutcomes(this->p.size(), {0, 0.0f});

    for (size_t i = 0; i < this->p.size(); ++i) {
        int32_t originalOutcome = static_cast<int32_t>(i + this->min);
        int32_t newOutcome = func(originalOutcome);
        mappedOutcomes[i] = {newOutcome, this->p[i]};
    }

    int32_t newMin = std::numeric_limits<int32_t>::max();
    int32_t newMax = std::numeric_limits<int32_t>::min();

    for (const auto &[outcome, prob] : mappedOutcomes) {
        if (outcome < newMin)
            newMin = outcome;
        if (outcome > newMax)
            newMax = outcome;
    }

    std::vector<float_t> newProbabilities(static_cast<size_t>(newMax - newMin + 1), 0.0f);
    for (const auto &[outcome, prob] : mappedOutcomes) {
        int32_t j = outcome - newMin;
        newProbabilities[static_cast<size_t>(j)] += prob;
    }

    return RangeDist(newMin, newMax, newProbabilities);
}

RangeDist RangeDist::map(std::function<int32_t(int32_t)> func, int32_t newMin,
                         int32_t newMax) const {
    std::vector<float_t> newProbabilities(static_cast<size_t>(newMax - newMin + 1), 0.0f);

    for (size_t i = 0; i < this->p.size(); ++i) {
        int32_t originalOutcome = static_cast<int32_t>(i + this->min);
        int32_t newOutcome = func(originalOutcome);
        int32_t j = newOutcome - newMin;
        if (j < 0 || j >= static_cast<int32_t>(newProbabilities.size())) {
            throw std::out_of_range("Mapped outcome is out of the specified new range.");
        }
        newProbabilities[static_cast<size_t>(j)] += this->p[i];
    }

    return RangeDist(newMin, newMax, newProbabilities);
}

RangeDist RangeDist::map(std::function<RangeDist(int32_t)> func) const {
    std::vector<std::pair<RangeDist, float_t>> mappedOutcomes;
    mappedOutcomes.reserve(this->p.size());

    for (size_t i = 0; i < this->p.size(); ++i) {
        int32_t originalOutcome = static_cast<int32_t>(i + this->min);
        RangeDist mappedDist = func(originalOutcome);
        mappedOutcomes.emplace_back(mappedDist, this->p[i]);
    }

    int32_t newMin = std::numeric_limits<int32_t>::max();
    int32_t newMax = std::numeric_limits<int32_t>::min();

    for (const auto &[outcome, prob] : mappedOutcomes) {
        if (outcome.min < newMin)
            newMin = outcome.min;
        if (outcome.max > newMax)
            newMax = outcome.max;
    }

    std::vector<float_t> newProbabilities(static_cast<size_t>(newMax - newMin + 1), 0.0f);
    for (const auto &[outcome, prob] : mappedOutcomes) {
        for (size_t j = 0; j < outcome.p.size(); ++j) {
            int32_t mappedOutcome = static_cast<int32_t>(j + outcome.min);
            int32_t k = mappedOutcome - newMin;
            newProbabilities[static_cast<size_t>(k)] += outcome.p[j] * prob;
        }
    }

    return RangeDist(newMin, newMax, newProbabilities);
}

/**
 * Applies a mapping function to the outcomes of the original distribution and updates
 * the `parts` array with the resulting `RangeDist` objects.
 *
 * @param func A function that takes an `int32_t` outcome as input and returns a
 *             `std::unique_ptr<RangeDist>` representing the mapped distribution for that outcome.
 *             If the function returns `nullptr`, the corresponding part is not updated.
 *
 * @return A reference to the current `PartialRangeDist` object after applying the mapping function.
 *
 * @details This method iterates over the probabilities of the original distribution. For each
 *          outcome that does not already have a corresponding `RangeDist` in the `parts` array,
 *          the provided mapping function is called. If the function returns a non-null pointer,
 *          the resulting `RangeDist` is stored in the `parts` array at the corresponding index.
 */
PartialRangeDist
RangeDist::partialMap(std::function<std::optional<RangeDist>(int32_t)> func) const {
    return PartialRangeDist(*this).partialMap(func);
}

PartialRangeDist::PartialRangeDist(const RangeDist &original)
    : original(original), parts(original.getProbabilities().size()) {}

/**
 * Applies a mapping function to the outcomes of the original distribution and updates
 * the `parts` array with the resulting `RangeDist` objects.
 *
 * @param func A function that takes an `int32_t` outcome as input and returns a
 *             `std::unique_ptr<RangeDist>` representing the mapped distribution for that outcome.
 *             If the function returns `nullptr`, the corresponding part is not updated.
 *
 * @return A reference to the current `PartialRangeDist` object after applying the mapping function.
 *
 * @details This method iterates over the probabilities of the original distribution. For each
 *          outcome that does not already have a corresponding `RangeDist` in the `parts` array,
 *          the provided mapping function is called. If the function returns a non-null pointer,
 *          the resulting `RangeDist` is stored in the `parts` array at the corresponding index.
 */
PartialRangeDist
PartialRangeDist::partialMap(std::function<std::optional<RangeDist>(int32_t)> func) {
    for (auto &part : parts) {
        if (part) {
            continue;
        }
        size_t index = &part - &parts[0];
        int32_t outcome = static_cast<int32_t>(index + original.min);
        std::optional<RangeDist> mappedDist = func(outcome);
        if (mappedDist) {
            part.emplace(std::move(*mappedDist));
        }
    }
    return std::move(*this);
}

RangeDist PartialRangeDist::finalize() const {
    int32_t newMin = std::numeric_limits<int32_t>::max();
    int32_t newMax = std::numeric_limits<int32_t>::min();

    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] == std::nullopt) {
            throw std::runtime_error(
                "Not all parts have been mapped; cannot finalize PartialRangeDist.");
        }

        const RangeDist &partDist = *parts[i];
        if (partDist.min < newMin)
            newMin = partDist.min;
        if (partDist.max > newMax)
            newMax = partDist.max;
    }

    std::vector<float_t> newProbabilities(static_cast<size_t>(newMax - newMin + 1), 0.0f);

    for (size_t i = 0; i < parts.size(); ++i) {
        float_t originalProb = original.p[i];
        const RangeDist &partDist = *parts[i];
        for (size_t j = 0; j < partDist.p.size(); ++j) {
            int32_t mappedOutcome = static_cast<int32_t>(j + partDist.getMin());
            int32_t k = mappedOutcome - newMin;
            newProbabilities[static_cast<size_t>(k)] += partDist.p[j] * originalProb;
        }
    }

    return RangeDist(newMin, newMax, newProbabilities);
}