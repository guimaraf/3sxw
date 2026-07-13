#include "port/utils.h"
#include "port/paths.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#define SYMBOL_NAME_MAX 256
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#define BACKTRACE_MAX 100

static void write_error_log(const char* prefix, const char* message) {
    fprintf(stderr, "%s: %s\n", prefix, message);

    char log_path[512];
    FILE* log_f = NULL;
    const char* data_path = Paths_GetDataPath();

    if (data_path != NULL) {
        snprintf(log_path, sizeof(log_path), "%serror.log", data_path);
        log_f = fopen(log_path, "a");
    }

    if (log_f != NULL) {
        fprintf(log_f, "%s: %s\n", prefix, message);
        fclose(log_f);
    }
}

static void report_critical_error(const char* fmt, va_list args) {
    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);

    write_error_log("Critical error", message);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "3SX - Critical error", message, NULL);
}

void log_error(const char* fmt, ...) {
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    write_error_log("Error", message);
}

void critical_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    report_critical_error(fmt, args);
    va_end(args);
}

void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    report_critical_error(fmt, args);
    va_end(args);

    const char* data_path = Paths_GetDataPath();
    char log_path[512];
    FILE* log_f = NULL;

    if (data_path != NULL) {
        snprintf(log_path, sizeof(log_path), "%serror.log", data_path);
        log_f = fopen(log_path, "a");
    }

    void* buffer[BACKTRACE_MAX];

#if !_WIN32
    int nptrs = backtrace(buffer, BACKTRACE_MAX);
    fprintf(stderr, "Stack trace:\n");
    if (log_f) fprintf(log_f, "Stack trace:\n");
    backtrace_symbols_fd(buffer, nptrs, fileno(stderr));
    if (log_f) backtrace_symbols_fd(buffer, nptrs, fileno(log_f));
#else
    fprintf(stderr, "Stack trace:\n");
    if (log_f) fprintf(log_f, "Stack trace:\n");
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    int nptrs = CaptureStackBackTrace(0, BACKTRACE_MAX, buffer, NULL);
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + SYMBOL_NAME_MAX);
    if (!symbol) {
        fprintf(stderr, "Calloc failed when allocating SYMBOL_INFO, bailing!\n\n");
        if (log_f) fprintf(log_f, "Calloc failed when allocating SYMBOL_INFO, bailing!\n\n");
        SymCleanup(process);
        if (log_f) fclose(log_f);
        abort();
    }
    symbol->MaxNameLen = SYMBOL_NAME_MAX;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    for (int i = 0; i < nptrs; i++) {
        SymFromAddr(process, (DWORD64)buffer[i], 0, symbol);
        fprintf(stderr, "%i: %s - 0x%0llX\n", nptrs - i - 1, symbol->Name, symbol->Address);
        if (log_f) fprintf(log_f, "%i: %s - 0x%0llX\n", nptrs - i - 1, symbol->Name, symbol->Address);
    }
    free(symbol);
    SymCleanup(process);
#endif

    if (log_f) fclose(log_f);
    abort();
}


void not_implemented(const char* func) {
    fatal_error("Function not implemented: %s\n", func);
}

void debug_print(const char* fmt, ...) {
#if DEBUG
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
#endif
}

void stop_if(bool condition) {
#if DEBUG
    if (!condition) {
        return;
    }

#if _WIN32
    __debugbreak();
#else
    raise(SIGSTOP);
#endif
#endif
}
