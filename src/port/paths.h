#ifndef PORT_PATHS_H
#define PORT_PATHS_H

#include <stdbool.h>
#include <stddef.h>

/// Get app directory path
///
/// This value shouldn't be freed after use
const char* Paths_GetPrefPath();

const char* Paths_GetBasePath();

/// Get data directory path
const char* Paths_GetDataPath();

/// Validate that the portable data directory supports create, write, read and delete operations.
bool Paths_ValidatePortableStorage(char* error, size_t error_size);

#endif
