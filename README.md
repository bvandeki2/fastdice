# FastDice

direct (and hopefully speedy) calculation of dice expression.

big dice expressions can use FFT (using [meow_fft](https://github.com/JodiTheTigger/meow_fft) for a hopefully easy WASM port 🤞).

you can do stuff like

```cpp
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

auto totalDamage = RangeDist::uniform(1, 20).map([&](int32_t d20roll) {
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
```

the above runs in roughly ~400us on my machine (5700x3d, 2x32GB 3200MHz DDR4).

## Building

### Native Build

pick your favorite flavor of C++ 17 and beyond...

```sh
cmake --build build
./build/Debug/FastDice

# release helps ~10x, anecdotally
cmake --build build --config Release
./build/Release/Bench
```

### WebAssembly Build

Targeting web assembly

#### Prerequisites

Install the Emscripten SDK:

```sh
# Clone the emsdk repository
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate the latest SDK
./emsdk install latest
./emsdk activate latest

# Activate PATH and other environment variables in the current terminal (Unix/Mac)
source ./emsdk_env.sh

# On Windows, use:
# .\emsdk_env.ps1
```

#### Build for WebAssembly

**On Unix/Linux/Mac:**
```sh
./build-wasm.sh
```

**On Windows:**
```powershell
.\build-wasm.ps1
```

#### Run the Demo

After building, serve the web directory:

```sh
cd web
python3 -m http.server 8000
# or
npx http-server -p 8000
```

Then open http://localhost:8000 in your browser to see the interactive demo!

The WebAssembly build provides a JavaScript API to:
- Roll multiple dice (e.g., 3d6, 2d20)
- Combine distributions (addition, subtraction, multiplication)
- Calculate percentiles and probability distributions
- Build complex dice expressions programmatically

50,000d100 runs in ~5980ms using the web/ calculate.

## Future Directions
- Graph Compilation (lots of low hanging fruit optimization-wise)
- `double` precision
- SIMD (webassembly too)
- Dice DSL (lua?)
- ~~WASM port~~ ✓ (done!)