#include <cassert>
#include "R3PointKdTree.h"
#include "../LinAlgOps.h"


namespace {

  uint32_t log2(const uint32_t n)
  {
    uint32_t rv = 0;
    auto t = n;
    while(t >>= 1) {
      rv++;
    }
    assert(n == 0u || ((1u << rv) <= n));
    assert(n < (1u << (rv + 1u)));
    return rv;
  }


  void calcBounds(Vec3f& bbmin, Vec3f& bbmax, R3PointKDTree::Point* P, uint32_t N)
  {
    bbmin = P[0].p;
    bbmax = P[0].p;
    for (uint32_t i = 0; i < N; i++) {
      bbmin = min(bbmin, P[i].p);
      bbmax = min(bbmax, P[i].p);
    }
  }

  uint32_t largestAxis(const Vec3f& bbmin, const Vec3f& bbmax)
  {
    Vec3f delta = bbmax - bbmin;
    if (delta.x > delta.y && delta.x > delta.z) {
      return 0;
    }
    else if (delta.y > delta.z) {
      return 1;
    }
    else {
      return 2;
    }

  }

  uint32_t partition(Vec3f* P, uint32_t N, uint32_t axis, float threshold)
  {
    auto i0 = 0;
    auto i1 = N - 1;
    while (true) {

      while (i0 < N && P[i0][axis] < threshold) i0++;
      while (i1 < N && threshold <= P[i0][axis]) i1--;

      if (i0 == i1) break;
      else {
        auto t = P[i0];
        P[i0] = P[i1];
        P[i1] = t;
      }
    }
  }

}

namespace {

  struct Range
  {
    uint32_t a;
    uint32_t b;
  };

}


R3PointKDTree::R3PointKDTree(Vec3f* P, uint32_t N)
{
  if (N == 0)  return;

  uint32_t log2_N = log2(N);

  points.resize(N);

  Vector<Range> stack;
  stack.pushBack(Range{ 0, N });

  while (stack.any()) {
    auto range = stack.popBack();

  }




/*

  Vec3f bbmin, bbmax;
  calcBounds(bbmin, bbmax, points.data(), N);
  auto axis = largestAxis(bbmin, bbmax);

  partition()

  */


  


}
