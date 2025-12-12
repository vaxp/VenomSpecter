#include "shot-client.h"
#include <gio/gio.h>

#define SHOT_BUS_NAME "org.venom.Shot"
#define SHOT_OBJECT_PATH "/org/venom/Shot"
#define SHOT_INTERFACE "org.venom.Shot"

static GDBusProxy *_proxy = NULL;

static void ensure_proxy(void) {
    if (_proxy) return;
    
    GError *error = NULL;
    _proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          SHOT_BUS_NAME,
                                          SHOT_OBJECT_PATH,
                                          SHOT_INTERFACE,
                                          NULL, &error);
    if (error) {
        g_warning("Shot: Failed to connect daemon: %s", error->message);
        g_error_free(error);
        _proxy = NULL;
    }
}

void shot_client_init(void) {
    ensure_proxy();
}

void shot_client_cleanup(void) {
    if (_proxy) {
        g_object_unref(_proxy);
        _proxy = NULL;
    }
}

static void call_void_method(const gchar *method) {
    ensure_proxy();
    if (!_proxy) return;
    
    g_dbus_proxy_call(_proxy, method, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

void shot_take_full_screenshot(void) {
    call_void_method("FullScreen");
}

void shot_take_region_screenshot(void) {
    call_void_method("SelectRegion");
}

void shot_start_full_record(void) {
    call_void_method("StartRecord");
}

void shot_start_region_record(void) {
    call_void_method("SelectRecord");
}

void shot_stop_record(void) {
    call_void_method("StopRecord");
}
