#pragma once
#include <cstdint>
#include "../LinAlg.h"
#include "../Common.h"

namespace KdTree
{
  enum struct NodeKind : uint8_t {
    Inner = 0,
    Leaf = 1
  };

  struct Node {
    NodeKind kind;
    uint32_t parent;

    union {

      struct {
        uint32_t axis;
        float split;
        uint32_t left;
        uint32_t right;
      } inner;

      struct {
        uint32_t rangeBegin;
        uint32_t rangeEnd;
      } leaf;

    };
  };

  struct R3Point {
    Vec3f p;
    uint32_t ix;
  };

  struct R3StaticTree
  {

    R3StaticTree(Logger logger, Vec3f* P, uint32_t N);

    void getPointsWithinRadius(Vector<uint32_t>& result, const Vec3f& origin, float radius);

    void getNearestNeighbours(Vector<uint32_t>& result, const Vec3f& origin, uint32_t maximum);

    Vector<R3Point> points;
    Vector<Node> nodes;
    BBox3f bbox;
    uint32_t maxLevel = 0;

  private:

    uint32_t buildRecurse(uint32_t parent, const BBox3f& nodeBBox, R3Point* P, uint32_t N, uint32_t level);

    void getPointsWithinRadiusRecurse(Vector<uint32_t>& result, uint32_t nodeIndex, const Vec3f& origin, float radius);

  };

}
