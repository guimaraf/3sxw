#ifndef PORT_RESOURCES_H
#define PORT_RESOURCES_H

#include <stdbool.h>

/// @brief Get path to a file in resources folder.
/// @param file_path Relative path to a file in resources, or `NULL` for path to the root of resources folder.
char* Resources_GetPath(const char* file_path);

bool Resources_Check();

typedef enum ResourcesFlowResult {
    RESOURCES_FLOW_IN_PROGRESS,
    RESOURCES_FLOW_READY,
    RESOURCES_FLOW_EXIT_REQUESTED,
} ResourcesFlowResult;

/// @brief Run resource copying flow. Repeated calls of this function progress the flow.
ResourcesFlowResult Resources_RunResourceCopyingFlow();

void Resources_Quit();

const char* Resources_GetAFSPath();

#endif
