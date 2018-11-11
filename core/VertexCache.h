#pragma once
#include "Common.h"

void getAverageCacheMissRatioPerTriangle(float& fifo4, float& fifo8, float& fifo16, float& fifo32, const uint32_t* indices, const uint32_t N);

__declspec(noinline) void linearSpeedVertexCacheOptimisation(Logger logger, uint32_t * output, const uint32_t* input, const uint32_t N);
