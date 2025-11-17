#include <iostream>

#include "rangedist.h"
#include "wren.hpp"

void writeFn(WrenVM *vm, const char *text) { std::cout << text; }

void mathAdd(WrenVM *vm) {
    const char *str = wrenGetSlotString(vm, 1);
    std::cout << "FFI received string: " << str << std::endl;
}

WrenForeignMethodFn bindForeignMethodFn(WrenVM *vm, const char *module, const char *className,
                                        bool isStatic, const char *signature) {

    // this catches *any* foreign method defined in Wren. (might want to later change this to an
    // error?)
    return mathAdd;
}

int main() {
    WrenConfiguration config;
    wrenInitConfiguration(&config);
    config.writeFn = writeFn;
    config.bindForeignMethodFn = bindForeignMethodFn;

    WrenVM *vm = wrenNewVM(&config);
    wrenEnsureSlots(vm, 2);

    const char *script = R"_WREN(
System.print("Hello from Wren!")
class FFIExample {
    foreign static test(value)
}

FFIExample.test("Calling FFI from Wren!")

)_WREN";

    WrenInterpretResult result = wrenInterpret(vm, "main", script);
    if (result != WREN_RESULT_SUCCESS) {
        std::cerr << "Failed to execute Wren script." << std::endl;
    }

    wrenFreeVM(vm);

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

    auto totalDamage = (RangeDist::uniform(1, 20)
                            .partialMap([&](int32_t d20roll) {
                                if (d20roll == 20) {
                                    return std::optional(critDamageAug);
                                }
                                return std::optional<RangeDist>();
                            })
                            .partialMap([&](int32_t d20roll) {
                                auto toHit = (-hitReduction) + d20roll + attackBonus;

                                return toHit.map([&](int32_t attackRoll) {
                                    if (attackRoll >= monsterAc) {
                                        // hit!
                                        return baseDamageAug;
                                    }
                                    return RangeDist::uniform(0, 0); // no damage on miss
                                });
                            })
                            .finalize());

    auto percentiles = totalDamage.percentiles({0.05f, 0.25f, 0.5f, 0.75f, 0.95f});

    std::cout << "Damage percentiles:" << std::endl;
    std::cout << "  5th percentile: " << percentiles[0] << std::endl;
    std::cout << " 25th percentile: " << percentiles[1] << std::endl;
    std::cout << " 50th percentile: " << percentiles[2] << std::endl;
    std::cout << " 75th percentile: " << percentiles[3] << std::endl;
    std::cout << " 95th percentile: " << percentiles[4] << std::endl;

    return 0;
}