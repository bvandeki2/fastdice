#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

#include "rangedist.h"

void benchmarkRangeDist() {
    using namespace std::chrono;

    constexpr int iterations = 10;

    auto measure = [iterations](auto func) {
        std::vector<long long> times;
        for (int i = 0; i < iterations; ++i) {
            auto start = high_resolution_clock::now();
            func();
            auto end = high_resolution_clock::now();
            times.push_back(duration_cast<microseconds>(end - start).count());
        }
        return std::accumulate(times.begin(), times.end(), 0LL) / iterations;
    };

    // Warmup
    RangeDist::uniform(1, 6);

    // Benchmark uniform distribution creation
    auto uniformTime = measure([]() { RangeDist::uniform(1, 6); });
    std::cout << "Average time for uniform distribution creation: " << uniformTime
              << " microseconds." << std::endl;

    const int32_t monsterAc = 15;
    const int32_t attackBonus = 5;

    auto d4ModReducedNestTime = measure([&]() {
        auto hitReduction = RangeDist::uniform(1, 4).repeat(5);

        auto bonusDamage = RangeDist::uniform(1, 4).repeat(10);
        auto baseDamage = RangeDist::uniform(1, 100).repeat(10);
        auto critDamage = baseDamage + baseDamage;

        // apply bonus damage to each type of damage roll
        auto baseDamageAug = baseDamage + bonusDamage;
        auto critDamageAug = critDamage + bonusDamage;

        // need to find a more ergonomic way to express this
        auto isCrit =
            RangeDist::uniform(1, 20).map([&](int32_t roll) { return roll == 20 ? 1 : 0; });
        auto nonCritAttackRoll = RangeDist::uniform(1, 19); // i.e. outcome GIVEN we did not crit
        auto toHit = nonCritAttackRoll + attackBonus - hitReduction; // still conditional on no crit

        // .map is slow; there are two outcomes, hit or miss.
        // would prefer reduction to 0,1 RangeDist and *then* operate on that.
        // or do fancy graph of operations analysis to optimize away the map
        auto nonCritDamageInclMiss = toHit.map([&](int32_t attackRoll) {
            if (attackRoll >= monsterAc) {
                // hit!
                return baseDamageAug;
            }
            return RangeDist::uniform(0, 0); // no damage on miss
        });

        auto totalDamage = isCrit.map([&](int32_t crit) {
            // crit is boolean so this is more of an if-else than a map in terms of
            // time complexity
            if (crit)
                return critDamageAug;
            return nonCritDamageInclMiss;
        });

        volatile auto percentiles = totalDamage.percentiles({0.05f, 0.25f, 0.5f, 0.75f, 0.95f});
    });
    std::cout << "Average time for d4 mod (1d20 - 5d4, 10d100 + 10d4, optimized): "
              << d4ModReducedNestTime << " microseconds." << std::endl;

    auto d4ModNestTime = measure([&]() {
        auto hitReduction = RangeDist::uniform(1, 4).repeat(5);

        auto bonusDamage = RangeDist::uniform(1, 4).repeat(10);
        auto baseDamage = RangeDist::uniform(1, 100).repeat(10);
        auto critDamage = baseDamage + baseDamage;

        // apply bonus damage to each type of damage roll
        auto baseDamageAug = baseDamage + bonusDamage;
        auto critDamageAug = critDamage + bonusDamage;

        auto totalDamage = RangeDist::uniform(1, 20).map([&](int32_t d20roll) {
            if (d20roll == 20) {
                return critDamageAug;
            } else {
                // fucky order just to get RangeDist first, this is just a normal attack
                // roll
                auto toHit = (-hitReduction) + d20roll + attackBonus;

                return toHit.map([&](int32_t attackRoll) {
                    if (attackRoll >= monsterAc) {
                        // hit!
                        return baseDamageAug;
                    }
                    return RangeDist::uniform(0, 0); // no damage on miss
                });
            }
        });

        volatile auto percentiles = totalDamage.percentiles({0.05f, 0.25f, 0.5f, 0.75f, 0.95f});
    });
    std::cout << "Average time for d4 mod (1d20 - 5d4, 10d100 + 10d4, nested maps): "
              << d4ModNestTime << " microseconds." << std::endl;

    auto thousandD100Time = measure([&]() {
        auto thousandD100 = RangeDist::uniform(1, 100).repeat(1000);
        volatile auto percentiles = thousandD100.percentiles({0.05f, 0.25f, 0.5f, 0.75f, 0.95f});
    });
    std::cout << "Average time for 1000d100 materialization: " << thousandD100Time
              << " microseconds." << std::endl;
}

int main() {
    std::cout << "Starting RangeDist benchmarks..." << std::endl;
    benchmarkRangeDist();
    std::cout << "Benchmarks completed." << std::endl;
    return 0;
}