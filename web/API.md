# FastDice WebAssembly API Reference

This document describes the JavaScript API for FastDice when compiled to WebAssembly.

## Loading the Module

```javascript
createFastDiceModule().then(function(Module) {
    // Module is ready to use
    // Wrap C functions for easier calling
    const api = {
        createUniformDist: Module.cwrap('createUniformDist', 'number', ['number', 'number']),
        destroyRangeDist: Module.cwrap('destroyRangeDist', null, ['number']),
        rollDice: Module.cwrap('rollDice', 'number', ['number', 'number']),
        // ... other functions
    };
});
```

## Core Functions

### Creating Distributions

#### `createUniformDist(min, max)`
Creates a uniform distribution (single die).
- **Parameters:**
  - `min` (int32): Minimum value
  - `max` (int32): Maximum value
- **Returns:** Pointer to RangeDist object
- **Example:** `createUniformDist(1, 6)` creates a d6

#### `rollDice(count, sides)`
Helper function to roll multiple dice.
- **Parameters:**
  - `count` (int32): Number of dice
  - `sides` (int32): Sides per die
- **Returns:** Pointer to RangeDist object
- **Example:** `rollDice(3, 6)` creates 3d6

### Distribution Operations

#### `repeatDist(dist, count)`
Repeats a distribution (rolls multiple times and sums).
- **Parameters:**
  - `dist` (pointer): Distribution to repeat
  - `count` (uint32): Number of repetitions
- **Returns:** New RangeDist pointer

#### `addDists(lhs, rhs)`
Adds two distributions together.
- **Parameters:**
  - `lhs` (pointer): First distribution
  - `rhs` (pointer): Second distribution
- **Returns:** New RangeDist pointer

#### `addConstant(dist, constant)`
Adds a constant to a distribution.
- **Parameters:**
  - `dist` (pointer): Distribution
  - `constant` (int32): Value to add
- **Returns:** New RangeDist pointer

#### `subtractConstant(dist, constant)`
Subtracts a constant from a distribution.
- **Parameters:**
  - `dist` (pointer): Distribution
  - `constant` (int32): Value to subtract
- **Returns:** New RangeDist pointer

#### `multiplyConstant(dist, constant)`
Multiplies a distribution by a constant.
- **Parameters:**
  - `dist` (pointer): Distribution
  - `constant` (int32): Multiplier
- **Returns:** New RangeDist pointer

#### `negateDist(dist)`
Negates a distribution (multiplies by -1).
- **Parameters:**
  - `dist` (pointer): Distribution to negate
- **Returns:** New RangeDist pointer

### Querying Distributions

#### `getMin(dist)`
Gets the minimum possible value.
- **Parameters:**
  - `dist` (pointer): Distribution
- **Returns:** int32 minimum value

#### `getMax(dist)`
Gets the maximum possible value.
- **Parameters:**
  - `dist` (pointer): Distribution
- **Returns:** int32 maximum value

#### `getPercentile(dist, percentile)`
Calculates a single percentile.
- **Parameters:**
  - `dist` (pointer): Distribution
  - `percentile` (float): Percentile value (0.0 to 1.0)
- **Returns:** int32 value at that percentile
- **Example:** `getPercentile(dist, 0.5)` gets the median

#### `getPercentiles(dist, percentiles_buffer, count, results_buffer)`
Calculates multiple percentiles (more efficient than multiple calls).
- **Parameters:**
  - `dist` (pointer): Distribution
  - `percentiles_buffer` (pointer): Float array of percentile values
  - `count` (int32): Number of percentiles
  - `results_buffer` (pointer): Int32 array to store results
- **Returns:** void (results written to buffer)

#### `getProbabilityCount(dist)`
Gets the size of the probability array.
- **Parameters:**
  - `dist` (pointer): Distribution
- **Returns:** int32 array size

#### `getProbabilities(dist, buffer)`
Copies probability values to a buffer.
- **Parameters:**
  - `dist` (pointer): Distribution
  - `buffer` (pointer): Float array to store probabilities
- **Returns:** void (probabilities written to buffer)

### Memory Management

#### `destroyRangeDist(dist)`
**IMPORTANT:** Always call this to free distributions when done!
- **Parameters:**
  - `dist` (pointer): Distribution to free
- **Returns:** void

## Example Usage

### Simple Dice Roll
```javascript
// Roll 3d6
const dist = api.rollDice(3, 6);
const median = api.getPercentile(dist, 0.5);
console.log(`Median of 3d6: ${median}`);
api.destroyRangeDist(dist);
```

### Complex Expression: 2d6 + 1d4 + 5
```javascript
const d6 = api.rollDice(2, 6);
const d4 = api.rollDice(1, 4);
const sum = api.addDists(d6, d4);
const final = api.addConstant(sum, 5);

// Get percentiles
const p50 = api.getPercentile(final, 0.5);
const p95 = api.getPercentile(final, 0.95);

console.log(`Median: ${p50}, 95th percentile: ${p95}`);

// Clean up ALL distributions
api.destroyRangeDist(d6);
api.destroyRangeDist(d4);
api.destroyRangeDist(sum);
api.destroyRangeDist(final);
```

### Getting Multiple Percentiles Efficiently
```javascript
const dist = api.rollDice(4, 6);

const percentiles = [0.05, 0.25, 0.5, 0.75, 0.95];
const count = percentiles.length;

// Allocate WASM memory
const percentilesPtr = Module._malloc(count * 4); // 4 bytes per float
const resultsPtr = Module._malloc(count * 4); // 4 bytes per int32

// Copy percentiles to WASM memory
Module.HEAPF32.set(percentiles, percentilesPtr / 4);

// Call function
api.getPercentiles(dist, percentilesPtr, count, resultsPtr);

// Read results
const results = [];
for (let i = 0; i < count; i++) {
    results.push(Module.HEAP32[resultsPtr / 4 + i]);
}

console.log('Percentiles:', results);

// Free memory
Module._free(percentilesPtr);
Module._free(resultsPtr);
api.destroyRangeDist(dist);
```

## Performance Tips

1. **Reuse distributions** when possible instead of recreating them
2. **Use `getPercentiles()`** for multiple percentile calculations (more efficient)
3. **Always free distributions** with `destroyRangeDist()` to avoid memory leaks