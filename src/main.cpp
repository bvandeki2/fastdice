#include <iostream>

#include "graph.hpp"
#include "rangedist.hpp"

int main() {

    const int32_t monsterAc = 15;
    const int32_t attackBonus = 5;

    const uint32_t times = 5;

    auto hitReduction = RangeDist::uniform(1, 4).repeat(times);

    auto bonusDamage = hitReduction.repeat(2);
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

    auto percentiles = totalDamage.percentiles({ 0.05f, 0.25f, 0.5f, 0.75f, 0.95f });

    auto minDamage = totalDamage.getMin();
    auto maxDamage = totalDamage.getMax();

    std::cout << "Minimum damage: " << minDamage << std::endl;
    std::cout << "Maximum damage: " << maxDamage << std::endl;
    std::cout << "Damage percentiles:" << std::endl;
    std::cout << "  5th percentile: " << percentiles[0] << std::endl;
    std::cout << " 25th percentile: " << percentiles[1] << std::endl;
    std::cout << " 50th percentile: " << percentiles[2] << std::endl;
    std::cout << " 75th percentile: " << percentiles[3] << std::endl;
    std::cout << " 95th percentile: " << percentiles[4] << std::endl;

    return 0;
}