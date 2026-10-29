#include <SDL3/SDL_main.h>

extern "C" int SF3_Main(int argc, const char *argv[]);

extern "C" int SDL_main(int argc, char *argv[])
{
    return SF3_Main(argc, (const char **)argv);
}
