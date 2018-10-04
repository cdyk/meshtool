#include <cassert>
#include "Common.h"
#include "LinAlg.h"
#include "LinAlgOps.h"
#include "Mesh.h"

namespace {

  template<typename Element>
  struct Block
  {
    static const unsigned capacity = 1024;
    struct Block<Element>* next = nullptr;
    unsigned fill = 0;
    Element data[capacity];
  };

  struct Triangle
  {
    uint32_t vtx[3];
    uint32_t nrm[3];
    uint32_t tex[3];
    uint32_t object;
    uint32_t color;
  };

  struct Line
  {
    uint32_t vtx[2];
    uint32_t object;
    uint32_t color;
  };

  struct Object
  {
    Object* next = nullptr;
    const char* str;
  };

  struct Context
  {
    Logger logger = nullptr;
    uint32_t line = 1;
    uint32_t currentObject = 0;
    uint32_t currentColor = 0x888888;
    Arena arena;
    StringInterning strings;
    Map objectId;

    ListHeader<Block<Vec3f>> vertices;
    ListHeader<Block<Vec3f>> normals;
    ListHeader<Block<Vec2f>> texcoords;
    ListHeader<Block<Triangle>> triangles;
    ListHeader<Block<Line>> lines;

    ListHeader<Object> objects;


    uint32_t vertices_n = 0;
    uint32_t normals_n = 0;
    uint32_t texcoords_n = 0;
    uint32_t triangles_n = 0;
    uint32_t objects_n = 0;

    bool useNormals = false;
    bool useTexcoords = false;
  };


  const char* endOfine(const char* p, const char* end)
  {
    while (p < end && (*p != '#' && *p != '\n' && *p != '\r'))  p++;
    return p;
  }

  const char* skipNewLine(const char* p, const char* end)
  {
    if (p < end && *p == '#') {
      while (p < end && (*p != '\n' && *p != '\r'))  p++;
    }
    if (p < end && (*p == '\r'))  p++;
    if (p < end && (*p == '\n'))  p++;
    return p;
  }

  const char* skipSpacing(const char* p, const char* end)
  {
    while (p < end && (*p=='\r' || *p == ' ' || *p == '\f' || *p == '\t' || *p == '\v')) p++;
    return p;
  }

  const char* skipNonSpacing(const char* p, const char* end)
  {
    while (p < end && (*p != '\r' && *p != ' ' && *p != '\f' && *p != '\t' && *p != '\v')) p++;
    return p;
  }

  
  constexpr unsigned key(const char a)
  {
    return a;
  }

  constexpr unsigned key(const char a, const char b)
  {
    return (a << 8) | b;
  }

  constexpr unsigned key(const char a, const char b, const char c)
  {
    return (a << 16) | (b<<8) | c;
  }

  constexpr unsigned key(const char a, const char b, const char c, const char d)
  {
    return (a << 24) | (b << 16) | (c<<8) | d;
  }

  unsigned key(const char *p, size_t N)
  {
    unsigned rv = 0;
    for (size_t i = 0; i < N; i++) {
      rv = (rv << 8) | p[i];
    }
    return rv;
  }

  const char* parseInt(Context* context, int32_t& value, const char* p, const char* end)
  {
    p = skipSpacing(p, end);

    int32_t sign = 1;
    if (p < end) {
      if (*p == '-') { sign = -1; p++; }
      else if (*p == '+') p++;
    }

    int64_t t = 0;
    for (; p < end && '0' <= *p && *p <= '9'; p++) {
      if (t < 0x100000000) {
        t = 10 * t + (*p - '0');
      }
    }
    t = sign * t;

    if (t < std::numeric_limits<int32_t>::min()) value = std::numeric_limits<int>::min();
    else if (std::numeric_limits<int32_t>::max() < t) value = std::numeric_limits<int>::max();
    else value = int32_t(t);

    return p;
  }

  const char* parseFloat(Context* context, float& value, const char* p, const char* end)
  {
    auto *s = p;

    // Note: no attempt on correct rounding.

    // read sign
    int32_t sign = 1;
    if (p < end) {
      if (*p == '-') { sign = -1; p++; }
      else if (*p == '+') p++;
    }

    int32_t mantissa = 0;
    int32_t exponent = 0;
    for (; p < end && '0' <= *p && *p <= '9'; p++) {
      if (mantissa < 100000000) {
        mantissa = 10 * mantissa + (*p - '0');
      }
      else {
        exponent++;
      }
    }
    if (p < end && *p == '.') {
      p++;
      for (; p < end && '0' <= *p && *p <= '9'; p++) {
        if (mantissa < 100000000) {
          mantissa = 10 * mantissa + (*p - '0');
          exponent--;
        }
      }
    }
    if (p < end && *p == 'e') {
      p++;
      int32_t expSign = 1;
      if (p < end) {
        if (*p == '-') { expSign = -1; p++; }
        else if (*p == '+') p++;
      }

      int32_t exp = 0;
      for (; p < end && '0' <= *p && *p <= '9'; p++) {
        if (exp < 100000000) {
          exp = 10 * exp + (*p - '0');
        }
      }

      exponent += expSign * exp;
    }
    value = float(sign*mantissa)*pow(10.f, exponent);
    return p;
  }


  template<unsigned d, typename Element>
  void parseVec(Context* context, ListHeader<Block<Element>>& V, uint32_t& N,  const char * a, const char* b)
  {
    if (V.empty() || V.last->capacity <= V.last->fill + 1) {
      V.append(context->arena.alloc<Block<Element>>());
    }
    assert(V.last->fill < V.last->capacity);
    auto * v = V.last->data[V.last->fill++].data;
    unsigned i = 0;
    for (; a < b && i < d; i++) {
      a = skipSpacing(a, b);
      auto * q = a;
      a = parseFloat(context, v[i], a, b);
    }
    for (; i < d; i++) v[i] = 0.f;
    N++;
  }

  void parseF(Context* context, const char* a, const char* b)
  {
    int ix;
    Triangle t;

    unsigned k = 0;
    for (; a < b && k < 3; k++) {
      t.tex[k] = ~0u;
      t.nrm[k] = ~0u;

      a = skipSpacing(a, b);
      a = parseInt(context, ix, a, b);
      if (ix < 0) ix += context->vertices_n;
      else --ix;
      if (ix < 0 || context->vertices_n <= uint32_t(ix)) {
        context->logger(2, "Illegal vertex index %d at line %d.", ix, context->line);
        return;
      }
      t.vtx[k] = ix;

      if (a < b && *a == '/') {
        a++;
        if (a < b && *a != '/' && *a != ' ') {
          a = parseInt(context, ix, a, b);
          if (ix < 0) ix += context->texcoords_n;
          else --ix;
          if (ix < 0 || context->texcoords_n <= uint32_t(ix)) {
            context->logger(2, "Illegal texcoord index %d at line %d.", ix, context->line);
            return;
          }
          t.tex[k] = ix;
          context->useTexcoords = true;
        }
        if (a < b && *a == '/' && *a != ' ') {
          a++;
          a = parseInt(context, ix, a, b);
          if (ix < 0) ix += context->normals_n;
          else --ix;
          if (ix < 0 || context->normals_n <= uint32_t(ix)) {
            context->logger(2, "Illegal normal index %d at line %d.", ix, context->line);
            return;
          }
          t.nrm[k] = ix;
          context->useNormals = true;
        }
      }
    }
    if (k == 3) {
      auto & T = context->triangles;
      if(T.empty() || T.last->capacity <= T.last->fill + 1) {
        T.append(context->arena.alloc<Block<Triangle>>());
      }
      T.last->data[T.last->fill++] = t;
      context->triangles_n++;
    }
    else {
      context->logger(2, "Skipped malformed triangle at line %d", context->line);
    }
  }

  void parseO(Context* context, const char* a, const char* b)
  {
    a = skipSpacing(a, b);
    auto * m = skipNonSpacing(a, b);
    auto * str = context->strings.intern(a, m);

    uint64_t id;
    if (context->objectId.get(id, uint64_t(str))) {
    }
    else {
      id = context->objects_n++;
      context->objectId.insert(uint64_t(str), id);

      auto * obj = context->arena.alloc<Object>();
      obj->str = str;
      context->objects.append(obj);
    }
    context->currentObject = uint32_t(id);
  }

}

Mesh*  readObj(Logger logger, const void * ptr, size_t size)
{
  Context context;
  context.logger = logger;
  context.line = 1;



  auto * p = (const char*)(ptr);
  auto * end = p + size;
  while (p < end) {
    context.line++;
    p = skipSpacing(p, end);        // skip inital spaces on line
    auto *q = endOfine(p, end);     // get end of line, before newline etc.

    if (p < q) {
      auto * r = skipNonSpacing(p, end);  // Find keyword
      auto l = r - p;                     // length of keyword

      bool recognized = false;
      if (l <= 4) {
        unsigned keyword = key(p, l);
        switch (keyword) {
        case key('v'):
          parseVec<3>(&context, context.vertices, context.vertices_n, r, q);
          recognized = true;
          break;

        case key('v', 'n'):
          parseVec<3>(&context, context.normals, context.normals_n, r, q);
          recognized = true;
          break;

        case key('v', 't'):
          parseVec<2>(&context, context.texcoords, context.texcoords_n, r, q);
          recognized = true;
          break;

        case key('f'):        // face primitive
          parseF(&context, r, q);
          recognized = true;
          break;

        case key('o'):        // o object_name
          parseO(&context, r, q);
          recognized = true;
          break;

        case key('p'):        // point primitive
        case key('l'):        // line primitive
        case key('g'):        // g group_name1 group_name2
        case key('s'):        // s group_number

        case key('m', 'g'):
        case key('v', 'p'):
        case key('d', 'e', 'g'):
        case key('b', 'm', 'a', 't'):
        case key('s', 't', 'e', 'p'):
        case key('c', 'u', 'r', 'v'):
        case key('s', 'u', 'r', 'f'):
        case key('t', 'r', 'i', 'm'):
        case key('h', 'o', 'l', 'e'):
        case key('s', 'p'):
        case key('e', 'n', 'd'):
        case key('c', 'o', 'n'):
        case key('l', 'o', 'd'):

          recognized = true;
          break;
        }
      }
      else {
        if (l == 5 && std::memcmp(p, "curv2", 5)) recognized = true;
        else if (l == 5 && std::memcmp(p, "ctech", 5)) recognized = true;
        else if (l == 5 && std::memcmp(p, "stech", 5)) recognized = true;
        else if (l == 5 && std::memcmp(p, "bevel", 5)) recognized = true;
        else if (l == 6 && std::memcmp(p, "cstype", 6)) recognized = true;
        else if (l == 6 && std::memcmp(p, "maplib", 6)) recognized = true;
        else if (l == 6 && std::memcmp(p, "usemap", 6)) recognized = true;
        else if (l == 6 && std::memcmp(p, "usemtl", 6)) recognized = true;
        else if (l == 6 && std::memcmp(p, "mtllib", 6)) recognized = true;
        else if (l == 8 && std::memcmp(p, "c_interp", 8)) recognized = true;
        else if (l == 8 && std::memcmp(p, "d_interp", 8)) recognized = true;
        else if (l == 9 && std::memcmp(p, "trace_obj", 9)) recognized = true;
        else if (l == 10 && std::memcmp(p, "shadow_obj", 10)) recognized = true;
      }

      if (recognized == false) {
        logger(1, "Unrecognized keyword '%.*s' at line %d", q - p, p, context.line);
      }
    }

    p = skipNewLine(q, end);
  }


  auto * mesh = new Mesh();

  {
    BBox3f bbox = createEmptyBBox3f();
    unsigned o = 0;
    mesh->vtx_n = context.vertices_n;
    mesh->vtx = (Vec3f*)mesh->arena.alloc(sizeof(Vec3f)*mesh->vtx_n);

    for (auto * block = context.vertices.first; block; block = block->next) {

      for (unsigned i = 0; i < block->fill; i++) {
        engulf(bbox, block->data[i]);
        mesh->vtx[o++] = block->data[i];
      }
    }
    assert(o == mesh->vtx_n);
    mesh->bbox = bbox;
  }

  {
    unsigned o = 0;
    mesh->tri_n = context.triangles_n;
    mesh->tri = (uint32_t*)mesh->arena.alloc(sizeof(uint32_t) * 3 * mesh->tri_n);
    for (auto * block = context.triangles.first; block; block = block->next) {
      for (unsigned i = 0; i < block->fill; i++) {
        for (unsigned k = 0; k < 3; k++) {
          auto ix = block->data[i].vtx[k];
          assert(0 <= ix && ix < context.vertices_n);
          mesh->tri[o++] = ix;
        }
      }
    }
    assert(o == 3*mesh->tri_n);
  }

  {
    unsigned o = 0;
    mesh->obj_n = context.objects_n;
    mesh->obj = (const char**)mesh->arena.alloc(sizeof(const char*)*mesh->obj_n);
    for (auto * obj = context.objects.first; obj; obj = obj->next) {
      mesh->obj[o++] = mesh->strings.intern(obj->str);
    }
    assert(o == mesh->obj_n);
  }

  logger(0, "readObj parsed %d lines, Vn=%d, Nn=%d, Tn=%d, tris=%d",
         context.line, context.vertices_n, context.normals_n, context.texcoords_n, context.triangles_n);
  return mesh;
}
