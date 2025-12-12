/*
 * network-client.c
 *
 * D-Bus client for venom_network daemon (org.venom.Network).
 * Connects to session bus and provides simple API for WiFi, Bluetooth, Ethernet.
 */

#include "network-client.h"
#include <gio/gio.h>
#include <stdio.h>

/* D-Bus configuration */
#define DBUS_NAME "org.venom.Network"
#define DBUS_PATH_WIFI "/org/venom/Network/WiFi"
#define DBUS_PATH_BLUETOOTH "/org/venom/Network/Bluetooth"
#define DBUS_PATH_ETHERNET "/org/venom/Network/Ethernet"
#define DBUS_IFACE_WIFI "org.venom.Network.WiFi"
#define DBUS_IFACE_BLUETOOTH "org.venom.Network.Bluetooth"
#define DBUS_IFACE_ETHERNET "org.venom.Network.Ethernet"

/* Global proxies */
static GDBusProxy *_wifi_proxy = NULL;
static GDBusProxy *_bluetooth_proxy = NULL;
static GDBusProxy *_ethernet_proxy = NULL;
static gboolean _initialized = FALSE;

/* =====================================================================
 * INITIALIZATION
 * ===================================================================== */

void network_client_init(void) {
    if (_initialized) return;
    
    GError *error = NULL;
    
    g_print("[NetworkClient] Connecting to org.venom.Network...\n");
    
    /* WiFi Proxy */
    _wifi_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        DBUS_NAME,
        DBUS_PATH_WIFI,
        DBUS_IFACE_WIFI,
        NULL, &error);
    
    if (error) {
        g_print("[NetworkClient] ❌ WiFi proxy failed: %s\n", error->message);
        g_clear_error(&error);
    } else if (_wifi_proxy) {
        g_print("[NetworkClient] ✅ WiFi proxy connected\n");
    }
    
    /* Bluetooth Proxy */
    _bluetooth_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        DBUS_NAME,
        DBUS_PATH_BLUETOOTH,
        DBUS_IFACE_BLUETOOTH,
        NULL, &error);
    
    if (error) {
        g_print("[NetworkClient] ❌ Bluetooth proxy failed: %s\n", error->message);
        g_clear_error(&error);
    } else if (_bluetooth_proxy) {
        g_print("[NetworkClient] ✅ Bluetooth proxy connected\n");
    }
    
    /* Ethernet Proxy */
    _ethernet_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        DBUS_NAME,
        DBUS_PATH_ETHERNET,
        DBUS_IFACE_ETHERNET,
        NULL, &error);
    
    if (error) {
        g_print("[NetworkClient] ❌ Ethernet proxy failed: %s\n", error->message);
        g_clear_error(&error);
    } else if (_ethernet_proxy) {
        g_print("[NetworkClient] ✅ Ethernet proxy connected\n");
    }
    
    _initialized = TRUE;
    
    if (!_wifi_proxy && !_bluetooth_proxy && !_ethernet_proxy) {
        g_print("[NetworkClient] ⚠️  No proxies connected! Is venom_network daemon running?\n");
    } else {
        g_print("[NetworkClient] Initialized\n");
    }
}

void network_client_cleanup(void) {
    if (_wifi_proxy) {
        g_object_unref(_wifi_proxy);
        _wifi_proxy = NULL;
    }
    if (_bluetooth_proxy) {
        g_object_unref(_bluetooth_proxy);
        _bluetooth_proxy = NULL;
    }
    if (_ethernet_proxy) {
        g_object_unref(_ethernet_proxy);
        _ethernet_proxy = NULL;
    }
    _initialized = FALSE;
}

gboolean network_client_is_available(void) {
    return _wifi_proxy != NULL || _bluetooth_proxy != NULL || _ethernet_proxy != NULL;
}

/* =====================================================================
 * WIFI
 * ===================================================================== */

gboolean wifi_client_is_enabled(void) {
    if (!_wifi_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _wifi_proxy, "IsEnabled", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_debug("[NetworkClient] WiFi IsEnabled failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean enabled = FALSE;
    g_variant_get(result, "(b)", &enabled);
    g_variant_unref(result);
    return enabled;
}

gboolean wifi_client_set_enabled(gboolean enabled) {
    if (!_wifi_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _wifi_proxy, "SetEnabled",
        g_variant_new("(b)", enabled),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[NetworkClient] WiFi SetEnabled failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_variant_unref(result);
    return success;
}

gchar* wifi_client_get_ssid(void) {
    if (!_wifi_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _wifi_proxy, "GetStatus", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_debug("[NetworkClient] WiFi GetStatus failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    /* Result is ((bsssssis)) - connected, ssid, ip, gateway, subnet, dns, strength, speed */
    gboolean connected;
    const gchar *ssid;
    g_variant_get(result, "((bs&s&s&s&sis))", &connected, &ssid, NULL, NULL, NULL, NULL, NULL, NULL);
    
    gchar *ret = NULL;
    if (connected && ssid && strlen(ssid) > 0) {
        ret = g_strdup(ssid);
    }
    g_variant_unref(result);
    return ret;
}

/* WiFiNetwork memory management */
void wifi_network_free(WiFiNetwork *network) {
    if (network) {
        g_free(network->ssid);
        g_free(network->bssid);
        g_free(network);
    }
}

void wifi_network_list_free(GList *list) {
    g_list_free_full(list, (GDestroyNotify)wifi_network_free);
}

/* Get list of available WiFi networks */
GList* wifi_client_get_networks(void) {
    if (!_wifi_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _wifi_proxy, "GetNetworks", NULL,
        G_DBUS_CALL_FLAGS_NONE, 10000, NULL, &error);
    
    if (error) {
        g_warning("[WiFi] GetNetworks failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    /* Result is (a(ssiibb)) - array of (ssid, bssid, strength, frequency, secured, connected) */
    GList *networks = NULL;
    GVariantIter *iter;
    g_variant_get(result, "(a(ssiibb))", &iter);
    
    const gchar *ssid, *bssid;
    gint strength, frequency;
    gboolean secured, connected;
    
    while (g_variant_iter_next(iter, "(&s&siibb)", &ssid, &bssid, &strength, &frequency, &secured, &connected)) {
        if (ssid && strlen(ssid) > 0) {
            WiFiNetwork *net = g_new0(WiFiNetwork, 1);
            net->ssid = g_strdup(ssid);
            net->bssid = g_strdup(bssid);
            net->strength = strength;
            net->frequency = frequency;
            net->secured = secured;
            net->connected = connected;
            
            /* Put connected network first */
            if (connected) {
                networks = g_list_prepend(networks, net);
            } else {
                networks = g_list_append(networks, net);
            }
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    g_print("[WiFi] Found %d networks\n", g_list_length(networks));
    return networks;
}

/* Connect to a WiFi network */
gboolean wifi_client_connect(const gchar *ssid, const gchar *password) {
    if (!_wifi_proxy || !ssid) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _wifi_proxy, "Connect",
        g_variant_new("(ss)", ssid, password ? password : ""),
        G_DBUS_CALL_FLAGS_NONE, 30000, NULL, &error);
    
    if (error) {
        g_warning("[WiFi] Connect to '%s' failed: %s", ssid, error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_variant_unref(result);
    
    g_print("[WiFi] Connect to '%s': %s\n", ssid, success ? "success" : "failed");
    return success;
}

/* Disconnect from current WiFi network */
gboolean wifi_client_disconnect(void) {
    if (!_wifi_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _wifi_proxy, "Disconnect", NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[WiFi] Disconnect failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_variant_unref(result);
    
    g_print("[WiFi] Disconnect: %s\n", success ? "success" : "failed");
    return success;
}

/* =====================================================================
 * BLUETOOTH
 * ===================================================================== */

gboolean bluetooth_client_has_adapter(void) {
    if (!_bluetooth_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "HasAdapter", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_debug("[NetworkClient] Bluetooth HasAdapter failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean has = FALSE;
    g_variant_get(result, "(b)", &has);
    g_variant_unref(result);
    return has;
}

gboolean bluetooth_client_is_powered(void) {
    if (!_bluetooth_proxy) {
        g_print("[Bluetooth] No proxy!\n");
        return FALSE;
    }
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "GetStatus", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_print("[Bluetooth] GetStatus failed: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    /* Debug: print the actual type */
    const gchar *type_str = g_variant_get_type_string(result);
    g_print("[Bluetooth] GVariant type: %s\n", type_str);
    g_print("[Bluetooth] GVariant data: %s\n", g_variant_print(result, TRUE));
    
    /* Result is ((bbbss)) - powered, discovering, pairable, name, address */
    gboolean powered = FALSE, discovering = FALSE, pairable = FALSE;
    const gchar *name = NULL, *address = NULL;
    
    /* Try parsing based on actual type */
    if (g_strcmp0(type_str, "((bbbss))") == 0) {
        g_variant_get(result, "((bbb&s&s))", &powered, &discovering, &pairable, &name, &address);
    } else if (g_strcmp0(type_str, "(bbbss)") == 0) {
        g_variant_get(result, "(bbb&s&s)", &powered, &discovering, &pairable, &name, &address);
    } else {
        g_print("[Bluetooth] Unknown type! Trying generic parse...\n");
        /* Try to extract from child */
        GVariant *inner = g_variant_get_child_value(result, 0);
        if (inner) {
            g_print("[Bluetooth] Inner type: %s\n", g_variant_get_type_string(inner));
            g_variant_get(inner, "(bbb&s&s)", &powered, &discovering, &pairable, &name, &address);
            g_variant_unref(inner);
        }
    }
    
    g_print("[Bluetooth] Status: powered=%d, discovering=%d, pairable=%d\n", powered, discovering, pairable);
    g_variant_unref(result);
    return powered;
}

gboolean bluetooth_client_set_powered(gboolean powered) {
    g_print("[Bluetooth] SetPowered called with: %d\n", powered);
    
    if (!_bluetooth_proxy) {
        g_print("[Bluetooth] No proxy!\n");
        return FALSE;
    }
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "SetPowered",
        g_variant_new("(b)", powered),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_print("[Bluetooth] SetPowered failed: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_print("[Bluetooth] SetPowered result: %d\n", success);
    g_variant_unref(result);
    return success;
}

/* BluetoothDevice memory management */
void bluetooth_device_free(BluetoothDevice *device) {
    if (device) {
        g_free(device->address);
        g_free(device->name);
        g_free(device->icon);
        g_free(device);
    }
}

void bluetooth_device_list_free(GList *list) {
    g_list_free_full(list, (GDestroyNotify)bluetooth_device_free);
}

/* Start Bluetooth scanning */
gboolean bluetooth_client_start_scan(void) {
    if (!_bluetooth_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "StartScan", NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] StartScan failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_variant_unref(result);
    g_print("[Bluetooth] Scan started: %d\n", success);
    return success;
}

/* Stop Bluetooth scanning */
gboolean bluetooth_client_stop_scan(void) {
    if (!_bluetooth_proxy) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "StopScan", NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] StopScan failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_variant_unref(result);
    return success;
}

/* Get list of Bluetooth devices */
GList* bluetooth_client_get_devices(void) {
    if (!_bluetooth_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "GetDevices", NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] GetDevices failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    /* Result is (a(sssbbb)) - array of (address, name, icon, paired, connected, trusted) */
    GList *devices = NULL;
    GVariantIter *iter;
    g_variant_get(result, "(a(sssbbb))", &iter);
    
    const gchar *address, *name, *icon;
    gboolean paired, connected, trusted;
    
    while (g_variant_iter_next(iter, "(&s&s&sbbb)", &address, &name, &icon, &paired, &connected, &trusted)) {
        BluetoothDevice *dev = g_new0(BluetoothDevice, 1);
        dev->address = g_strdup(address);
        dev->name = g_strdup(name && strlen(name) > 0 ? name : address);
        dev->icon = g_strdup(icon);
        dev->paired = paired;
        dev->connected = connected;
        dev->trusted = trusted;
        dev->rssi = 0;
        
        /* Put connected devices first, then paired */
        if (connected) {
            devices = g_list_prepend(devices, dev);
        } else if (paired) {
            GList *first_unpaired = g_list_find_custom(devices, NULL, 
                (GCompareFunc)(void*)g_direct_equal);
            if (first_unpaired) {
                devices = g_list_insert_before(devices, first_unpaired, dev);
            } else {
                devices = g_list_append(devices, dev);
            }
        } else {
            devices = g_list_append(devices, dev);
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    g_print("[Bluetooth] Found %d devices\n", g_list_length(devices));
    return devices;
}

/* Pair with a Bluetooth device */
gboolean bluetooth_client_pair(const gchar *address) {
    if (!_bluetooth_proxy || !address) return FALSE;
    
    g_print("[Bluetooth] Pairing with %s...\n", address);
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "Pair",
        g_variant_new("(s)", address),
        G_DBUS_CALL_FLAGS_NONE, 60000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] Pair failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    /* Result is (bs) - success, message */
    gboolean success = FALSE;
    const gchar *message;
    g_variant_get(result, "(b&s)", &success, &message);
    g_print("[Bluetooth] Pair result: %s - %s\n", success ? "success" : "failed", message);
    g_variant_unref(result);
    return success;
}

/* Connect to a Bluetooth device */
gboolean bluetooth_client_connect(const gchar *address) {
    if (!_bluetooth_proxy || !address) return FALSE;
    
    g_print("[Bluetooth] Connecting to %s...\n", address);
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "Connect",
        g_variant_new("(s)", address),
        G_DBUS_CALL_FLAGS_NONE, 30000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] Connect failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    const gchar *message;
    g_variant_get(result, "(b&s)", &success, &message);
    g_print("[Bluetooth] Connect result: %s - %s\n", success ? "success" : "failed", message);
    g_variant_unref(result);
    return success;
}

/* Disconnect from a Bluetooth device */
gboolean bluetooth_client_disconnect(const gchar *address) {
    if (!_bluetooth_proxy || !address) return FALSE;
    
    g_print("[Bluetooth] Disconnecting from %s...\n", address);
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "Disconnect",
        g_variant_new("(s)", address),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] Disconnect failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    const gchar *message;
    g_variant_get(result, "(b&s)", &success, &message);
    g_variant_unref(result);
    return success;
}

/* Remove a Bluetooth device */
gboolean bluetooth_client_remove(const gchar *address) {
    if (!_bluetooth_proxy || !address) return FALSE;
    
    g_print("[Bluetooth] Removing %s...\n", address);
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _bluetooth_proxy, "Remove",
        g_variant_new("(s)", address),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] Remove failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean success = FALSE;
    g_variant_get(result, "(b)", &success);
    g_variant_unref(result);
    return success;
}

/* =====================================================================
 * ETHERNET
 * ===================================================================== */

/* EthernetInterface memory management */
void ethernet_interface_free(EthernetInterface *iface) {
    if (iface) {
        g_free(iface->name);
        g_free(iface->mac_address);
        g_free(iface->ip_address);
        g_free(iface->gateway);
        g_free(iface);
    }
}

void ethernet_interface_list_free(GList *list) {
    g_list_free_full(list, (GDestroyNotify)ethernet_interface_free);
}

/* Get list of Ethernet interfaces */
GList* ethernet_client_get_interfaces(void) {
    if (!_ethernet_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _ethernet_proxy, "GetInterfaces", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_warning("[Ethernet] GetInterfaces failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    /* Result is (a(ssssibb)) - array of (name, mac, ip, gateway, speed, connected, enabled) */
    GList *interfaces = NULL;
    GVariantIter *iter;
    g_variant_get(result, "(a(ssssibb))", &iter);
    
    const gchar *name, *mac, *ip, *gateway;
    gint speed;
    gboolean connected, enabled;
    
    while (g_variant_iter_next(iter, "(&s&s&s&sibb)", &name, &mac, &ip, &gateway, &speed, &connected, &enabled)) {
        EthernetInterface *iface = g_new0(EthernetInterface, 1);
        iface->name = g_strdup(name);
        iface->mac_address = g_strdup(mac);
        iface->ip_address = g_strdup(ip);
        iface->gateway = g_strdup(gateway);
        iface->speed = speed;
        iface->connected = connected;
        iface->enabled = enabled;
        
        /* Put connected interfaces first */
        if (connected) {
            interfaces = g_list_prepend(interfaces, iface);
        } else {
            interfaces = g_list_append(interfaces, iface);
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    g_print("[Ethernet] Found %d interfaces\n", g_list_length(interfaces));
    return interfaces;
}

gboolean ethernet_client_is_connected(void) {
    g_print("[Ethernet] Checking connection...\n");
    
    if (!_ethernet_proxy) {
        g_print("[Ethernet] No proxy!\n");
        return FALSE;
    }
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _ethernet_proxy, "GetInterfaces", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_print("[Ethernet] GetInterfaces failed: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    /* Result is (a(ssssibb)) - array of (name, mac, ip, gateway, speed, connected, enabled) */
    GVariantIter *iter;
    g_variant_get(result, "(a(ssssibb))", &iter);
    
    gboolean any_connected = FALSE;
    const gchar *name, *mac, *ip, *gateway;
    gint speed;
    gboolean connected, enabled;
    
    while (g_variant_iter_next(iter, "(&s&s&s&sibb)", &name, &mac, &ip, &gateway, &speed, &connected, &enabled)) {
        g_print("[Ethernet] Interface: %s, connected=%d, enabled=%d\n", name, connected, enabled);
        if (connected) {
            any_connected = TRUE;
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    g_print("[Ethernet] Any connected: %d\n", any_connected);
    return any_connected;
}

gchar* ethernet_client_get_interface_name(void) {
    if (!_ethernet_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        _ethernet_proxy, "GetInterfaces", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_debug("[NetworkClient] Ethernet GetInterfaces failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    GVariantIter *iter;
    g_variant_get(result, "(a(ssssibb))", &iter);
    
    gchar *ret = NULL;
    const gchar *name, *mac, *ip, *gateway;
    gint speed;
    gboolean connected, enabled;
    
    while (g_variant_iter_next(iter, "(ssssibb)", &name, &mac, &ip, &gateway, &speed, &connected, &enabled)) {
        if (connected && name) {
            ret = g_strdup(name);
            break;
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    return ret;
}
