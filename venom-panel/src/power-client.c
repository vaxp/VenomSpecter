#include "power-client.h"
#include <gio/gio.h>

#define POWER_BUS_NAME "org.venom.Power"
#define POWER_OBJECT_PATH "/org/venom/Power"
#define POWER_INTERFACE "org.venom.Power"

static GDBusProxy *_proxy = NULL;
static PowerProfileCallback _profile_cb = NULL;
static gpointer _profile_data = NULL;

static void ensure_proxy(void);

static void on_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
                      GVariant *parameters, gpointer user_data) {
    (void)proxy; (void)sender_name; (void)user_data;
    
    if (g_strcmp0(signal_name, "ProfileChanged") == 0) {
        if (_profile_cb) {
            const gchar *profile;
            g_variant_get(parameters, "(&s)", &profile);
            _profile_cb(profile, _profile_data);
        }
    }
}

static void ensure_proxy(void) {
    if (_proxy) return;
    
    GError *error = NULL;
    _proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          POWER_BUS_NAME,
                                          POWER_OBJECT_PATH,
                                          POWER_INTERFACE,
                                          NULL, &error);
    if (error) {
        g_warning("Power: Failed to connect daemon: %s", error->message);
        g_error_free(error);
        _proxy = NULL;
    } else {
        g_signal_connect(_proxy, "g-signal", G_CALLBACK(on_signal), NULL);
    }
}

void power_client_init(void) {
    ensure_proxy();
}

void power_client_cleanup(void) {
    if (_proxy) {
        g_object_unref(_proxy);
        _proxy = NULL;
    }
}

static void call_void_method(const gchar *method) {
    ensure_proxy();
    if (!_proxy) {
        g_warning("[Power] Proxy is NULL! Cannot call %s", method);
        return;
    }
    g_dbus_proxy_call(_proxy, method, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

void power_client_shutdown(void) { call_void_method("Shutdown"); }
void power_client_reboot(void) { call_void_method("Reboot"); }
void power_client_suspend(void) { call_void_method("Suspend"); }
void power_client_lock_screen(void) { call_void_method("LockScreen"); }

void power_get_active_profile(void) {
    ensure_proxy();
    if (!_proxy) return;
    
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetActiveProfile", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    if (res) {
        const gchar *profile;
        g_variant_get(res, "(&s)", &profile);
        if (_profile_cb) _profile_cb(profile, _profile_data);
        g_variant_unref(res);
    }
}

void power_set_active_profile(const gchar *profile) {
    ensure_proxy();
    if (!_proxy) return;
    g_dbus_proxy_call(_proxy, "SetActiveProfile", g_variant_new("(s)", profile), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

void power_client_on_profile_changed(PowerProfileCallback cb, gpointer user_data) {
    _profile_cb = cb;
    _profile_data = user_data;
}
