#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>

typedef struct DebugRuntimeConfiguration {
    int enabled;
    int indexed_texture_path_enabled;
    int light_profile_enabled;
} DebugRuntimeConfiguration;

#if DEBUG
typedef struct TestRunnerConfiguration {
    bool enabled;
    const char* states_path;
} TestRunnerConfiguration;
#endif

typedef struct Configuration {
    DebugRuntimeConfiguration debug_runtime;
#if DEBUG
    TestRunnerConfiguration test;
#endif
} Configuration;

#endif
