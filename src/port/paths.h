#ifndef PORT_PATHS_H
#define PORT_PATHS_H

/// Get app directory path
///
/// This value shouldn't be freed after use
const char* Paths_GetPrefPath();

const char* Paths_GetBasePath();

/// Get data directory path
const char* Paths_GetDataPath();

#endif
