#ifndef FASTDICE_FFT_HPP
#define FASTDICE_FFT_HPP

// Note to self: meow_fft might not have implementations pulled in correctly, it's header only and
// has a def.
#include "meow_fft.h"
#include <vector>

size_t nextBestSize(size_t n);

void convolveFFT(
    const std::vector<float_t>::const_iterator lhsBegin,
    const std::vector<float_t>::const_iterator lhsEnd,
    const std::vector<float_t>::const_iterator rhsBegin,
    const std::vector<float_t>::const_iterator rhsEnd,
    std::vector<float_t>::iterator resultBegin
);

void nConvolveFFT(
    const std::vector<float_t>::const_iterator begin,
    const std::vector<float_t>::const_iterator end,
    size_t count,
    std::vector<float_t>::iterator resultBegin
);

Meow_FFT_Complex complexPow(Meow_FFT_Complex base, uint32_t exponent);

#endif // FASTDICE_FFT_HPP