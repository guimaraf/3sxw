#include "args.h"
#include "main.h"

#include "argparse/argparse.h"

void read_args(int argc, const char* argv[], Configuration* configuration) {
    struct argparse_option options[] = {
        OPT_HELP(),

        OPT_GROUP("Runtime diagnostics"),
        OPT_BOOLEAN(0, "debug-mode", &configuration->debug_runtime.enabled, "Enable runtime diagnostic mode.", NULL, 0, 0),
        OPT_BOOLEAN(0,
                    "debug-indexed-texture-path",
                    &configuration->debug_runtime.indexed_texture_path_enabled,
                    "Enable experimental indexed texture diagnostics.",
                    NULL,
                    0,
                    0),
        OPT_BOOLEAN(0,
                    "debug-light-profile",
                    &configuration->debug_runtime.light_profile_enabled,
                    "Enable low-overhead runtime profiling.",
                    NULL,
                    0,
                    0),

#if DEBUG
        OPT_GROUP("Test runner"),
        OPT_BOOLEAN(0, "test-enable", &configuration->test.enabled, "Enable test runner.", NULL, 0, 0),
        OPT_STRING(0, "test-states", &configuration->test.states_path, "Path to states.", NULL, 0, 0),
#endif

        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_parse(&argparse, argc, argv);

}
