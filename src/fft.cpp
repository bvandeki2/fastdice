#define MEOW_FFT_IMPLEMENTATION
#include "meow_fft.h"

// fft.hpp has to come after meow_fft.h to get the definitions right
#include "fft.hpp"
#include <algorithm>
#include <stdexcept>

size_t nextBestSize(size_t n) {
    // just power of 2 for now, though we have radix 3 and 5 available too
    // min fft size is 4
    size_t pow2 = 4;
    while (pow2 < n) {
        pow2 <<= 1;
    }
    return pow2;
}

void convolveFFT(
    const std::vector<float_t>::const_iterator lhsBegin,
    const std::vector<float_t>::const_iterator lhsEnd,
    const std::vector<float_t>::const_iterator rhsBegin,
    const std::vector<float_t>::const_iterator rhsEnd,
    std::vector<float_t>::iterator resultBegin
) {
    size_t lhsSize = std::distance(lhsBegin, lhsEnd);
    size_t rhsSize = std::distance(rhsBegin, rhsEnd);
    size_t resultSize = lhsSize + rhsSize - 1;

    int fftSize = static_cast<int>(nextBestSize(lhsSize + rhsSize - 1));
    std::vector<float_t> lhsPadded(fftSize, 0.0f);
    std::vector<float_t> rhsPadded(fftSize, 0.0f);

    std::copy(lhsBegin, lhsEnd, lhsPadded.begin());
    std::copy(rhsBegin, rhsEnd, rhsPadded.begin());

    std::vector<Meow_FFT_Complex> lhsFFT(fftSize / 2 + 1);
    std::vector<Meow_FFT_Complex> rhsFFT(fftSize / 2 + 1);
    std::vector<Meow_FFT_Complex> resultFFT(fftSize / 2 + 1);

    Meow_FFT_Workset_Real* workset
        = (Meow_FFT_Workset_Real*)malloc(meow_fft_generate_workset_real(fftSize, nullptr));
    meow_fft_generate_workset_real(fftSize, workset);

    meow_fft_real(workset, lhsPadded.data(), lhsFFT.data());
    meow_fft_real(workset, rhsPadded.data(), rhsFFT.data());

    for (size_t i = 0; i < lhsFFT.size(); ++i) {
        resultFFT[i] = meow_mul(lhsFFT[i], rhsFFT[i]);
    }

    std::vector<float_t> resultPadded(fftSize, 0.0f);
    meow_fft_real_i(workset, resultFFT.data(), lhsFFT.data(), resultPadded.data());
    free(workset);

    std::transform(
        resultPadded.begin(),
        resultPadded.begin() + resultSize,
        resultBegin,
        [fftSize](float_t val) { return val / fftSize; }
    );
}

Meow_FFT_Complex complexPow(Meow_FFT_Complex base, uint64_t exponent) {
    Meow_FFT_Complex result { 1.0f, 0.0f };

    while (exponent > 0) {
        if (exponent & 1U) {
            result = meow_mul(result, base);
        }
        base = meow_mul(base, base);
        exponent >>= 1U;
    }

    return result;
}

void nConvolveFFT(
    const std::vector<float_t>::const_iterator begin,
    const std::vector<float_t>::const_iterator end,
    size_t count,
    std::vector<float_t>::iterator resultBegin
) {
    size_t inputSize = std::distance(begin, end);
    size_t resultSize = inputSize * count - (count - 1);

    int fftSize = static_cast<int>(nextBestSize(resultSize));
    std::vector<float_t> paddedInput(fftSize, 0.0f);
    std::copy(begin, end, paddedInput.begin());

    std::vector<Meow_FFT_Complex> inputFFT(fftSize / 2 + 1);
    Meow_FFT_Workset_Real* workset
        = (Meow_FFT_Workset_Real*)malloc(meow_fft_generate_workset_real(fftSize, nullptr));
    meow_fft_generate_workset_real(fftSize, workset);

    meow_fft_real(workset, paddedInput.data(), inputFFT.data());

    for (size_t i = 0; i < inputFFT.size(); ++i) {
        inputFFT[i] = complexPow(inputFFT[i], count);
    }

    std::vector<float_t> resultPadded(fftSize, 0.0f);
    meow_fft_real_i(workset, inputFFT.data(), inputFFT.data(), resultPadded.data());
    free(workset);

    std::transform(
        resultPadded.begin(),
        resultPadded.begin() + resultSize,
        resultBegin,
        [fftSize](float_t val) { return val / fftSize; }
    );
}