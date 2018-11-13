struct Vertex
{
  float px, py, pz;
  uint8_t nx, ny, nz, pad;
  float16_t tu, tv;
  uint8_t r, g, b, a;
};

struct Meshlet
{
  vec3 center;
  float radius;
  uint offset;
  uint8_t vertexCount;
  uint8_t triangleCount;
};
