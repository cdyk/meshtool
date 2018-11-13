#pragma once
#include "LinAlg.h"


// Find bounding sphere that contains the bounding box.
void boundingSphereNaive(Vec3f& center, float& radius, const Vec3f* P, size_t N);

// Implementation of J. Ritter, An Efficient Bounding Sphere.
void boundingSphere(Vec3f& center, float& radius, const Vec3f* P, size_t N);