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

#define BACKTRACE_MAX 100

void fatal_error(const char* fmt, ...) {
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%serror.log", Paths_GetDataPath());
    FILE* log_f = fopen(log_path, "a");

    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Fatal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    if (log_f) {
        va_list args_copy;
        va_copy(args_copy, args);
        fprintf(log_f, "Fatal error: ");
        vfprintf(log_f, fmt, args_copy);
        fprintf(log_f, "\n");
        va_end(args_copy);
    }

    va_end(args);
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
