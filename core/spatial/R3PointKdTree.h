#pragma once
#include <cstdint>
#include <vector>
#include "../LinAlg.h"
#include "../Common.h"

namespace KdTree
{
  enum struct NodeKind : uint8_t {
    Inner = 1,
    Leaf = 2
  };

  struct Node {
    union {
      NodeKind kind;
      struct {
        NodeKind kind;
        uint8_t axis;
        float split;
        uint32_t left;
        uint32_t right;
      } inner;

      struct {
        NodeKind kind;
        uint32_t rangeBegin;
        uint32_t rangeEnd;
      } leaf;
    };

  };
  static_assert(sizeof(Node) == 4 * sizeof(uint32_t));

  struct R3Point {
    Vec3f p;
    uint32_t ix;
  };

  struct R3StaticTree
  {

    // preferSpatialSplit - trade balanced tree away for balanced spatial coverage
    R3StaticTree(Logger logger, Vec3f* P, uint32_t N, bool preferSpatialSplit);

    void assertInvariants();

    void getPointsWithinRadius(Vector<uint32_t>& result, const Vec3f& origin, float radius);

    void getNearestNeighbours(Vector<uint32_t>& result, const Vec3f& origin, uint32_t maximum);

    std::vector<R3Point> points;
    std::vector<Node> nodes;
    BBox3f bbox;
    uint32_t maxLevel = 0;

  private:
    void assertInvariantsRecurse(std::vector<unsigned>& touched, const BBox3f& domain, uint32_t nodeIx);

    uint32_t buildRecurse(const BBox3f& nodeBBox, R3Point* P, uint32_t N, uint32_t level, bool preferSpatialSplit);

    void getPointsWithinRadiusRecurse(Vector<uint32_t>& result, uint32_t nodeIndex, const Vec3f& origin, float radius);

  };

}
