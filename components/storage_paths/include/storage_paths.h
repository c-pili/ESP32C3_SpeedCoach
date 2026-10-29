#pragma once

#include <stdbool.h>

/**
 * Centralized storage path definitions.
 * Used by main app, activity_store, and UI.
 */

#define STORAGE_MOUNT      "/spiffs"
#define ACTIVITY_DIR       STORAGE_MOUNT "/activities"
#define ACTIVITY_INDEX_PATH ACTIVITY_DIR "/index.csv"

typedef struct {
    bool mounted;
    const char *mount_point;
} activity_storage_t;
