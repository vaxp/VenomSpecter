#include "app_launcher.h"
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Internal Cache Structure */
typedef struct {
    char *name;
    char *desktop_path;
    uint8_t *icon_data;
    int32_t icon_size;
} CachedApp;

static bool is_initialized = false;
static GList *cached_apps = NULL;
static uint64_t cache_version = 1;
static GAppInfoMonitor *monitor = NULL;

/* Forward declarations */
static void rebuild_cache();
static void on_app_info_changed(GAppInfoMonitor *monitor, gpointer user_data);
static void free_cached_app(gpointer data);

void launcher_init() {
    if (is_initialized) return;
    if (!gtk_init_check(NULL, NULL)) {
        fprintf(stderr, "Failed to initialize GTK\n");
        return;
    }

    // Setup monitor for changes
    monitor = g_app_info_monitor_get();
    g_signal_connect(monitor, "changed", G_CALLBACK(on_app_info_changed), NULL);

    // Build initial cache
    rebuild_cache();

    is_initialized = true;
}

static void on_app_info_changed(GAppInfoMonitor *monitor, gpointer user_data) {
    // Invalidate cache
    // We can either rebuild immediately or lazily. 
    // Let's rebuild immediately to keep it simple and ready.
    rebuild_cache();
}

static void free_cached_app(gpointer data) {
    CachedApp *app = (CachedApp *)data;
    if (app->name) free(app->name);
    if (app->desktop_path) free(app->desktop_path);
    if (app->icon_data) free(app->icon_data);
    free(app);
}

static void rebuild_cache() {
    if (cached_apps) {
        g_list_free_full(cached_apps, free_cached_app);
        cached_apps = NULL;
    }

    GList *all_apps = g_app_info_get_all();
    GList *l;

    for (l = all_apps; l != NULL; l = l->next) {
        GAppInfo *app_info = (GAppInfo *)l->data;
        if (!g_app_info_should_show(app_info)) continue;

        CachedApp *app = (CachedApp *)malloc(sizeof(CachedApp));
        app->name = strdup(g_app_info_get_name(app_info));
        
        // Get desktop file path
        if (G_IS_DESKTOP_APP_INFO(app_info)) {
            const char *filename = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(app_info));
            app->desktop_path = filename ? strdup(filename) : NULL;
        } else {
            app->desktop_path = NULL;
        }

        // Icon processing
        app->icon_data = NULL;
        app->icon_size = 0;
        GIcon *icon = g_app_info_get_icon(app_info);
        
        if (icon) {
            GtkIconTheme *theme = gtk_icon_theme_get_default();
            GtkIconInfo *info = gtk_icon_theme_lookup_by_gicon(theme, icon, 64, GTK_ICON_LOOKUP_FORCE_SIZE);
            if (info) {
                GdkPixbuf *pixbuf = gtk_icon_info_load_icon(info, NULL);
                if (pixbuf) {
                    gchar *buffer;
                    gsize buffer_size;
                    if (gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &buffer_size, "png", NULL, NULL)) {
                        app->icon_data = (uint8_t*)malloc(buffer_size);
                        memcpy(app->icon_data, buffer, buffer_size);
                        app->icon_size = buffer_size;
                        g_free(buffer);
                    }
                    g_object_unref(pixbuf);
                }
                g_object_unref(info);
            }
        }

        cached_apps = g_list_append(cached_apps, app);
    }
    
    g_list_free_full(all_apps, g_object_unref);
    
    // Increment version
    cache_version++;
}

/* 
 * Modified to accept a version pointer.
 * If *client_version == cache_version, returns NULL (no changes).
 * Otherwise, returns the list and updates *client_version.
 */
AppItemStruct* launcher_get_installed_apps(int32_t* count, uint64_t* client_version) {
    if (!is_initialized) launcher_init();

    if (client_version != NULL && *client_version == cache_version) {
        *count = 0;
        return NULL;
    }

    if (client_version != NULL) {
        *client_version = cache_version;
    }

    guint size = g_list_length(cached_apps);
    *count = size;
    
    AppItemStruct* items = (AppItemStruct*)malloc(sizeof(AppItemStruct) * size);
    
    int i = 0;
    for (GList *l = cached_apps; l != NULL; l = l->next) {
        CachedApp *cached = (CachedApp *)l->data;
        
        // Deep copy for the client (Flutter will free this)
        items[i].name = strdup(cached->name);
        items[i].desktop_path = cached->desktop_path ? strdup(cached->desktop_path) : NULL;
        
        items[i].icon_size = cached->icon_size;
        if (cached->icon_data) {
            items[i].icon_data = (uint8_t*)malloc(cached->icon_size);
            memcpy(items[i].icon_data, cached->icon_data, cached->icon_size);
        } else {
            items[i].icon_data = NULL;
        }
        
        i++;
    }

    return items;
}

void launcher_launch_app(const char* desktop_path) {
    if (!desktop_path) return;
    
    GDesktopAppInfo *app_info = g_desktop_app_info_new_from_filename(desktop_path);
    if (app_info) {
        GdkAppLaunchContext *context = gdk_display_get_app_launch_context(gdk_display_get_default());
        g_app_info_launch(G_APP_INFO(app_info), NULL, G_APP_LAUNCH_CONTEXT(context), NULL);
        g_object_unref(context);
        g_object_unref(app_info);
    }
}

void launcher_free_items(AppItemStruct* items, int32_t count) {
    if (items == NULL) return;
    for (int i = 0; i < count; i++) {
        free(items[i].name);
        if (items[i].desktop_path) free(items[i].desktop_path);
        if (items[i].icon_data) free(items[i].icon_data);
    }
    free(items);
}
