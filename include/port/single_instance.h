#ifndef PORT_SINGLE_INSTANCE_H
#define PORT_SINGLE_INSTANCE_H

#include <stddef.h>

typedef enum SingleInstanceResult {
    SINGLE_INSTANCE_ACQUIRED,
    SINGLE_INSTANCE_ALREADY_RUNNING,
    SINGLE_INSTANCE_ERROR,
} SingleInstanceResult;

SingleInstanceResult SingleInstance_Acquire(char* error, size_t error_size);
void SingleInstance_Release(void);

#endif
