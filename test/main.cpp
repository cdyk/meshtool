#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cctype>
#include <cstdio>


namespace {

  void logger(unsigned level, const char* msg, ...)
  {
    switch (level) {
    case 0: fprintf(stderr, "[I] "); break;
    case 1: fprintf(stderr, "[W] "); break;
    case 2: fprintf(stderr, "[E] "); break;
    }

    va_list argptr;
    va_start(argptr, msg);
    vfprintf(stderr, msg, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
  }


}


int main(int argc, char** argv)
{
  return 0;
}
