#include "rangedist.h"
#include <emscripten/emscripten.h>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {

/**
 * Creates a uniform distribution dice (e.g., 1d6, 1d20).
 * Returns a pointer to the RangeDist object that must be freed with destroyRangeDist.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* createUniformDist(int32_t min, int32_t max) {
    return new RangeDist(RangeDist::uniform(min, max));
}

/**
 * Destroys a RangeDist object created by any of the create functions.
 */
EMSCRIPTEN_KEEPALIVE
void destroyRangeDist(RangeDist* dist) {
    delete dist;
}

/**
 * Repeats a distribution (e.g., rolling multiple dice).
 * Returns a new RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* repeatDist(RangeDist* dist, uint32_t count) {
    return new RangeDist(dist->repeat(count));
}

/**
 * Adds two distributions together.
 * Returns a new RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* addDists(RangeDist* lhs, RangeDist* rhs) {
    return new RangeDist(*lhs + *rhs);
}

/**
 * Adds a constant to a distribution.
 * Returns a new RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* addConstant(RangeDist* dist, int32_t constant) {
    return new RangeDist(*dist + constant);
}

/**
 * Subtracts a constant from a distribution.
 * Returns a new RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* subtractConstant(RangeDist* dist, int32_t constant) {
    return new RangeDist(*dist - constant);
}

/**
 * Multiplies a distribution by a constant.
 * Returns a new RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* multiplyConstant(RangeDist* dist, int32_t constant) {
    return new RangeDist(*dist * constant);
}

/**
 * Negates a distribution.
 * Returns a new RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* negateDist(RangeDist* dist) {
    return new RangeDist(-(*dist));
}

/**
 * Gets the minimum value of the distribution.
 */
EMSCRIPTEN_KEEPALIVE
int32_t getMin(RangeDist* dist) {
    return dist->getMin();
}

/**
 * Gets the maximum value of the distribution.
 */
EMSCRIPTEN_KEEPALIVE
int32_t getMax(RangeDist* dist) {
    return dist->getMax();
}

/**
 * Gets the size of the probability array.
 */
EMSCRIPTEN_KEEPALIVE
int32_t getProbabilityCount(RangeDist* dist) {
    return static_cast<int32_t>(dist->getProbabilities().size());
}

/**
 * Copies the probability array to a provided buffer.
 * buffer must be at least getProbabilityCount() floats in size.
 */
EMSCRIPTEN_KEEPALIVE
void getProbabilities(RangeDist* dist, float* buffer) {
    const auto& probs = dist->getProbabilities();
    std::memcpy(buffer, probs.data(), probs.size() * sizeof(float));
}

/**
 * Calculates a single percentile value.
 * percentile should be between 0.0 and 1.0 (e.g., 0.5 for median).
 */
EMSCRIPTEN_KEEPALIVE
int32_t getPercentile(RangeDist* dist, float percentile) {
    std::vector<float> p = {percentile};
    auto results = dist->percentiles(p);
    return results[0];
}

/**
 * Calculates multiple percentiles.
 * percentiles_buffer contains the percentile values (0.0 to 1.0).
 * results_buffer will be filled with the calculated percentile values.
 * Both buffers must be at least count elements in size.
 */
EMSCRIPTEN_KEEPALIVE
void getPercentiles(RangeDist* dist, float* percentiles_buffer, int32_t count, int32_t* results_buffer) {
    std::vector<float> percentiles(percentiles_buffer, percentiles_buffer + count);
    auto results = dist->percentiles(percentiles);
    std::memcpy(results_buffer, results.data(), results.size() * sizeof(int32_t));
}

/**
 * Simple helper function to roll NdM dice (e.g., 3d6, 2d20).
 * Returns a RangeDist that must be freed.
 */
EMSCRIPTEN_KEEPALIVE
RangeDist* rollDice(int32_t count, int32_t sides) {
    return new RangeDist(RangeDist::uniform(1, sides).repeat(count));
}

} // extern "C"
