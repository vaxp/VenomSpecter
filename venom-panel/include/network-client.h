/*
 * network-client.h
 *
 * D-Bus client for venom_network daemon.
 * Interfaces: org.venom.Network.WiFi, org.venom.Network.Bluetooth, org.venom.Network.Ethernet
 */

#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <glib.h>

/* =====================================================================
 * WiFi Network Structure
 * ===================================================================== */
typedef struct {
    gchar *ssid;
    gchar *bssid;
    gint strength;
    gint frequency;
    gboolean secured;
    gboolean connected;
} WiFiNetwork;

/* Free a WiFiNetwork */
void wifi_network_free(WiFiNetwork *network);

/* Free a list of WiFiNetworks */
void wifi_network_list_free(GList *list);

/* =====================================================================
 * Initialization
 * ===================================================================== */
void network_client_init(void);
void network_client_cleanup(void);
gboolean network_client_is_available(void);

/* =====================================================================
 * WiFi
 * ===================================================================== */
gboolean wifi_client_is_enabled(void);
gboolean wifi_client_set_enabled(gboolean enabled);
gchar* wifi_client_get_ssid(void);

/* Get list of available networks (returns GList of WiFiNetwork*) */
GList* wifi_client_get_networks(void);

/* Connect to a network (password can be NULL for open networks) */
gboolean wifi_client_connect(const gchar *ssid, const gchar *password);

/* Disconnect from current network */
gboolean wifi_client_disconnect(void);

/* =====================================================================
 * Bluetooth
 * ===================================================================== */

/* Bluetooth Device Structure */
typedef struct {
    gchar *address;
    gchar *name;
    gchar *icon;
    gboolean paired;
    gboolean connected;
    gboolean trusted;
    gint rssi;
} BluetoothDevice;

/* Free a BluetoothDevice */
void bluetooth_device_free(BluetoothDevice *device);

/* Free a list of BluetoothDevices */
void bluetooth_device_list_free(GList *list);

/* Status */
gboolean bluetooth_client_is_powered(void);
gboolean bluetooth_client_set_powered(gboolean powered);
gboolean bluetooth_client_has_adapter(void);

/* Scanning */
gboolean bluetooth_client_start_scan(void);
gboolean bluetooth_client_stop_scan(void);

/* Get list of devices (returns GList of BluetoothDevice*) */
GList* bluetooth_client_get_devices(void);

/* Device operations */
gboolean bluetooth_client_pair(const gchar *address);
gboolean bluetooth_client_connect(const gchar *address);
gboolean bluetooth_client_disconnect(const gchar *address);
gboolean bluetooth_client_remove(const gchar *address);

/* =====================================================================
 * Ethernet
 * ===================================================================== */

/* Ethernet Interface Structure */
typedef struct {
    gchar *name;
    gchar *mac_address;
    gchar *ip_address;
    gchar *gateway;
    gint speed;
    gboolean connected;
    gboolean enabled;
} EthernetInterface;

/* Free an EthernetInterface */
void ethernet_interface_free(EthernetInterface *iface);

/* Free a list of EthernetInterfaces */
void ethernet_interface_list_free(GList *list);

/* Get list of interfaces (returns GList of EthernetInterface*) */
GList* ethernet_client_get_interfaces(void);

/* Legacy functions */
gboolean ethernet_client_is_connected(void);
gchar* ethernet_client_get_interface_name(void);

#endif /* NETWORK_CLIENT_H */

