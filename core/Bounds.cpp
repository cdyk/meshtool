#include <cassert>
#include "LinAlgOps.h"

void boundingSphereNaive(Vec3f& center, float& radius, const Vec3f* P, size_t N)
{
  assert(N);

  Vec3f bbmin = P[0];
  Vec3f bbmax = P[0];
  for (size_t i = 1; i < N; i++) {
    bbmin = min(bbmin, P[i]);
    bbmax = max(bbmax, P[i]);
  }
  center = 0.5f*(bbmin + bbmax);
  radius = 0.5f*distance(bbmin, bbmax);

  for (size_t i = 0; i < N; i++) {
    assert(bbmin.x <= P[i].x);
    assert(bbmin.y <= P[i].y);
    assert(bbmin.z <= P[i].z);
    assert(P[i].x <= bbmax.x);
    assert(P[i].y <= bbmax.y);
    assert(P[i].z <= bbmax.z);

    auto r2 = distance(center, P[i]);
    assert(r2 <= 1.001f*radius);
  }

}


void boundingSphere(Vec3f& center, float& radius, const Vec3f* P, size_t N)
{
  assert(N);

  Vec3f E[6];
  for (size_t i = 0; i < 6; i++)
    E[i] = P[0];

  for (size_t i = 1; i < N; i++) {

    if (P[i].x < E[0].x) E[0] = P[i];
    if (E[1].x < P[i].x) E[1] = P[i];

    if (P[i].y < E[2].y) E[2] = P[i];
    if (E[3].y < P[i].y) E[3] = P[i];

    if (P[i].z < E[4].z) E[4] = P[i];
    if (E[5].z < P[i].z) E[5] = P[i];
  }

  // Find most separated pair
  auto c = 0.5f*(E[0] + E[1]);
  float d2 = distanceSquared(E[0], E[1]);

  for (size_t i = 1; i < 3; i++) {
    auto d2_ = distanceSquared(E[2 * i], E[2 * i + 1]);
    if (d2 < d2_) {
      d2 = d2_;
      c = 0.5f*(E[2*i] + E[2*i+1]);
    }
  }

  auto r = 0.5f*std::sqrt(d2);
  for (size_t i = 0; i < N; i++) {
    auto g2 = distanceSquared(c, P[i]);
    if (g2 <= r * r) continue;

    auto g = std::sqrt(g2);

    // back point is c + (c-P[i])*(r/g)
    // new center is 0.5*(c + (c-P[i])*(r/g) + P[i])
    //               0.5*(c + (r/g)c - (r/g)P[i] + P[i])
    //               0.5*( (1+(r/g))c + (1-(r/g))P[i])
    auto wc = 0.5f*(1.f + r / g);
    auto wq = 0.5f*(1.f - r / g);
    c = wc * c + wq * P[i];

    // new diameter is 2r + (g-r) = r+g, new radius is (r+g)/2.
    r = 0.5f*(r + g);
  }

  for (size_t i = 0; i < N; i++) {
    auto r2_i2 = distanceSquared(c, P[i]);
    assert(r2_i2 <= r * r * 1.001f);
  }


  center = c;
  radius = r;
}