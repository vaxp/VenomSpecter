/*
 * bluetooth-manager.c
 *
 * Implementation of Bluetooth Manager using GDBus.
 * Based on bluetoothmanagerdialog.dart
 */

#include "bluetooth-manager.h"
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

/* =====================================================================
 * GLOBALS & STATE
 * ===================================================================== */

static GDBusConnection *_bus = NULL;
static char *_adapter_path = NULL;
static gboolean _is_scanning = FALSE;
static BluetoothCallback _callback = NULL;
static gpointer _callback_data = NULL;
static guint _signal_sub_id = 0;
static guint _interfaces_added_sub_id = 0;
static guint _interfaces_removed_sub_id = 0;

/* =====================================================================
 * HELPER FUNCTIONS
 * ===================================================================== */

static void _notify_update(void) {
    if (_callback) {
        BluetoothDeviceList *list = bluetooth_manager_get_devices();
        _callback(list, _callback_data);
        bluetooth_manager_free_devices(list);
    }
}

/* =====================================================================
 * D-BUS SIGNAL HANDLERS
 * ===================================================================== */

static void _on_properties_changed(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path;
    (void)interface_name; (void)signal_name; (void)parameters; (void)user_data;
    
    /* We could parse parameters to be more specific, but for now just refresh */
    _notify_update();
}

static void _on_interfaces_changed(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path;
    (void)interface_name; (void)signal_name; (void)parameters; (void)user_data;
    
    _notify_update();
}

/* =====================================================================
 * INITIALIZATION & DISCOVERY
 * ===================================================================== */

static void _find_adapter(void) {
    if (_adapter_path) {
        g_free(_adapter_path);
        _adapter_path = NULL;
    }

    GError *error = NULL;
    GDBusProxy *om = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                           "org.bluez", "/", "org.freedesktop.DBus.ObjectManager",
                                           NULL, &error);
    if (!om) {
        g_warning("[Bluetooth] Failed to get ObjectManager: %s", error->message);
        g_error_free(error);
        return;
    }

    GVariant *result = g_dbus_proxy_call_sync(om, "GetManagedObjects", NULL,
                                              G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (result) {
        /* The result is a tuple containing one dictionary: (a{oa{sa{sv}}}) */
        GVariant *managed_objects = g_variant_get_child_value(result, 0);
        
        GVariantIter iter;
        g_variant_iter_init(&iter, managed_objects);
        
        GVariant *entry;
        while ((entry = g_variant_iter_next_value(&iter))) {
            GVariant *path_v = g_variant_get_child_value(entry, 0);
            GVariant *ifaces_v = g_variant_get_child_value(entry, 1);
            
            const char *path = g_variant_get_string(path_v, NULL);
            
            if (g_variant_lookup(ifaces_v, "org.bluez.Adapter1", NULL)) {
                _adapter_path = g_strdup(path);
                g_variant_unref(path_v);
                g_variant_unref(ifaces_v);
                g_variant_unref(entry);
                break; 
            }
            
            g_variant_unref(path_v);
            g_variant_unref(ifaces_v);
            g_variant_unref(entry);
        }
        
        g_variant_unref(managed_objects);
        g_variant_unref(result);
    } else {
        g_warning("[Bluetooth] GetManagedObjects failed: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(om);
}

void bluetooth_manager_init(void) {
    GError *error = NULL;
    _bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!_bus) {
        g_warning("[Bluetooth] Failed to connect to system bus: %s", error->message);
        g_error_free(error);
        return;
    }

    _find_adapter();

    /* Subscribe to signals */
    _signal_sub_id = g_dbus_connection_signal_subscribe(_bus, "org.bluez", 
                                                        "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                                        NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                                        _on_properties_changed, NULL, NULL);

    _interfaces_added_sub_id = g_dbus_connection_signal_subscribe(_bus, "org.bluez",
                                                                  "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
                                                                  NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                                                  _on_interfaces_changed, NULL, NULL);

    _interfaces_removed_sub_id = g_dbus_connection_signal_subscribe(_bus, "org.bluez",
                                                                    "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
                                                                    NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                                                    _on_interfaces_changed, NULL, NULL);
}

void bluetooth_manager_cleanup(void) {
    if (_bus) {
        if (_signal_sub_id) g_dbus_connection_signal_unsubscribe(_bus, _signal_sub_id);
        if (_interfaces_added_sub_id) g_dbus_connection_signal_unsubscribe(_bus, _interfaces_added_sub_id);
        if (_interfaces_removed_sub_id) g_dbus_connection_signal_unsubscribe(_bus, _interfaces_removed_sub_id);
        g_object_unref(_bus);
        _bus = NULL;
    }
    if (_adapter_path) {
        g_free(_adapter_path);
        _adapter_path = NULL;
    }
}

/* =====================================================================
 * POWER CONTROL
 * ===================================================================== */

gboolean bluetooth_manager_is_adapter_powered(void) {
    if (!_bus || !_adapter_path) return FALSE;

    GError *error = NULL;
    GDBusProxy *adapter = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                                "org.bluez", _adapter_path, "org.bluez.Adapter1",
                                                NULL, &error);
    if (!adapter) {
        g_error_free(error);
        return FALSE;
    }

    GVariant *v = g_dbus_proxy_get_cached_property(adapter, "Powered");
    gboolean powered = FALSE;
    if (v) {
        powered = g_variant_get_boolean(v);
        g_variant_unref(v);
    } else {
        /* Try explicit get if cached is missing */
        v = g_dbus_proxy_call_sync(adapter, "Get", 
                                   g_variant_new("(ss)", "org.bluez.Adapter1", "Powered"),
                                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL); // ignoring error for brevity
        /* This returns (v), so we need to unwrap */
        /* Actually org.freedesktop.DBus.Properties.Get */
    }
    g_object_unref(adapter);
    return powered;
}

void bluetooth_manager_set_adapter_powered(gboolean powered) {
    if (!_bus || !_adapter_path) return;

    GError *error = NULL;
    /* We need to use org.freedesktop.DBus.Properties interface to Set */
    GDBusProxy *props = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                              "org.bluez", _adapter_path, "org.freedesktop.DBus.Properties",
                                              NULL, &error);
    if (!props) {
        g_warning("[Bluetooth] Failed to get Properties interface: %s", error->message);
        g_error_free(error);
        return;
    }

    g_dbus_proxy_call_sync(props, "Set",
                           g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(powered)),
                           G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    
    if (error) {
        g_warning("[Bluetooth] Failed to set power: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(props);
}

/* =====================================================================
 * SCANNING
 * ===================================================================== */

void bluetooth_manager_start_scan(void) {
    if (!_bus || !_adapter_path) return;
    
    GError *error = NULL;
    GDBusProxy *adapter = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                                "org.bluez", _adapter_path, "org.bluez.Adapter1",
                                                NULL, &error);
    if (adapter) {
        g_dbus_proxy_call_sync(adapter, "StartDiscovery", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        if (error) {
            /* Ignore "InProgress" error */
            if (!g_strrstr(error->message, "InProgress")) {
                g_warning("[Bluetooth] StartDiscovery failed: %s", error->message);
            }
            g_error_free(error);
        } else {
            _is_scanning = TRUE;
        }
        g_object_unref(adapter);
    }
}

void bluetooth_manager_stop_scan(void) {
    if (!_bus || !_adapter_path) return;

    GError *error = NULL;
    GDBusProxy *adapter = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                                "org.bluez", _adapter_path, "org.bluez.Adapter1",
                                                NULL, &error);
    if (adapter) {
        g_dbus_proxy_call_sync(adapter, "StopDiscovery", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        if (error) {
             /* Ignore "NoDiscoveryStarted" */
             g_error_free(error);
        }
        _is_scanning = FALSE;
        g_object_unref(adapter);
    }
}

gboolean bluetooth_manager_is_scanning(void) {
    return _is_scanning;
}

/* =====================================================================
 * DEVICE MANAGEMENT
 * ===================================================================== */

BluetoothDeviceList* bluetooth_manager_get_devices(void) {
    BluetoothDeviceList *list = g_new0(BluetoothDeviceList, 1);
    
    if (!_bus) return list;

    GError *error = NULL;
    GDBusProxy *om = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                           "org.bluez", "/", "org.freedesktop.DBus.ObjectManager",
                                           NULL, &error);
    if (!om) return list;

    GVariant *result = g_dbus_proxy_call_sync(om, "GetManagedObjects", NULL,
                                              G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (result) {
        /* The result is a tuple containing one dictionary: (a{oa{sa{sv}}}) */
        GVariant *managed_objects = g_variant_get_child_value(result, 0);
        
        GVariantIter iter;
        g_variant_iter_init(&iter, managed_objects);
        
        GVariant *entry;
        while ((entry = g_variant_iter_next_value(&iter))) {
            GVariant *path_v = g_variant_get_child_value(entry, 0);
            GVariant *ifaces_v = g_variant_get_child_value(entry, 1);
            
            const char *path = g_variant_get_string(path_v, NULL);
            
            GVariant *device_iface = g_variant_lookup_value(ifaces_v, "org.bluez.Device1", G_VARIANT_TYPE_VARDICT);
            if (device_iface) {
                /* It's a device */
                list->devices = g_realloc(list->devices, (list->count + 1) * sizeof(BluetoothDevice));
                BluetoothDevice *dev = &list->devices[list->count];
                
                dev->path = g_strdup(path);
                dev->name = g_strdup("Unknown");
                dev->address = g_strdup("??:??:??:??:??:??");
                dev->connected = FALSE;
                dev->paired = FALSE;
                dev->rssi = -100;

                /* Parse properties */
                GVariantDict dict;
                g_variant_dict_init(&dict, device_iface);
                
                GVariant *v;
                if ((v = g_variant_dict_lookup_value(&dict, "Alias", G_VARIANT_TYPE_STRING))) {
                    g_free(dev->name);
                    dev->name = g_strdup(g_variant_get_string(v, NULL));
                    g_variant_unref(v);
                } else if ((v = g_variant_dict_lookup_value(&dict, "Name", G_VARIANT_TYPE_STRING))) {
                    g_free(dev->name);
                    dev->name = g_strdup(g_variant_get_string(v, NULL));
                    g_variant_unref(v);
                }

                if ((v = g_variant_dict_lookup_value(&dict, "Address", G_VARIANT_TYPE_STRING))) {
                    g_free(dev->address);
                    dev->address = g_strdup(g_variant_get_string(v, NULL));
                    g_variant_unref(v);
                }

                if ((v = g_variant_dict_lookup_value(&dict, "Connected", G_VARIANT_TYPE_BOOLEAN))) {
                    dev->connected = g_variant_get_boolean(v);
                    g_variant_unref(v);
                }

                if ((v = g_variant_dict_lookup_value(&dict, "Paired", G_VARIANT_TYPE_BOOLEAN))) {
                    dev->paired = g_variant_get_boolean(v);
                    g_variant_unref(v);
                }

                if ((v = g_variant_dict_lookup_value(&dict, "RSSI", G_VARIANT_TYPE_INT16))) {
                    dev->rssi = g_variant_get_int16(v);
                    g_variant_unref(v);
                }

                g_variant_dict_clear(&dict);
                g_variant_unref(device_iface);
                list->count++;
            }
            
            g_variant_unref(path_v);
            g_variant_unref(ifaces_v);
            g_variant_unref(entry);
        }
        
        g_variant_unref(managed_objects);
        g_variant_unref(result);
    }
    g_object_unref(om);

    /* Sort: Connected > Paired > RSSI */
    /* Simple bubble sort for now */
    if (list->count > 1) {
        for (guint i = 0; i < list->count - 1; i++) {
            for (guint j = 0; j < list->count - i - 1; j++) {
                BluetoothDevice *a = &list->devices[j];
                BluetoothDevice *b = &list->devices[j + 1];
                
                int score_a = (a->connected ? 1000 : 0) + (a->paired ? 100 : 0) + a->rssi;
                int score_b = (b->connected ? 1000 : 0) + (b->paired ? 100 : 0) + b->rssi;
                
                if (score_b > score_a) {
                    BluetoothDevice temp = *a;
                    *a = *b;
                    *b = temp;
                }
            }
        }
    }

    return list;
}

void bluetooth_manager_free_devices(BluetoothDeviceList *list) {
    if (!list) return;
    for (guint i = 0; i < list->count; i++) {
        g_free(list->devices[i].path);
        g_free(list->devices[i].name);
        g_free(list->devices[i].address);
    }
    g_free(list->devices);
    g_free(list);
}

void bluetooth_manager_pair_device(const char *path) {
    if (!_bus || !path) return;
    
    GError *error = NULL;
    GDBusProxy *device = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                               "org.bluez", path, "org.bluez.Device1",
                                               NULL, &error);
    if (device) {
        g_dbus_proxy_call_sync(device, "Pair", NULL, G_DBUS_CALL_FLAGS_NONE, 30000, NULL, &error);
        if (error) {
            g_warning("[Bluetooth] Pair failed: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(device);
    }
}

void bluetooth_manager_connect_device(const char *path) {
    if (!_bus || !path) return;
    
    GError *error = NULL;
    GDBusProxy *device = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                               "org.bluez", path, "org.bluez.Device1",
                                               NULL, &error);
    if (!device) return;
    
    /* Check if device is paired */
    GVariant *paired_v = g_dbus_proxy_get_cached_property(device, "Paired");
    gboolean is_paired = FALSE;
    if (paired_v) {
        is_paired = g_variant_get_boolean(paired_v);
        g_variant_unref(paired_v);
    }
    
    /* If not paired, pair first */
    if (!is_paired) {
        g_print("[Bluetooth] Device not paired, pairing first...\n");
        g_dbus_proxy_call_sync(device, "Pair", NULL, G_DBUS_CALL_FLAGS_NONE, 30000, NULL, &error);
        if (error) {
            g_warning("[Bluetooth] Pair failed: %s", error->message);
            g_error_free(error);
            g_object_unref(device);
            return;
        }
        g_print("[Bluetooth] Pairing successful\n");
    }
    
    /* Now connect */
    g_dbus_proxy_call_sync(device, "Connect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error) {
        g_warning("[Bluetooth] Connect failed: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(device);
}

void bluetooth_manager_disconnect_device(const char *path) {
    if (!_bus || !path) return;
    
    GError *error = NULL;
    GDBusProxy *device = g_dbus_proxy_new_sync(_bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                               "org.bluez", path, "org.bluez.Device1",
                                               NULL, &error);
    if (device) {
        g_dbus_proxy_call_sync(device, "Disconnect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        if (error) {
            g_warning("[Bluetooth] Disconnect failed: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(device);
    }
}

void bluetooth_manager_set_callback(BluetoothCallback callback, gpointer user_data) {
    _callback = callback;
    _callback_data = user_data;
}
