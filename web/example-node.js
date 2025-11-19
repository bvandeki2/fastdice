// Example of using FastDice WebAssembly module in Node.js
// Run with: node example-node.js

const fs = require('fs');
const path = require('path');

// Load the module
const moduleCode = fs.readFileSync(path.join(__dirname, 'fastdice.js'), 'utf8');

// Create a minimal Module object for Node.js
const Module = {
    wasmBinary: fs.readFileSync(path.join(__dirname, 'fastdice.wasm')),
    onRuntimeInitialized: function() {
        console.log('FastDice WebAssembly module loaded!\n');
        runExamples();
    }
};

// Evaluate the module code
eval(moduleCode);

function runExamples() {
    // Wrap C functions for easier calling
    const api = {
        createUniformDist: Module.cwrap('createUniformDist', 'number', ['number', 'number']),
        destroyRangeDist: Module.cwrap('destroyRangeDist', null, ['number']),
        repeatDist: Module.cwrap('repeatDist', 'number', ['number', 'number']),
        addDists: Module.cwrap('addDists', 'number', ['number', 'number']),
        addConstant: Module.cwrap('addConstant', 'number', ['number', 'number']),
        subtractConstant: Module.cwrap('subtractConstant', 'number', ['number', 'number']),
        multiplyConstant: Module.cwrap('multiplyConstant', 'number', ['number', 'number']),
        negateDist: Module.cwrap('negateDist', 'number', ['number']),
        getMin: Module.cwrap('getMin', 'number', ['number']),
        getMax: Module.cwrap('getMax', 'number', ['number']),
        getPercentile: Module.cwrap('getPercentile', 'number', ['number', 'number']),
        getPercentiles: Module.cwrap('getPercentiles', null, ['number', 'number', 'number', 'number']),
        rollDice: Module.cwrap('rollDice', 'number', ['number', 'number'])
    };

    // Helper function to get percentiles
    function getDistributionPercentiles(distPtr) {
        const percentiles = [0.05, 0.25, 0.5, 0.75, 0.95];
        const count = percentiles.length;
        
        const percentilesPtr = Module._malloc(count * 4);
        const resultsPtr = Module._malloc(count * 4);
        
        Module.HEAPF32.set(percentiles, percentilesPtr / 4);
        api.getPercentiles(distPtr, percentilesPtr, count, resultsPtr);
        
        const results = [];
        for (let i = 0; i < count; i++) {
            results.push(Module.HEAP32[resultsPtr / 4 + i]);
        }
        
        Module._free(percentilesPtr);
        Module._free(resultsPtr);
        
        return {
            p5: results[0],
            p25: results[1],
            p50: results[2],
            p75: results[3],
            p95: results[4]
        };
    }

    // Example 1: Simple dice roll
    console.log('Example 1: Rolling 3d6');
    console.log('======================');
    const start1 = Date.now();
    const d6_3 = api.rollDice(3, 6);
    const percentiles1 = getDistributionPercentiles(d6_3);
    const time1 = Date.now() - start1;
    
    console.log(`  5th percentile: ${percentiles1.p5}`);
    console.log(` 25th percentile: ${percentiles1.p25}`);
    console.log(' 50th percentile (median):', percentiles1.p50);
    console.log(` 75th percentile: ${percentiles1.p75}`);
    console.log(` 95th percentile: ${percentiles1.p95}`);
    console.log(`Range: ${api.getMin(d6_3)} to ${api.getMax(d6_3)}`);
    console.log(`Time: ${time1}ms\n`);
    
    api.destroyRangeDist(d6_3);

    // Example 2: Complex expression (2d6 + 1d4 + 5)
    console.log('Example 2: 2d6 + 1d4 + 5');
    console.log('=========================');
    const start2 = Date.now();
    const d6_2 = api.rollDice(2, 6);
    const d4_1 = api.rollDice(1, 4);
    const sum = api.addDists(d6_2, d4_1);
    const final = api.addConstant(sum, 5);
    const percentiles2 = getDistributionPercentiles(final);
    const time2 = Date.now() - start2;
    
    console.log(`  5th percentile: ${percentiles2.p5}`);
    console.log(` 25th percentile: ${percentiles2.p25}`);
    console.log(' 50th percentile (median):', percentiles2.p50);
    console.log(` 75th percentile: ${percentiles2.p75}`);
    console.log(` 95th percentile: ${percentiles2.p95}`);
    console.log(`Range: ${api.getMin(final)} to ${api.getMax(final)}`);
    console.log(`Time: ${time2}ms\n`);
    
    api.destroyRangeDist(d6_2);
    api.destroyRangeDist(d4_1);
    api.destroyRangeDist(sum);
    api.destroyRangeDist(final);

    // Example 3: Large computation (10d100)
    console.log('Example 3: Rolling 10d100 (large computation)');
    console.log('===============================================');
    const start3 = Date.now();
    const d100_10 = api.rollDice(10, 100);
    const percentiles3 = getDistributionPercentiles(d100_10);
    const time3 = Date.now() - start3;
    
    console.log(`  5th percentile: ${percentiles3.p5}`);
    console.log(` 25th percentile: ${percentiles3.p25}`);
    console.log(' 50th percentile (median):', percentiles3.p50);
    console.log(` 75th percentile: ${percentiles3.p75}`);
    console.log(` 95th percentile: ${percentiles3.p95}`);
    console.log(`Range: ${api.getMin(d100_10)} to ${api.getMax(d100_10)}`);
    console.log(`Time: ${time3}ms (using FFT optimization!)\n`);
    
    api.destroyRangeDist(d100_10);

    // Example 4: Attack roll with modifier
    console.log('Example 4: Attack roll (1d20 + 5)');
    console.log('===================================');
    const start4 = Date.now();
    const d20 = api.rollDice(1, 20);
    const attackRoll = api.addConstant(d20, 5);
    const percentiles4 = getDistributionPercentiles(attackRoll);
    const time4 = Date.now() - start4;
    
    console.log(`  5th percentile: ${percentiles4.p5}`);
    console.log(` 25th percentile: ${percentiles4.p25}`);
    console.log(' 50th percentile (median):', percentiles4.p50);
    console.log(` 75th percentile: ${percentiles4.p75}`);
    console.log(` 95th percentile: ${percentiles4.p95}`);
    console.log(`Range: ${api.getMin(attackRoll)} to ${api.getMax(attackRoll)}`);
    console.log(`Time: ${time4}ms\n`);
    
    api.destroyRangeDist(d20);
    api.destroyRangeDist(attackRoll);

    console.log('All examples completed successfully!');
}
