#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* name;
    char* desktop_path;
    uint8_t* icon_data;
    int32_t icon_size;
} AppItemStruct;

// Initialize the app launcher
void launcher_init();

// Get the list of installed apps.
// Returns a pointer to an array of AppItemStruct.
// Sets *count to the number of items.
// If client_version matches cache_version, returns NULL and sets count to 0.
// Updates *client_version to the current cache version.
AppItemStruct* launcher_get_installed_apps(int32_t* count, uint64_t* client_version);

// Launch an app by its desktop file path
void launcher_launch_app(const char* desktop_path);

// Free the memory allocated by launcher_get_installed_apps
void launcher_free_items(AppItemStruct* items, int32_t count);

#ifdef __cplusplus
}
#endif

#endif // APP_LAUNCHER_H
