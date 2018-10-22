#pragma once
#include <cmath>
#include <cfloat>
#include "LinAlg.h"

// Vec2f

inline Vec2f operator*(const float a, const Vec2f& b) { return Vec2f(a*b.x, a*b.y); }

inline Vec2f operator-(const Vec2f& a, const Vec2f& b) { return Vec2f(a.x - b.x, a.y - b.y); }

inline Vec2f operator+(const Vec2f& a, const Vec2f& b) { return Vec2f(a.x + b.x, a.y + b.y); }

inline float dot(const Vec2f& a, const Vec2f& b) { return a.x * b.x + a.y * b.y; }

inline float lengthSquared(const Vec2f& a) { return dot(a, a); }


// Vec3f

inline Vec3f cross(const Vec3f& a, const Vec3f& b)
{
  return Vec3f(a.y * b.z - a.z * b.y,
               a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x);
}

inline float dot(const Vec3f& a, const Vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3f operator+(const Vec3f& a, const Vec3f& b) { return Vec3f(a.x + b.x, a.y + b.y, a.z + b.z); }

inline Vec3f operator-(const Vec3f& a, const Vec3f& b) { return Vec3f(a.x - b.x, a.y - b.y, a.z - b.z); }

inline Vec3f operator*(const float a, const Vec3f& b) { return Vec3f(a*b.x, a*b.y, a*b.z); }

inline float lengthSquared(const Vec3f& a) { return dot(a, a); }

inline float length(const Vec3f& a) { return std::sqrt(dot(a, a)); }

inline float distanceSquared(const Vec3f& a, const Vec3f&b) { return lengthSquared(a - b); }

inline float distance(const Vec3f& a, const Vec3f&b) { return length(a - b); }

inline Vec3f normalize(const Vec3f& a) { return (1.f / length(a))*a; }

inline void write(float* dst, const Vec3f& a) { *dst++ = a.data[0]; *dst++ = a.data[1]; *dst++ = a.data[2]; }

inline Vec3f max(const Vec3f& a, const Vec3f& b)
{
  return Vec3f(a.x > b.x ? a.x : b.x,
               a.y > b.y ? a.y : b.y,
               a.z > b.z ? a.z : b.z);
}

inline Vec3f min(const Vec3f& a, const Vec3f& b)
{
  return Vec3f(a.x < b.x ? a.x : b.x,
               a.y < b.y ? a.y : b.y,
               a.z < b.z ? a.z : b.z);
}

void tangentSpaceBasis(Vec3f& u, Vec3f& v, const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec2f& t0, const Vec2f& t1, const Vec2f& t2);

// Quatf

inline Quatf creatUnitQuatf() { return Quatf(0, 0, 0, 1); }

Quatf mul(const Quatf& a, const Quatf& b);

Quatf mul(const Quatf& q, const Vec3f& v);

Quatf mul(const Vec3f& v, const Quatf& q);

Quatf axisAngleRotation(const Vec3f& axis, float angle);

Quatf greatCircleRotation(const Vec3f& a, const Vec3f& b);

inline Quatf conjugate(const Quatf& a) { return Quatf(-a.x, -a.y, -a.z, a.w); }

inline Vec3f rotate(const Quatf& q, const Vec3f v)
{
  auto t = mul(mul(conjugate(q), v), q);
  return Vec3f(t.x, t.y, t.z);
}

inline Vec3f rotate(const Vec3f v, const Quatf& q)
{
  auto t = mul(mul(q, v), conjugate(q));
  return Vec3f(t.x, t.y, t.z);
}


// Mat3f

Mat3f inverse(const Mat3f& M);

Mat3f mul(const Mat3f& A, const Mat3f& B);


float getScale(const Mat3f& M);

inline float getScale(const Mat3x4f& M) { return getScale(Mat3f(M.data)); }

inline Mat4f createIdentityMat4f() { Mat4f rv; for (unsigned j = 0; j < 4; j++) for (unsigned i = 0; i < 4; i++) rv.data[4 * j + i] = i == j ? 1.f : 0.f; return rv; }

inline Vec3f mul(const Mat3f& A, const Vec3f& x)
{
  Vec3f r;
  for (unsigned k = 0; k < 3; k++) {
    r.data[k] = A.data[k] * x.data[0] + A.data[3 + k] * x.data[1] + A.data[6 + k] * x.data[2];
  }
  return r;
}


inline Vec3f mul(const Mat3x4f& A, const Vec3f& x)
{
  Vec3f r;
  for (unsigned k = 0; k < 3; k++) {
    r.data[k] = A.data[k] * x.data[0] + A.data[3 + k] * x.data[1] + A.data[6 + k] * x.data[2] + A.data[9 + k];
  }
  return r;
}

// Mat4f

Mat4f mul(const Mat4f& A, const Mat4f& B);

inline Vec4f mul(const Mat4f& A, const Vec4f& p)
{
  return Vec4f(A.data[4 * 0 + 0] * p.x + A.data[4 * 1 + 0] * p.y + A.data[4 * 2 + 0] * p.z + A.data[4 * 3 + 0] * p.w,
               A.data[4 * 0 + 1] * p.x + A.data[4 * 1 + 1] * p.y + A.data[4 * 2 + 1] * p.z + A.data[4 * 3 + 1] * p.w,
               A.data[4 * 0 + 2] * p.x + A.data[4 * 1 + 2] * p.y + A.data[4 * 2 + 2] * p.z + A.data[4 * 3 + 2] * p.w,
               A.data[4 * 0 + 3] * p.x + A.data[4 * 1 + 3] * p.y + A.data[4 * 2 + 3] * p.z + A.data[4 * 3 + 3] * p.w);
}


// BBox3f

inline BBox3f createEmptyBBox3f()
{
  return BBox3f(Vec3f(FLT_MAX, FLT_MAX, FLT_MAX), Vec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX));
}

inline BBox3f::BBox3f(const BBox3f& bbox, float margin) :
  min(bbox.min - Vec3f(margin)),
  max(bbox.max + Vec3f(margin))
{}

inline void engulf(BBox3f& target, const Vec3f& p)
{
  target.min = min(target.min, p);
  target.max = max(target.max, p);
}

inline void engulf(BBox3f& target, const BBox3f& other)
{
  target.min = min(target.min, other.min);
  target.max = max(target.max, other.max);
}

BBox3f transform(const Mat3x4f& M, const BBox3f& bbox);

inline float diagonal(const BBox3f& b) { return distance(b.min, b.max); }

inline bool isEmpty(const BBox3f& b) { return b.max.x < b.min.x; }

inline bool isNotEmpty(const BBox3f& b) { return b.min.x <= b.max.x; }

inline float maxSideLength(const BBox3f& b)
{
  auto l = b.max - b.min;
  auto t = l.x > l.y ? l.x : l.y;
  return l.z > t ? l.z : t;
}

inline bool isStrictlyInside(const BBox3f& a, const BBox3f& b)
{
  auto lx = a.min.x <= b.min.x;
  auto ly = a.min.y <= b.min.y;
  auto lz = a.min.z <= b.min.z;
  auto ux = b.max.x <= a.max.x;
  auto uy = b.max.y <= a.max.y;
  auto uz = b.max.z <= a.max.z;
  return lx && ly && lz && ux && uy && uz;
}

inline bool isNotOverlapping(const BBox3f& a, const BBox3f& b)
{
  auto lx = b.max.x < a.min.x;
  auto ly = b.max.y < a.min.y;
  auto lz = b.max.z < a.min.z;
  auto ux = a.max.x < b.min.x;
  auto uy = a.max.y < b.min.y;
  auto uz = a.max.z < b.min.z;
  return lx || ly || lz || ux || uy || uz;
}

inline bool isOverlapping(const BBox3f& a, const BBox3f& b) { return !isNotOverlapping(a, b); }



Mat3f rotationMatrix(const Quatf& q);

Mat4f translationMatrix(const Vec3f& v);
