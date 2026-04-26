#ifndef PORT_DEBUG_DEBUG_LOG_H
#define PORT_DEBUG_DEBUG_LOG_H

#include <stdbool.h>

void DebugLog_Init(int enabled, int argc, const char* argv[]);
void DebugLog_Shutdown();

bool DebugLog_IsEnabled();
const char* DebugLog_GetSessionPath();
void DebugLog_Write(const char* file_name, const char* text);
void DebugLog_Printf(const char* file_name, const char* format, ...);

#endif
