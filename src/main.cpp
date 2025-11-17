#include <iostream>

#include "rangedist.h"

int main() {
    // -5d4 to hit
    auto hitReduction = RangeDist::uniform(1, 4).repeat(5);

    // but +10d4 to damage on a hit!
    auto bonusDamage = RangeDist::uniform(1, 4).repeat(10);

    // stupid damage to test how this program scales
    auto baseDamage = RangeDist::uniform(1, 100).repeat(10);
    auto critDamage = baseDamage + baseDamage; // roll damage again on a crit

    // apply bonus damage to each type of damage roll
    auto baseDamageAug = baseDamage + bonusDamage;
    auto critDamageAug = critDamage + bonusDamage;

    const int32_t monsterAc = 15;
    const int32_t attackBonus = 5;

    auto d20 = RangeDist::uniform(1, 20);
    auto d20adv = d20.maximum(d20);

    auto totalDamage = d20adv.map([&](int32_t d20roll) {
        if (d20roll == 20) {
            return critDamageAug;
        } else {
            // silly order just to get RangeDist first, this is just a normal attack
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

    auto percentiles = totalDamage.percentiles({0.05f, 0.25f, 0.5f, 0.75f, 0.95f});

    std::cout << "Damage percentiles:" << std::endl;
    std::cout << "  5th percentile: " << percentiles[0] << std::endl;
    std::cout << " 25th percentile: " << percentiles[1] << std::endl;
    std::cout << " 50th percentile: " << percentiles[2] << std::endl;
    std::cout << " 75th percentile: " << percentiles[3] << std::endl;
    std::cout << " 95th percentile: " << percentiles[4] << std::endl;

    return 0;
}