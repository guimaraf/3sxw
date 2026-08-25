/*
 * This file is supposed to be used to build tests on platforms that require
 * the main function to be implemented in C++, which means that SDL_main's
 * implementation needs C++ and thus can't be included in test*.c
 *
 * Placed in the public domain by Daniel Gibson, 2022-12-12
 */

#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_WINRT
extern "C" int testdraw_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    return testdraw_main(argc, argv);
}
#else
// that's all, folks!
#endif
