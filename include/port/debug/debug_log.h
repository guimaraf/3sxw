#ifndef PORT_DEBUG_DEBUG_LOG_H
#define PORT_DEBUG_DEBUG_LOG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct DebugFrameTiming {
    uint64_t frame;
    double total_ms;
    double poll_ms;
    double begin_ms;
    double game0_ms;
    double end_ms;
    double game1_ms;
    double sleep_ms;
    int late_flag;
} DebugFrameTiming;

void DebugLog_Init(int enabled, int argc, const char* command_line);
void DebugLog_Shutdown();

bool DebugLog_IsEnabled();
const char* DebugLog_GetSessionPath();
void DebugLog_Write(const char* file_name, const char* text);
void DebugLog_Printf(const char* file_name, const char* format, ...);
void DebugLog_PrintSession(const char* format, ...);
void DebugLog_RecordFrameTiming(const DebugFrameTiming* timing);

#endif
