#include "LinAlgOps.h"


Mat3f inverse(const Mat3f& M)
{
  const auto & c0 = M.cols[0];
  const auto & c1 = M.cols[1];
  const auto & c2 = M.cols[2];

  auto r0 = cross(c1, c2);
  auto r1 = cross(c2, c0);
  auto r2 = cross(c0, c1);

  auto invDet = 1.f / dot(r2, c2);

  return Mat3f(invDet * r0.x, invDet * r0.y, invDet*r0.z,
               invDet * r1.x, invDet * r1.y, invDet*r1.z,
               invDet * r2.x, invDet * r2.y, invDet*r2.z);
}

Mat3f mul(const Mat3f& A, const Mat3f& B)
{
  auto A00 = A.cols[0].x;  auto A10 = A.cols[0].y;  auto A20 = A.cols[0].z;
  auto A01 = A.cols[1].x;  auto A11 = A.cols[1].y;  auto A21 = A.cols[1].z;
  auto A02 = A.cols[2].x;  auto A12 = A.cols[2].y;  auto A22 = A.cols[2].z;

  auto B00 = B.cols[0].x;  auto B10 = B.cols[0].y;  auto B20 = B.cols[0].z;
  auto B01 = B.cols[1].x;  auto B11 = B.cols[1].y;  auto B21 = B.cols[1].z;
  auto B02 = B.cols[2].x;  auto B12 = B.cols[2].y;  auto B22 = B.cols[2].z;

  return Mat3f(A00 * B00 + A01 * B10 + A02 * B20,
               A00 * B01 + A01 * B11 + A02 * B21,
               A00 * B02 + A01 * B12 + A02 * B22,

               A10 * B00 + A11 * B10 + A12 * B20,
               A10 * B01 + A11 * B11 + A12 * B21,
               A10 * B02 + A11 * B12 + A12 * B22,

               A20 * B00 + A21 * B10 + A22 * B20,
               A20 * B01 + A21 * B11 + A22 * B21,
               A20 * B02 + A21 * B12 + A22 * B22);
}

Mat4f mul(const Mat4f& A, const Mat4f& B)
{
  auto A00 = A.cols[0].x;  auto A10 = A.cols[0].y;  auto A20 = A.cols[0].z;  auto A30 = A.cols[0].w;
  auto A01 = A.cols[1].x;  auto A11 = A.cols[1].y;  auto A21 = A.cols[1].z;  auto A31 = A.cols[1].w;
  auto A02 = A.cols[2].x;  auto A12 = A.cols[2].y;  auto A22 = A.cols[2].z;  auto A32 = A.cols[2].w;
  auto A03 = A.cols[3].x;  auto A13 = A.cols[3].y;  auto A23 = A.cols[3].z;  auto A33 = A.cols[3].w;

  auto B00 = B.cols[0].x;  auto B10 = B.cols[0].y;  auto B20 = B.cols[0].z;  auto B30 = B.cols[0].w;
  auto B01 = B.cols[1].x;  auto B11 = B.cols[1].y;  auto B21 = B.cols[1].z;  auto B31 = B.cols[1].w;
  auto B02 = B.cols[2].x;  auto B12 = B.cols[2].y;  auto B22 = B.cols[2].z;  auto B32 = B.cols[2].w;
  auto B03 = B.cols[3].x;  auto B13 = B.cols[3].y;  auto B23 = B.cols[3].z;  auto B33 = B.cols[3].w;

  return Mat4f(A00 * B00 + A01 * B10 + A02 * B20 + A03 * B30,
               A00 * B01 + A01 * B11 + A02 * B21 + A03 * B31,
               A00 * B02 + A01 * B12 + A02 * B22 + A03 * B32,
               A00 * B03 + A01 * B13 + A02 * B23 + A03 * B33,

               A10 * B00 + A11 * B10 + A12 * B20 + A13 * B30,
               A10 * B01 + A11 * B11 + A12 * B21 + A13 * B31,
               A10 * B02 + A11 * B12 + A12 * B22 + A13 * B32,
               A10 * B03 + A11 * B13 + A12 * B23 + A13 * B33,

               A20 * B00 + A21 * B10 + A22 * B20 + A23 * B30,
               A20 * B01 + A21 * B11 + A22 * B21 + A23 * B31,
               A20 * B02 + A21 * B12 + A22 * B22 + A23 * B32,
               A20 * B03 + A21 * B13 + A22 * B23 + A23 * B33,
               
               A30 * B00 + A31 * B10 + A32 * B20 + A33 * B30,
               A30 * B01 + A31 * B11 + A32 * B21 + A33 * B31,
               A30 * B02 + A31 * B12 + A32 * B22 + A33 * B32,
               A30 * B03 + A31 * B13 + A32 * B23 + A33 * B33);
}


float getScale(const Mat3f& M)
{
  float sx = length(M.cols[0]);
  float sy = length(M.cols[1]);
  float sz = length(M.cols[2]);
  auto t = sx > sy ? sx : sy;
  return sz > t ? sz : t;
}

void tangentSpaceBasis(Vec3f& u, Vec3f& v, const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec2f& t0, const Vec2f& t1, const Vec2f& t2)
{
  auto p01 = p1 - p0;
  auto p02 = p2 - p0;

  auto t01 = t1 - t0;
  auto t02 = t2 - t0;

  auto oneOverDet = 1.f/(t01.x * t02.y - t01.y*t02.x);

  for (unsigned k = 0; k < 3; k++) {
    u[k] = oneOverDet * (t01.y * p01[k] - t01.y * p02[k]);
    v[k] = oneOverDet * (-t02.x * p01[k] + t01.x * p02[k]);
  }
}


BBox3f transform(const Mat3x4f& M, const BBox3f& bbox)
{
  Vec3f p[8] = {
    mul(M, Vec3f(bbox.min.x, bbox.min.y, bbox.min.z)),
    mul(M, Vec3f(bbox.min.x, bbox.min.y, bbox.max.z)),
    mul(M, Vec3f(bbox.min.x, bbox.max.y, bbox.min.z)),
    mul(M, Vec3f(bbox.min.x, bbox.max.y, bbox.max.z)),
    mul(M, Vec3f(bbox.max.x, bbox.min.y, bbox.min.z)),
    mul(M, Vec3f(bbox.max.x, bbox.min.y, bbox.max.z)),
    mul(M, Vec3f(bbox.max.x, bbox.max.y, bbox.min.z)),
    mul(M, Vec3f(bbox.max.x, bbox.max.y, bbox.max.z))
  };
  return BBox3f(min(min(min(p[0], p[1]), min(p[2], p[3])), min(min(p[4], p[5]), min(p[6], p[7]))),
                max(max(max(p[0], p[1]), max(p[2], p[3])), max(max(p[4], p[5]), max(p[6], p[7]))));
}


Mat3f rotationMatrix(const Quatf& q)
{
  auto xx = q.x * q.x;
  auto xy = q.x * q.y;
  auto xz = q.x * q.z;
  auto xw = q.x * q.w;
  auto yy = q.y * q.y;
  auto yz = q.y * q.z;
  auto yw = q.y * q.w;
  auto zz = q.z * q.z;
  auto zw = q.z * q.w;

  Mat3f M;
  M.data[0] = 1.f - 2.f*(yy + zz);
  M.data[1] = 2.f*(xy - zw);
  M.data[2] = 2.f*(xz + yw);

  M.data[3] = 2.f*(xy + zw);
  M.data[4] = 1.f - 2.f*(xx + zz);
  M.data[5] = 2.f*(yz - xw);

  M.data[6] = 2.f*(xz - yw);
  M.data[7] = 2.f*(yz + xw);
  M.data[8] = 1.f - 2.f*(xx + yy);
  return M;
}

Mat4f translationMatrix(const Vec3f& v)
{
  Mat4f rv = createIdentityMat4f();
  rv.cols[3].x = v.x;
  rv.cols[3].y = v.y;
  rv.cols[3].z = v.z;
  return rv;
}

Quatf axisAngleRotation(const Vec3f& axis, float angle)
{
  auto c = std::cos(0.5f*angle);
  auto s = std::sin(0.5f*angle);
  return Quatf(s*axis.x, s*axis.y, s*axis.z, c);
}

Quatf greatCircleRotation(const Vec3f& a, const Vec3f& b)
{
  Vec3f axis = normalize(cross(a, b));
  if (std::isfinite(axis.x)) {
    auto angle = std::acos(dot(a, b));
    auto c = std::cos(0.5f*angle);
    auto s = std::sin(0.5f*angle);
    return Quatf(s*axis.x, s*axis.y, s*axis.z, c);
  }
  else {
    return Quatf(0.f, 0.f, 0.f, 1.f);
  }
}


Quatf mul(const Quatf& a, const Quatf& b)
{
  return Quatf(a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
               a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
               a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
               a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z);
}

Quatf mul(const Quatf& q, const Vec3f& v)
{
  // Assumes that the vector is a quaternion with zero scalar part.
  return mul(q, Quatf(v.x, v.y, v.z, 0));
}

Quatf mul(const Vec3f& v, const Quatf& q )
{
  // Assumes that the vector is a quaternion with zero scalar part.
  return mul(Quatf(v.x, v.y, v.z, 0), q);
}


