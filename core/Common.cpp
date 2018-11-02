#include "Common.h"
#include <cstdlib>
#include <algorithm>
#include <cassert>

namespace {

  template<typename T>
  constexpr bool isPow2(T x)
  {
    return x != 0 && (x & (x - 1)) == 0;
  }


  uint64_t hash_uint64(uint64_t x)
  {
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 32;
    return x;
  }

}

uint64_t fnv_1a(const char* bytes, size_t l)
{
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < l; i++) {
    hash = hash ^ bytes[i];
    hash = hash * 0x100000001B3;
  }
  return hash;
}





void BufferBase::free()
{
  if (ptr) ::free(ptr - sizeof(size_t));
  ptr = nullptr;
}

Map::~Map()
{
  free(keys);
  free(vals);
}

void Map::clear()
{
  for (unsigned i = 0; i < capacity; i++) {
    keys[i] = 0;
    vals[i] = 0;
  }
  fill = 0;
}

bool Map::get(uint64_t& val, uint64_t key)
{
  assert(key != 0);
  if (fill == 0) return false;

  auto mask = capacity - 1;
  for (auto i = size_t(hash_uint64(key)); true; i++) { // linear probing
    i = i & mask;
    if (keys[i] == key) {
      val = vals[i];
      return true;
    }
    else if (keys[i] == 0) {
      return false;
    }
  }
}

uint64_t Map::get(uint64_t key)
{
  uint64_t rv = 0;
  get(rv, key);
  return rv;
}


void Map::insert(uint64_t key, uint64_t value)
{
  assert(key != 0);     // null is used to denote no-key
  //assert(value != 0);   // null value is used to denote not found

  if (capacity <= 2 * fill) {
    auto old_capacity = capacity;
    auto old_keys = keys;
    auto old_vals = vals;

    fill = 0;
    capacity = capacity ? 2 * capacity : 16;
    keys = (uint64_t*)xcalloc(capacity, sizeof(uint64_t));
    vals = (uint64_t*)xmalloc(capacity * sizeof(uint64_t));

    unsigned g = 0;
    for (size_t i = 0; i < old_capacity; i++) {
      if (old_keys[i]) {
        insert(old_keys[i], old_vals[i]);
        g++;
      }
    }

    free(old_keys);
    free(old_vals);
  }

  assert(isPow2(capacity));
  auto mask = capacity - 1;
  for (auto i = size_t(hash_uint64(key)); true; i++) { // linear probing
    i = i & mask;
    if (keys[i] == key) {
      vals[i] = value;
      break;
    }
    else if (keys[i] == 0) {
      keys[i] = key;
      vals[i] = value;
      fill++;
      break;
    }
  }

}

namespace {

  struct StringHeader
  {
    StringHeader* next;
    size_t length;
    char string[1];
  };

}

const char* StringInterning::intern(const char* str)
{
  return intern(str, str + strlen(str));
}

const char* StringInterning::intern(const char* a, const char* b)
{
  auto length = b - a;
  auto hash = fnv_1a(a, length);
  hash = hash ? hash : 1;

  auto * intern = (StringHeader*)map.get(hash);
  for (auto * it = intern; it != nullptr; it = it->next) {
    if (it->length == length) {
      if (strncmp(it->string, a, length) == 0) {
        return it->string;
      }
    }
  }

  auto * newIntern = (StringHeader*)arena.alloc(sizeof(StringHeader) + length);
  newIntern->next = intern;
  newIntern->length = length;
  std::memcpy(newIntern->string, a, length);
  newIntern->string[length] = '\0';
  map.insert(hash, uint64_t(newIntern));
  return newIntern->string;
}