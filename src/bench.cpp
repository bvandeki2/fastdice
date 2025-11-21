#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

#include "rangedist.hpp"

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

        // with advantage, roll 2d20
        auto d20 = RangeDist::uniform(1, 20);
        auto toHit = d20.maximum({ d20, d20 });

        auto totalDamage = toHit.map(
            // 20 "splits" the domain into two parts: [-inf, 20) and [20, inf).
            // mapped onto our domain, that's [1,19] and [20]
            // the 0, 1 indicates the first span is branch 0, the second is branch 1
            // this makes it straightforward to partition arbitrarily large domains
            // though it is a little hard to read as it gets nested/complicated. Probably
            // use a language binding for anything complex.
            { 20 },
            { 0, 1 },
            { [&](RangeDist attackRoll) {
                 auto modifiedRoll = attackRoll - hitReduction;
                 return modifiedRoll.map(
                     // again, partition into miss/hit.
                     // note that we were able to take the RangeDist conditional on *not* being a
                     // crit and reuse it here. `map` is reasonably smart about not computing more
                     // than needed
                     { monsterAc },
                     { 0, 1 }, // miss, hit
                     {
                         [&](RangeDist roll) {
                             return 0; /* miss */
                         },
                         [&](RangeDist roll) { return baseDamageAug; },
                     }
                 );
             },
              [&](RangeDist attackRoll) { return critDamageAug; } }
        );

        volatile auto percentiles = totalDamage.percentiles({ 0.05f, 0.25f, 0.5f, 0.75f, 0.95f });
    });
    std::cout << "Average time for d4 mod (1d20 - 5d4, 10d100 + 10d4, optimized): "
              << d4ModReducedNestTime << " microseconds." << std::endl;

    auto thousandD100Time = measure([&]() {
        auto thousandD100 = RangeDist::uniform(1, 100).repeat(1000);
        volatile auto percentiles = thousandD100.percentiles({ 0.05f, 0.25f, 0.5f, 0.75f, 0.95f });
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