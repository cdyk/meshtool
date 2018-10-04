#include "Common.h"

namespace {



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

}

bool readObj(Logger logger, const void * ptr, size_t size)
{
  unsigned line = 0;
  unsigned vertices = 0;

  auto * p = (const char*)(ptr);
  auto * end = p + size;
  while (p < end) {
    line++;
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
          vertices++;
          recognized = true;
          break;

        case key('p'):        // point primitive
        case key('l'):        // line primitive
        case key('f'):        // face primitive
        case key('v', 't'):
        case key('v', 'n'):
        case key('g'):        // g group_name1 group_name2
        case key('s'):        // s group_number
        case key('o'):        // o object_name

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
        logger(1, "Unrecognized keyword '%.*s' at line %d", q - p, p, line);
      }
    }

    p = skipNewLine(q, end);
  }



  logger(0, "readObj parsed %d lines, %d vertices", line, vertices);
  return false;
}
