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
        uint32_t children[2];
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

  struct QueryResult
  {
    uint32_t ix;
    float distanceSquared;
  };

  struct R3StaticTree
  {


    // preferSpatialSplit - trade balanced tree away for balanced spatial coverage
    R3StaticTree(Logger logger, Vec3f* P, uint32_t N, bool preferSpatialSplit);

    void assertInvariants();

    void getPointsWithinRadius(Vector<QueryResult>& result, const Vec3f& origin, float radius);

    void getNearestNeighbours(Vector<QueryResult>& result, const Vec3f& origin, uint32_t K);

    KdTree::QueryResult getNearest(const Vec3f& origin);

    Vector<R3Point> points;
    Vector<Node> nodes;
    BBox3f bbox;
    uint32_t maxLevel = 0;

  private:
    void assertInvariantsRecurse(std::vector<unsigned>& touched, const BBox3f& domain, uint32_t nodeIx);

    uint32_t buildRecurse(const BBox3f& nodeBBox, R3Point* P, uint32_t N, uint32_t level, bool preferSpatialSplit);

    void getPointsWithinRadiusRecurse(Vector<QueryResult>& result, uint32_t nodeIndex, const Vec3f& origin, float radius);

    void getNearestNeighboursRecurse(Vector<QueryResult>& result, uint32_t nodeIx, const Vec3f& origin, uint32_t K);

    void getNearestRecurse(QueryResult& result, uint32_t nodeIx, const Vec3f& origin);

  };

}
