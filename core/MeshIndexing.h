#pragma once
#include "Common.h"

void getEdges(Logger, Vector<uint32_t>& edgeIndices, const uint32_t* triVtxIx, const uint32_t triCount);

void uniqueIndices(Logger logger, Vector<uint32_t>& indices, Vector<uint32_t>& vertices, const uint32_t* vtxIx, const uint32_t* nrmIx, const uint32_t N);
