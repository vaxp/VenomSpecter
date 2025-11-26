/*
 * Simple WiFi Manager
 * Just shows WiFi status - no complex connection management
 */

#include "wifi-manager.h"
#include <gio/gio.h>
#include <stdio.h>

static GDBusProxy *_nm_proxy = NULL;

void wifi_manager_init(void)
{
    GError *error = NULL;
    
    _nm_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.freedesktop.NetworkManager",
        "/org/freedesktop/NetworkManager",
        "org.freedesktop.NetworkManager",
        NULL,
        &error
    );
    
    if (!_nm_proxy) {
        g_debug("[WiFi] NetworkManager not available: %s", 
                error ? error->message : "unknown");
        if (error) g_error_free(error);
    } else {
        g_print("[WiFi] Manager initialized\n");
    }
}

void wifi_manager_cleanup(void)
{
    if (_nm_proxy) {
        g_object_unref(_nm_proxy);
        _nm_proxy = NULL;
    }
}

gboolean wifi_manager_is_wireless_enabled(void)
{
    if (!_nm_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _nm_proxy,
        "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.freedesktop.NetworkManager", "WirelessEnabled"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    
    gboolean enabled = FALSE;
    if (result) {
        GVariant *var;
        g_variant_get(result, "(v)", &var);
        enabled = g_variant_get_boolean(var);
        g_variant_unref(var);
        g_variant_unref(result);
    } else {
        if (error) {
            g_debug("[WiFi] Failed to get status: %s", error->message);
            g_error_free(error);
        }
    }
    
    return enabled;
}

void wifi_manager_set_wireless_enabled(gboolean enabled)
{
    if (!_nm_proxy) {
        g_warning("[WiFi] Cannot set WiFi state - NetworkManager not available");
        return;
    }
    
    g_print("[WiFi] Setting WiFi to: %s\n", enabled ? "ON" : "OFF");
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _nm_proxy,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", 
                      "org.freedesktop.NetworkManager", 
                      "WirelessEnabled",
                      g_variant_new_boolean(enabled)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    
    if (result) {
        g_variant_unref(result);
        g_print("[WiFi] WiFi state changed successfully\n");
    } else {
        g_warning("[WiFi] Failed to set WiFi state: %s", 
                  error ? error->message : "unknown");
        if (error) g_error_free(error);
    }
}
