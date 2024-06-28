// Relevant functions pulled from rgbcx v1.13

#pragma once

#include <stdint.h>

namespace rgbcx {

void encode_bc4(void *pDst, const uint8_t *pPixels, uint32_t stride = 4);
void encode_bc5(void *pDst, const uint8_t *pPixels, uint32_t chan0 = 0, uint32_t chan1 = 1, uint32_t stride = 4);

} // namespace rgbcx
