#include "bc5enc.hh"

#include <vull/maths/common.hh>

#include <stdint.h>
#include <string.h>

namespace rgbcx {

void encode_bc4(void *pDst, const uint8_t *pPixels, uint32_t stride) {
    uint32_t min0_v, max0_v, min1_v, max1_v, min2_v, max2_v, min3_v, max3_v;

    {
        min0_v = max0_v = pPixels[0 * stride];
        min1_v = max1_v = pPixels[1 * stride];
        min2_v = max2_v = pPixels[2 * stride];
        min3_v = max3_v = pPixels[3 * stride];
    }

    {
        uint32_t v0 = pPixels[4 * stride];
        min0_v = vull::min(min0_v, v0);
        max0_v = vull::max(max0_v, v0);
        uint32_t v1 = pPixels[5 * stride];
        min1_v = vull::min(min1_v, v1);
        max1_v = vull::max(max1_v, v1);
        uint32_t v2 = pPixels[6 * stride];
        min2_v = vull::min(min2_v, v2);
        max2_v = vull::max(max2_v, v2);
        uint32_t v3 = pPixels[7 * stride];
        min3_v = vull::min(min3_v, v3);
        max3_v = vull::max(max3_v, v3);
    }

    {
        uint32_t v0 = pPixels[8 * stride];
        min0_v = vull::min(min0_v, v0);
        max0_v = vull::max(max0_v, v0);
        uint32_t v1 = pPixels[9 * stride];
        min1_v = vull::min(min1_v, v1);
        max1_v = vull::max(max1_v, v1);
        uint32_t v2 = pPixels[10 * stride];
        min2_v = vull::min(min2_v, v2);
        max2_v = vull::max(max2_v, v2);
        uint32_t v3 = pPixels[11 * stride];
        min3_v = vull::min(min3_v, v3);
        max3_v = vull::max(max3_v, v3);
    }

    {
        uint32_t v0 = pPixels[12 * stride];
        min0_v = vull::min(min0_v, v0);
        max0_v = vull::max(max0_v, v0);
        uint32_t v1 = pPixels[13 * stride];
        min1_v = vull::min(min1_v, v1);
        max1_v = vull::max(max1_v, v1);
        uint32_t v2 = pPixels[14 * stride];
        min2_v = vull::min(min2_v, v2);
        max2_v = vull::max(max2_v, v2);
        uint32_t v3 = pPixels[15 * stride];
        min3_v = vull::min(min3_v, v3);
        max3_v = vull::max(max3_v, v3);
    }

    const uint32_t min_v = vull::min(vull::min(vull::min(min0_v, min1_v), min2_v), min3_v);
    const uint32_t max_v = vull::max(vull::max(vull::max(max0_v, max1_v), max2_v), max3_v);

    uint8_t *pDst_bytes = static_cast<uint8_t *>(pDst);
    pDst_bytes[0] = (uint8_t)max_v;
    pDst_bytes[1] = (uint8_t)min_v;

    if (max_v == min_v) {
        memset(pDst_bytes + 2, 0, 6);
        return;
    }

    const uint32_t delta = max_v - min_v;

    // min_v is now 0. Compute thresholds between values by scaling max_v. It's x14 because we're adding two x7 scale
    // factors.
    const int t0 = delta * 13;
    const int t1 = delta * 11;
    const int t2 = delta * 9;
    const int t3 = delta * 7;
    const int t4 = delta * 5;
    const int t5 = delta * 3;
    const int t6 = delta * 1;

    // BC4 floors in its divisions, which we compensate for with the 4 bias.
    // This function is optimal for all possible inputs (i.e. it outputs the same results as checking all 8 values and
    // choosing the closest one).
    const int bias = 4 - min_v * 14;

    static const uint32_t s_tran0[8] = {1U, 7U, 6U, 5U, 4U, 3U, 2U, 0U};
    static const uint32_t s_tran1[8] = {1U << 3U, 7U << 3U, 6U << 3U, 5U << 3U, 4U << 3U, 3U << 3U, 2U << 3U, 0U << 3U};
    static const uint32_t s_tran2[8] = {1U << 6U, 7U << 6U, 6U << 6U, 5U << 6U, 4U << 6U, 3U << 6U, 2U << 6U, 0U << 6U};
    static const uint32_t s_tran3[8] = {1U << 9U, 7U << 9U, 6U << 9U, 5U << 9U, 4U << 9U, 3U << 9U, 2U << 9U, 0U << 9U};

    uint64_t a0, a1, a2, a3;
    {
        const int v0 = pPixels[0 * stride] * 14 + bias;
        const int v1 = pPixels[1 * stride] * 14 + bias;
        const int v2 = pPixels[2 * stride] * 14 + bias;
        const int v3 = pPixels[3 * stride] * 14 + bias;
        a0 = s_tran0[(v0 >= t0) + (v0 >= t1) + (v0 >= t2) + (v0 >= t3) + (v0 >= t4) + (v0 >= t5) + (v0 >= t6)];
        a1 = s_tran1[(v1 >= t0) + (v1 >= t1) + (v1 >= t2) + (v1 >= t3) + (v1 >= t4) + (v1 >= t5) + (v1 >= t6)];
        a2 = s_tran2[(v2 >= t0) + (v2 >= t1) + (v2 >= t2) + (v2 >= t3) + (v2 >= t4) + (v2 >= t5) + (v2 >= t6)];
        a3 = s_tran3[(v3 >= t0) + (v3 >= t1) + (v3 >= t2) + (v3 >= t3) + (v3 >= t4) + (v3 >= t5) + (v3 >= t6)];
    }

    {
        const int v0 = pPixels[4 * stride] * 14 + bias;
        const int v1 = pPixels[5 * stride] * 14 + bias;
        const int v2 = pPixels[6 * stride] * 14 + bias;
        const int v3 = pPixels[7 * stride] * 14 + bias;
        a0 |=
            (uint64_t)(s_tran0[(v0 >= t0) + (v0 >= t1) + (v0 >= t2) + (v0 >= t3) + (v0 >= t4) + (v0 >= t5) + (v0 >= t6)]
                       << 12U);
        a1 |=
            (uint64_t)(s_tran1[(v1 >= t0) + (v1 >= t1) + (v1 >= t2) + (v1 >= t3) + (v1 >= t4) + (v1 >= t5) + (v1 >= t6)]
                       << 12U);
        a2 |=
            (uint64_t)(s_tran2[(v2 >= t0) + (v2 >= t1) + (v2 >= t2) + (v2 >= t3) + (v2 >= t4) + (v2 >= t5) + (v2 >= t6)]
                       << 12U);
        a3 |=
            (uint64_t)(s_tran3[(v3 >= t0) + (v3 >= t1) + (v3 >= t2) + (v3 >= t3) + (v3 >= t4) + (v3 >= t5) + (v3 >= t6)]
                       << 12U);
    }

    {
        const int v0 = pPixels[8 * stride] * 14 + bias;
        const int v1 = pPixels[9 * stride] * 14 + bias;
        const int v2 = pPixels[10 * stride] * 14 + bias;
        const int v3 = pPixels[11 * stride] * 14 + bias;
        a0 |= (((uint64_t)
                    s_tran0[(v0 >= t0) + (v0 >= t1) + (v0 >= t2) + (v0 >= t3) + (v0 >= t4) + (v0 >= t5) + (v0 >= t6)])
               << 24U);
        a1 |= (((uint64_t)
                    s_tran1[(v1 >= t0) + (v1 >= t1) + (v1 >= t2) + (v1 >= t3) + (v1 >= t4) + (v1 >= t5) + (v1 >= t6)])
               << 24U);
        a2 |= (((uint64_t)
                    s_tran2[(v2 >= t0) + (v2 >= t1) + (v2 >= t2) + (v2 >= t3) + (v2 >= t4) + (v2 >= t5) + (v2 >= t6)])
               << 24U);
        a3 |= (((uint64_t)
                    s_tran3[(v3 >= t0) + (v3 >= t1) + (v3 >= t2) + (v3 >= t3) + (v3 >= t4) + (v3 >= t5) + (v3 >= t6)])
               << 24U);
    }

    {
        const int v0 = pPixels[12 * stride] * 14 + bias;
        const int v1 = pPixels[13 * stride] * 14 + bias;
        const int v2 = pPixels[14 * stride] * 14 + bias;
        const int v3 = pPixels[15 * stride] * 14 + bias;
        a0 |= (((uint64_t)
                    s_tran0[(v0 >= t0) + (v0 >= t1) + (v0 >= t2) + (v0 >= t3) + (v0 >= t4) + (v0 >= t5) + (v0 >= t6)])
               << 36U);
        a1 |= (((uint64_t)
                    s_tran1[(v1 >= t0) + (v1 >= t1) + (v1 >= t2) + (v1 >= t3) + (v1 >= t4) + (v1 >= t5) + (v1 >= t6)])
               << 36U);
        a2 |= (((uint64_t)
                    s_tran2[(v2 >= t0) + (v2 >= t1) + (v2 >= t2) + (v2 >= t3) + (v2 >= t4) + (v2 >= t5) + (v2 >= t6)])
               << 36U);
        a3 |= (((uint64_t)
                    s_tran3[(v3 >= t0) + (v3 >= t1) + (v3 >= t2) + (v3 >= t3) + (v3 >= t4) + (v3 >= t5) + (v3 >= t6)])
               << 36U);
    }

    const uint64_t f = a0 | a1 | a2 | a3;

    pDst_bytes[2] = (uint8_t)f;
    pDst_bytes[3] = (uint8_t)(f >> 8U);
    pDst_bytes[4] = (uint8_t)(f >> 16U);
    pDst_bytes[5] = (uint8_t)(f >> 24U);
    pDst_bytes[6] = (uint8_t)(f >> 32U);
    pDst_bytes[7] = (uint8_t)(f >> 40U);
}

void encode_bc5(void *pDst, const uint8_t *pPixels, uint32_t chan0, uint32_t chan1, uint32_t stride) {
    encode_bc4(pDst, pPixels + chan0, stride);
    encode_bc4(static_cast<uint8_t *>(pDst) + 8, pPixels + chan1, stride);
}

} // namespace rgbcx
