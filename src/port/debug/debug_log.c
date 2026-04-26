#include "port/debug/debug_log.h"
#include "port/paths.h"

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static bool debug_log_enabled = false;
static bool debug_log_initialized = false;
static char* debug_log_session_path = NULL;

static bool get_local_time(struct tm* local_time) {
    const time_t now = time(NULL);

    if (now == (time_t)-1) {
        return false;
    }

#if _WIN32
    return localtime_s(local_time, &now) == 0;
#else
    return localtime_r(&now, local_time) != NULL;
#endif
}

static bool ensure_directory(const char* path) {
    SDL_PathInfo path_info;

    if (SDL_GetPathInfo(path, &path_info)) {
        return path_info.type == SDL_PATHTYPE_DIRECTORY;
    }

    return SDL_CreateDirectory(path);
}

static void write_file_v(const char* file_name, const char* mode, const char* format, va_list args) {
    if (!debug_log_enabled || debug_log_session_path == NULL) {
        return;
    }

    char* file_path = NULL;
    SDL_asprintf(&file_path, "%s%s", debug_log_session_path, file_name);

    FILE* file = fopen(file_path, mode);

    if (file == NULL) {
        SDL_Log("Failed to open debug log file: %s", file_path);
        SDL_free(file_path);
        return;
    }

    vfprintf(file, format, args);
    fclose(file);
    SDL_free(file_path);
}

static void write_file(const char* file_name, const char* mode, const char* format, ...) {
    va_list args;
    va_start(args, format);
    write_file_v(file_name, mode, format, args);
    va_end(args);
}

static void format_timestamp(const struct tm* local_time, char* timestamp, size_t timestamp_size) {
    if (strftime(timestamp, timestamp_size, "%Y-%m-%d %H:%M:%S", local_time) == 0) {
        SDL_snprintf(timestamp, timestamp_size, "unknown-time");
    }
}

static void write_session_file(const char* started_at, int argc, const char* command_line) {
    write_file("session.txt", "w", "debug_mode=1\n");
    write_file("session.txt", "a", "started_at=%s\n", started_at);
    write_file("session.txt", "a", "build_date=%s\n", __DATE__);
    write_file("session.txt", "a", "build_time=%s\n", __TIME__);
    write_file("session.txt", "a", "base_path=%s\n", Paths_GetBasePath());
    write_file("session.txt", "a", "data_path=%s\n", Paths_GetDataPath());
    write_file("session.txt", "a", "session_path=%s\n", debug_log_session_path);
    write_file("session.txt", "a", "argc=%d\n", argc);
    write_file("session.txt", "a", "argv=%s\n", command_line != NULL ? command_line : "");
}

void DebugLog_Init(int enabled, int argc, const char* command_line) {
    if (debug_log_initialized) {
        return;
    }

    debug_log_initialized = true;

    if (!enabled) {
        return;
    }

    const char* data_path = Paths_GetDataPath();
    char* debug_path = NULL;
    SDL_asprintf(&debug_path, "%sdebug/", data_path);

    if (!ensure_directory(debug_path)) {
        SDL_Log("Failed to create debug directory: %s", debug_path);
        SDL_free(debug_path);
        return;
    }

    struct tm local_time = { 0 };
    char session_timestamp[32] = { 0 };
    char started_at[32] = { 0 };

    if (get_local_time(&local_time)) {
        strftime(session_timestamp, sizeof(session_timestamp), "%Y%m%d-%H%M%S", &local_time);
        format_timestamp(&local_time, started_at, sizeof(started_at));
    } else {
        SDL_snprintf(session_timestamp, sizeof(session_timestamp), "unknown-time");
        SDL_snprintf(started_at, sizeof(started_at), "unknown-time");
    }

    SDL_asprintf(&debug_log_session_path, "%ssession-%s/", debug_path, session_timestamp);
    SDL_free(debug_path);

    if (!ensure_directory(debug_log_session_path)) {
        SDL_Log("Failed to create debug session directory: %s", debug_log_session_path);
        SDL_free(debug_log_session_path);
        debug_log_session_path = NULL;
        return;
    }

    debug_log_enabled = true;
    write_session_file(started_at, argc, command_line);
}

void DebugLog_Shutdown() {
    debug_log_enabled = false;
    debug_log_initialized = false;

    if (debug_log_session_path != NULL) {
        SDL_free(debug_log_session_path);
        debug_log_session_path = NULL;
    }
}

bool DebugLog_IsEnabled() {
    return debug_log_enabled;
}

const char* DebugLog_GetSessionPath() {
    return debug_log_session_path;
}

void DebugLog_Write(const char* file_name, const char* text) {
    DebugLog_Printf(file_name, "%s", text);
}

void DebugLog_Printf(const char* file_name, const char* format, ...) {
    va_list args;
    va_start(args, format);
    write_file_v(file_name, "a", format, args);
    va_end(args);
}

void DebugLog_PrintSession(const char* format, ...) {
    va_list args;
    va_start(args, format);
    write_file_v("session.txt", "a", format, args);
    va_end(args);
}
