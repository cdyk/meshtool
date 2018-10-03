#pragma once

struct Vec2f
{
  Vec2f() = default;
  Vec2f(const Vec2f&) = default;
  Vec2f(float x) : x(x), y(x) {}
  Vec2f(float* ptr) : x(ptr[0]), y(ptr[1]) {}
  Vec2f(float x, float y) : x(x), y(y) {}
  union {
    struct {
      float x;
      float y;
    };
    float data[2];
  };
  float& operator[](unsigned i) { return data[i]; }
  const float& operator[](unsigned i) const { return data[i]; }
};


struct Vec3f
{
  Vec3f() = default;
  Vec3f(const Vec3f&) = default;
  Vec3f(float x) : x(x), y(x), z(x) {}
  Vec3f(float* ptr) : x(ptr[0]), y(ptr[1]), z(ptr[2]) {}
  Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}
  Vec3f(const Vec2f& a, float z) : x(a.x), y(a.y), z(z) {}

  union {
    struct {
      float x;
      float y;
      float z;
    };
    float data[3];
  };
  float& operator[](unsigned i) { return data[i]; }
  const float& operator[](unsigned i) const { return data[i]; }
};

struct Vec4f
{
  union {
    struct {
      float x;
      float y;
      float z;
      float w;
    };
    float data[4];
  };

};

struct Quatf
{
  Quatf() = default;
  Quatf(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

  union {
    struct {
      float w;
      float x;
      float y;
      float z;
    };
    float data[4];
  };
};


struct BBox3f
{
  BBox3f() = default;
  BBox3f(const BBox3f&) = default;
  BBox3f(const Vec3f& min, const Vec3f& max) : min(min), max(max) {}
  BBox3f(const BBox3f& bbox, float margin);

  union {
    struct {
      Vec3f min;
      Vec3f max;
    };
    float data[6];
  };
};

struct Mat3f
{
  Mat3f() = default;
  Mat3f(const Mat3f&) = default;
  Mat3f(const float* ptr) { for (unsigned i = 0; i < 3 * 3; i++) data[i] = ptr[i]; }
  Mat3f(float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22) :
    m00(m00), m10(m10), m20(m20),
    m01(m01), m11(m11), m21(m21),
    m02(m02), m12(m12), m22(m22)
  {}


  union {
    struct {
      float m00;
      float m10;
      float m20;
      float m01;
      float m11;
      float m21;
      float m02;
      float m12;
      float m22;
    };
    Vec3f cols[3];
    float data[3 * 3];
  };
};


struct Mat3x4f
{
  Mat3x4f() = default;
  Mat3x4f(const Mat3x4f&) = default;
  Mat3x4f(const float* ptr) { for (unsigned i = 0; i < 4 * 3; i++) data[i] = ptr[i]; }

  union {
    struct {
      float m00;
      float m10;
      float m20;

      float m01;
      float m11;
      float m21;

      float m02;
      float m12;
      float m22;

      float m03;
      float m13;
      float m23;
    };
    Vec3f cols[4];
    float data[4 * 3];
  };
};

struct Mat4f
{
  Mat4f() = default;
  Mat4f(const Mat4f&) = default;
  Mat4f(const Mat3f& M) :
    m00(M.m00), m10(M.m10), m20(M.m20), m30(0.f),
    m01(M.m01), m11(M.m11), m21(M.m21), m31(0.f),
    m02(M.m02), m12(M.m12), m22(M.m22), m32(0.f),
    m03(0.f), m13(0.f), m23(0.f), m33(1.f)
  {}

  Mat4f(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) :
    m00(m00), m10(m10), m20(m20), m30(m30),
    m01(m01), m11(m11), m21(m21), m31(m31),
    m02(m02), m12(m12), m22(m22), m32(m32),
    m03(m03), m13(m13), m23(m23), m33(m33)
  {}


  union {
    struct {
      float m00;
      float m10;
      float m20;
      float m30;

      float m01;
      float m11;
      float m21;
      float m31;

      float m02;
      float m12;
      float m22;
      float m32;

      float m03;
      float m13;
      float m23;
      float m33;
    };
    Vec4f cols[4];
    float data[4 * 4];
  };

};