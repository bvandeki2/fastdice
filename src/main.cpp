#include <iostream>

#include "graph.hpp"
#include "rangedist.hpp"

int main() {

    // -5d4 to hit
    auto v = RangeDist::uniform(1, 20);

    auto adv = v.maximum({ v, v });

    auto times = 1;
    auto toHitDebuff = RangeDist::uniform(1, 4).repeat(times);
    auto damageBuff = toHitDebuff.repeat(2);

    auto baseDamage = RangeDist::uniform(1, 8).repeat(2);
    auto bonusDamage = 5;

    auto damage = adv.map(
        { 20 },
        { 0, 1 }, // normal, crit
        { [&](RangeDist attackRoll) {
             // not an auto, so the debuff applies
             auto adjustedRoll = (attackRoll - toHitDebuff).clipMin(1);

             // assume AC 15 for this example
             auto damageInclMiss = adjustedRoll.map(
                 { 15 },
                 { 0, 1 }, // hit, miss
                 {
                     [&](RangeDist roll) {
                         return 0; /* miss */
                     },
                     [&](RangeDist roll) { return baseDamage + damageBuff + bonusDamage; },
                 }
             );

             return damageInclMiss;
         },
          [&](RangeDist attackRoll) {
              // critical hit: double damage dice
              auto critDamage = (baseDamage.repeat(2)) + damageBuff + bonusDamage;
              return critDamage;
          } }
    );

    auto percentiles = damage.percentiles({ 0.05f, 0.25f, 0.5f, 0.75f, 0.95f });

    auto minDamage = damage.getMin();
    auto maxDamage = damage.getMax();

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