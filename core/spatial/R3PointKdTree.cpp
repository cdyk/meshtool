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


  void calcBounds(BBox3f& bbox, const KdTree::R3Point* P, uint32_t N)
  {
    bbox.min = P[0].p;
    bbox.max = P[0].p;
    for (uint32_t i = 0; i < N; i++) {
      bbox.min = min(bbox.min, P[i].p);
      bbox.max = max(bbox.max, P[i].p);
    }
  }

  void getOrderedAxes(uint32_t* axes, const BBox3f& bbox)
  {
    Vec3f delta = bbox.max - bbox.min;
    axes[0] = delta.x > delta.y ? 0 : 1;
    axes[1] = delta.x > delta.y ? 1 : 0;
    axes[2] = 2;

    if (delta.z > delta[axes[0]]) {
      auto t = axes[0];
      axes[0] = axes[2];
      axes[2] = t;
    }

    if (delta[axes[2]] > delta[axes[1]]) {
      auto t = axes[1];
      axes[1] = axes[2];
      axes[2] = t;
    }

    assert(axes[0] != axes[1]);
    assert(axes[1] != axes[2]);
    assert(delta[axes[0]] >= delta[axes[1]]);
    assert(delta[axes[1]] >= delta[axes[2]]);
  }

  uint32_t partition(KdTree::R3Point* P, uint32_t N, uint32_t axis, float threshold)
  {
    auto A = 0u;
    auto B = N;
    while (true) {
      // invariant: p[i] < threshold for all i < A,
      //            threshold <= p[A] 
      while (A < N && P[A].p[axis] < threshold) A++;
      assert(A == N || threshold <= P[A].p[axis]);

      // invariant threshold <= p[i] for all B <= i 
      //           p[B-1] < threshold
      while (0 < B && threshold <= P[B - 1].p[axis]) B--;
      assert(B == 0 || P[B - 1].p[axis] < threshold);

      assert(A <= B);
      assert(B <= N);
      assert(A != B - 1);
      if (A == B) break;
      else {
        auto t = P[A];
        P[A] = P[B-1];
        P[B - 1] = t;
      }
    }

    for (uint32_t i = 0; i < A; i++) {
      assert(P[i].p[axis] < threshold);
    }
    for (uint32_t i = A; i < N; i++) {
      assert(threshold <= P[i].p[axis]);
    }

    return A;
  }

}

namespace {

  struct Range
  {
    BBox3f bbox;
    uint32_t a;
    uint32_t b;
  };

}


uint32_t KdTree::R3StaticTree::buildRecurse(uint32_t parent, const BBox3f& nodeBBox, R3Point* P, uint32_t N, uint32_t level)
{
  assert(0 < N);
  if (maxLevel < level) {
    maxLevel = level;
  }

  auto nodeIx = nodes.size32();
  nodes.pushBack(Node{});
  auto & node = nodes[nodeIx];
  node.parent = parent;

  if (5 <= N) {
    uint32_t axes[3];
    getOrderedAxes(axes, nodeBBox);

    for (unsigned a = 0; a < 3; a++) {
      auto axis = axes[a];
      auto splitVal = 0.5f*(nodeBBox.min[axis] + nodeBBox.max[axis]);
      auto splitIdx = partition(P, N, axis, splitVal);

      if (splitIdx == 0 || splitIdx == N) {
        // all points in one child, try approximate median
        float values[5];
        values[0] = points[0].p[axis];
        for (uint32_t j = 1; j < 5; j++) {
          // insertion sort
          auto t = points[(j*(N - 1)) / 4].p[axis];
          auto i = j;
          for (; 0 < i && t < values[i - 1]; i--) {
            values[i] = values[i - 1];
          }
          values[i] = t;
        }
        assert(values[0] <= values[1]);
        assert(values[1] <= values[2]);
        assert(values[2] <= values[3]);
        assert(values[3] <= values[4]);

        splitVal = values[2];
        splitIdx = partition(P, N, axis, splitVal);
      }

      if (splitIdx != 0 && splitIdx != N) {

        auto bboxLeft = nodeBBox;
        bboxLeft.max[axis] = splitVal;

        auto bboxRight = nodeBBox;
        bboxRight.min[axis] = splitVal;

        node.kind = NodeKind::Inner;
        node.inner.axis = axis;
        node.inner.split = splitVal;
        node.inner.left = buildRecurse(nodeIx, bboxLeft, P, splitIdx, level + 1);
        node.inner.right = buildRecurse(nodeIx, bboxRight, P + splitIdx, N - splitIdx, level + 1);

        return nodeIx;
      }
    }
  }

  node.kind = NodeKind::Leaf;
  node.leaf.rangeBegin = uint32_t(P - points.data());
  node.leaf.rangeEnd = node.leaf.rangeBegin + N;
  return nodeIx;
}


KdTree::R3StaticTree::R3StaticTree(Logger logger, Vec3f* P, uint32_t N)
{
  points.resize(N);
  nodes.resize(0);
  if (N == 0)  return;

  for (uint32_t i = 0; i < N; i++) {
    points[i].p = P[i];
    points[i].ix = i;
  }

  //uint32_t log2_N = log2(N);
  nodes.reserve(N);
  calcBounds(bbox, points.data(), N);
  buildRecurse(~0u, bbox, points.data(), N, 0);

  logger(0, "log2=%d, maxLevel=%d, N=%d", log2(N), maxLevel, N);
}


void KdTree::R3StaticTree::getPointsWithinRadiusRecurse(Vector<uint32_t>& result, uint32_t nodeIndex, const Vec3f& origin, float radius)
{
  auto & node = nodes[nodeIndex];
  if (node.kind == NodeKind::Leaf) {
    for (auto i = node.leaf.rangeBegin; i < node.leaf.rangeEnd; i++) {
      if (distanceSquared(origin, points[i].p) <= radius) {
        result.pushBack(points[i].ix);
      }
    }
  }
  else {
    if (origin[node.inner.axis] - radius < node.inner.split) {
      getPointsWithinRadiusRecurse(result, node.inner.left, origin, radius);
    }
    if (node.inner.split <= origin[node.inner.axis] + radius) {
      getPointsWithinRadiusRecurse(result, node.inner.right, origin, radius);
    }
  }
}

void KdTree::R3StaticTree::getPointsWithinRadius(Vector<uint32_t>& result, const Vec3f& origin, float radius)
{
  result.resize(0);
  if (points.empty()) return;

  getPointsWithinRadiusRecurse(result, 0, origin, radius);
}

void KdTree::R3StaticTree::getNearestNeighbours(Vector<uint32_t>& result, const Vec3f& origin, uint32_t maximum)
{

}
