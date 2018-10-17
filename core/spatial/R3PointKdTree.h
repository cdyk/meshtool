#pragma once
#include <cstdint>
#include "../LinAlg.h"
#include "../Common.h"

struct R3PointKDTree
{
  struct Point {
    Vec3f p;
    uint32_t ix;
  };

  R3PointKDTree(Vec3f* P, uint32_t N);

  Vector<Point> points;


};
