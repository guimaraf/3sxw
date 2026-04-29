#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>

typedef struct NetplayConfiguration {
    int p2p_local_player;
    const char* p2p_remote_ip;
    const char* matchmaking_ip;
    int matchmaking_port;
} NetplayConfiguration;

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
    NetplayConfiguration netplay;
    DebugRuntimeConfiguration debug_runtime;
#if DEBUG
    TestRunnerConfiguration test;
#endif
} Configuration;

#endif
