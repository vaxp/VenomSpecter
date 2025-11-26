/*
 * bluetooth-manager.h
 *
 * Bluetooth Manager using GDBus (BlueZ 5 API)
 * Mirrors logic from bluetoothmanagerdialog.dart
 */

#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <glib.h>
#include <stdbool.h>

typedef struct {
    char *path;
    char *name;
    char *address;
    bool connected;
    bool paired;
    int rssi;
} BluetoothDevice;

typedef struct {
    BluetoothDevice *devices;
    guint count;
} BluetoothDeviceList;

/* Callback for when device list changes */
typedef void (*BluetoothCallback)(BluetoothDeviceList *devices, gpointer user_data);

/* Core Lifecycle */
void bluetooth_manager_init(void);
void bluetooth_manager_cleanup(void);

/* Adapter Control */
gboolean bluetooth_manager_is_adapter_powered(void);
void bluetooth_manager_set_adapter_powered(gboolean powered);

/* Scanning */
void bluetooth_manager_start_scan(void);
void bluetooth_manager_stop_scan(void);
gboolean bluetooth_manager_is_scanning(void);

/* Device Management */
BluetoothDeviceList* bluetooth_manager_get_devices(void);
void bluetooth_manager_free_devices(BluetoothDeviceList *list);

void bluetooth_manager_pair_device(const char *path);
void bluetooth_manager_connect_device(const char *path);
void bluetooth_manager_disconnect_device(const char *path);

/* Events */
void bluetooth_manager_set_callback(BluetoothCallback callback, gpointer user_data);

#endif /* BLUETOOTH_MANAGER_H */
