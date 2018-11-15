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


uint32_t KdTree::R3StaticTree::buildRecurse(const BBox3f& nodeBBox, R3Point* P, uint32_t N, uint32_t level, bool preferSpatialSplit)
{
  assert(0 < N);
  if (maxLevel < level) {
    maxLevel = level;
  }
  auto nodeIx = nodes.size32();
  nodes.resize(nodes.size() + 1);

  if (5 <= N) {
    uint32_t axes[3];
    getOrderedAxes(axes, nodeBBox);

    for (unsigned a = 0; a < 3; a++) {
      auto axis = axes[a];
      uint32_t splitIdx;
      float splitVal;

      if (preferSpatialSplit) {
        splitVal = 0.5f*(nodeBBox.min[axis] + nodeBBox.max[axis]);
        splitIdx = partition(P, N, axis, splitVal);
      }
      if (!preferSpatialSplit || splitIdx == 0 || splitIdx == N) {
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

        auto childIx0 = buildRecurse(bboxLeft, P, splitIdx, level + 1, preferSpatialSplit);
        auto childIx1 = buildRecurse(bboxRight, P + splitIdx, N - splitIdx, level + 1, preferSpatialSplit);

        auto & node = nodes[nodeIx];
        node.kind = NodeKind::Inner;
        node.inner.axis = axis;
        node.inner.split = splitVal;
        node.inner.children[0] = childIx0;
        node.inner.children[1] = childIx1;
        return nodeIx;
      }
    }
  }

  auto & node = nodes[nodeIx];
  node.kind = NodeKind::Leaf;
  node.leaf.rangeBegin = uint32_t(P - points.data());
  node.leaf.rangeEnd = node.leaf.rangeBegin + N;
  return nodeIx;
}


KdTree::R3StaticTree::R3StaticTree(Logger logger, Vec3f* P, uint32_t N, bool preferSpatialSplit)
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
  buildRecurse(bbox, points.data(), N, 0, preferSpatialSplit);

  logger(0, "log2=%d, maxLevel=%d, N=%d", log2(N), maxLevel, N);
}

void KdTree::R3StaticTree::assertInvariantsRecurse(std::vector<unsigned>& touched, const BBox3f& domain, uint32_t nodeIx)
{
  assert(nodeIx < nodes.size32());
  const auto & node = nodes[nodeIx];
  if (node.kind == NodeKind::Inner) {
    assert(node.inner.children[0] != nodeIx);
    assert(node.inner.children[1] != nodeIx);

    auto leftDomain = domain;
    leftDomain.max[node.inner.axis] = node.inner.split;
    assertInvariantsRecurse(touched, leftDomain, node.inner.children[0]);

    auto rightDomain = domain;
    leftDomain.min[node.inner.axis] = node.inner.split;
    assertInvariantsRecurse(touched, leftDomain, node.inner.children[1]);
  }
  else {
    assert(node.kind == NodeKind::Leaf);
    assert(node.leaf.rangeBegin < node.leaf.rangeEnd);
    assert(node.leaf.rangeBegin < points.size32());
    assert(node.leaf.rangeEnd <= points.size32());
    for (uint32_t i = node.leaf.rangeBegin; i < node.leaf.rangeEnd; i++) {
      touched[i]++;
      for (unsigned k = 0; k < 3; k++) {
        assert(bbox.min[k] <= points[i].p[k]);
        assert(points[i].p[k] <= bbox.max[k]);  // should work on strict less than..?
      }
    }
  }
}

void KdTree::R3StaticTree::assertInvariants()
{
  if (nodes.empty()) return;

  std::vector<unsigned> touched(points.size());

  BBox3f range;
  range.min = Vec3f(-FLT_MAX);
  range.max = Vec3f(FLT_MAX);
  assertInvariantsRecurse(touched, range, 0);

  for (size_t i = 0; i < touched.size(); i++) {
    assert(touched[i] == 1);
  }
}


void KdTree::R3StaticTree::getPointsWithinRadiusRecurse(Vector<QueryResult>& result, uint32_t nodeIndex, const Vec3f& origin, float radius)
{
  auto & node = nodes[nodeIndex];
  if (node.kind == NodeKind::Leaf) {
    auto radiusSquared = radius * radius;
    for (auto i = node.leaf.rangeBegin; i < node.leaf.rangeEnd; i++) {
      auto d2 = distanceSquared(origin, points[i].p);
      if (d2 == 0.f) {
        int a = 2;
      }

      if (d2 <= radiusSquared) {
        result.pushBack({points[i].ix, d2});
      }
    }
  }
  else {
    if (origin[node.inner.axis] - radius <= node.inner.split) {
      getPointsWithinRadiusRecurse(result, node.inner.children[0], origin, radius);
    }
    if (node.inner.split <= origin[node.inner.axis] + radius) {
      getPointsWithinRadiusRecurse(result, node.inner.children[1], origin, radius);
    }
  }
}


void KdTree::R3StaticTree::getPointsWithinRadius(Vector<QueryResult>& result, const Vec3f& origin, float radius)
{
  result.resize(0);
  if (points.empty()) return;

  getPointsWithinRadiusRecurse(result, 0, origin, radius);
}


void KdTree::R3StaticTree::getNearestNeighboursRecurse(Vector<QueryResult>& result, uint32_t nodeIx, const Vec3f& origin, uint32_t K)
{
  auto & node = nodes[nodeIx];
  if (node.kind == NodeKind::Leaf) {

    for (uint32_t j = node.leaf.rangeBegin; j < node.leaf.rangeEnd; j++) {
      auto & point = points[j];
      auto d2 = distanceSquared(origin, point.p);
      auto n = result.size();

      if (n < K || d2 < result[n - 1].distanceSquared) {

        if (n < K) {
          result.resize(n + 1);
        }
        else {
          n = K - 1;
        }

        auto i = n;
        for (; 0 < i && d2 < result[i - 1].distanceSquared; i--) {
          result[i] = result[i - 1];
        }
        result[i] = { point.ix, d2 };

        //for (unsigned k = 1; k < result.size32(); k++) {
        //  assert(result[k - 1].distanceSquared <= result[k].distanceSquared);
        //}
      }
    }
  }
  else {
    assert(node.kind == NodeKind::Inner);
    auto d = origin[node.inner.axis] - node.inner.split;
    getNearestNeighboursRecurse(result, node.inner.children[d < 0.f ? 0 : 1], origin, K);
    if (result.size32() < K || d*d < result[K - 1].distanceSquared) {
      getNearestNeighboursRecurse(result, node.inner.children[d < 0.f ? 1 : 0], origin, K);
    }
  }
}


void KdTree::R3StaticTree::getNearestNeighbours(Vector<QueryResult>& result, const Vec3f& origin, uint32_t K)
{
  result.resize(0);
  if (points.empty()) return;

  result.reserve(K);
  getNearestNeighboursRecurse(result, 0, origin, K);
}
