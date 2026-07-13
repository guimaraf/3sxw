#include "port/single_instance.h"

#include <stdio.h>

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE instance_mutex = NULL;
#else
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int instance_lock_fd = -1;
#endif

SingleInstanceResult SingleInstance_Acquire(char* error, size_t error_size) {
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

#if _WIN32
    if (instance_mutex != NULL) {
        return SINGLE_INSTANCE_ACQUIRED;
    }

    instance_mutex =
        CreateMutexW(NULL, FALSE, L"Local\\3SXW.SingleInstance.6D591C12-80B2-48E5-A9AB-5809BC7D2768");

    if (instance_mutex == NULL) {
        if (error != NULL && error_size > 0) {
            snprintf(error,
                     error_size,
                     "CreateMutexW failed with Windows error %lu.",
                     (unsigned long)GetLastError());
        }

        return SINGLE_INSTANCE_ERROR;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(instance_mutex);
        instance_mutex = NULL;
        return SINGLE_INSTANCE_ALREADY_RUNNING;
    }

    return SINGLE_INSTANCE_ACQUIRED;
#else
    if (instance_lock_fd >= 0) {
        return SINGLE_INSTANCE_ACQUIRED;
    }

    const char* runtime_directory = getenv("XDG_RUNTIME_DIR");

    if (runtime_directory == NULL || runtime_directory[0] == '\0') {
        runtime_directory = "/tmp";
    }

    char lock_path[1024];
    const int path_length = snprintf(lock_path,
                                     sizeof(lock_path),
                                     "%s/3sxw-%lu.lock",
                                     runtime_directory,
                                     (unsigned long)getuid());

    if (path_length < 0 || (size_t)path_length >= sizeof(lock_path)) {
        if (error != NULL && error_size > 0) {
            snprintf(error, error_size, "The single-instance lock path is too long.");
        }

        return SINGLE_INSTANCE_ERROR;
    }

    int open_flags = O_RDWR | O_CREAT;

#ifdef O_CLOEXEC
    open_flags |= O_CLOEXEC;
#endif

#ifdef O_NOFOLLOW
    open_flags |= O_NOFOLLOW;
#endif

    instance_lock_fd = open(lock_path, open_flags, 0600);

    if (instance_lock_fd < 0) {
        if (error != NULL && error_size > 0) {
            snprintf(error, error_size, "Couldn't open %s: %s", lock_path, strerror(errno));
        }

        return SINGLE_INSTANCE_ERROR;
    }

    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
    };

    if (fcntl(instance_lock_fd, F_SETLK, &lock) != 0) {
        const int lock_error = errno;
        close(instance_lock_fd);
        instance_lock_fd = -1;

        if (lock_error == EACCES || lock_error == EAGAIN) {
            return SINGLE_INSTANCE_ALREADY_RUNNING;
        }

        if (error != NULL && error_size > 0) {
            snprintf(error, error_size, "Couldn't lock %s: %s", lock_path, strerror(lock_error));
        }

        return SINGLE_INSTANCE_ERROR;
    }

    return SINGLE_INSTANCE_ACQUIRED;
#endif
}

void SingleInstance_Release(void) {
#if _WIN32
    if (instance_mutex != NULL) {
        CloseHandle(instance_mutex);
        instance_mutex = NULL;
    }
#else
    if (instance_lock_fd >= 0) {
        struct flock lock = {
            .l_type = F_UNLCK,
            .l_whence = SEEK_SET,
            .l_start = 0,
            .l_len = 0,
        };

        fcntl(instance_lock_fd, F_SETLK, &lock);
        close(instance_lock_fd);
        instance_lock_fd = -1;
    }
#endif
}
