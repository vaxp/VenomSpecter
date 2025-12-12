#include "system-icons.h"
#include "network-client.h"

/* Global references for updating */
static GtkWidget *_wifi_icon = NULL;
static GtkWidget *_bluetooth_icon = NULL;

/* Update icon visibility based on network state */
static gboolean update_system_icons(gpointer data) {
    (void)data;
    
    /* Update WiFi icon visibility */
    if (_wifi_icon) {
        gboolean wifi_enabled = wifi_client_is_enabled();
        if (wifi_enabled) {
            gtk_widget_show(_wifi_icon);
        } else {
            gtk_widget_hide(_wifi_icon);
        }
    }
    
    /* Update Bluetooth icon visibility */
    if (_bluetooth_icon) {
        gboolean bt_powered = bluetooth_client_is_powered();
        if (bt_powered) {
            gtk_widget_show(_bluetooth_icon);
        } else {
            gtk_widget_hide(_bluetooth_icon);
        }
    }
    
    return G_SOURCE_CONTINUE;  /* Keep the timer running */
}

GtkWidget* create_system_icons(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    /* Initialize network client if not already done */
    network_client_init();
    
    /* WiFi icon - visible only if enabled */
    _wifi_icon = gtk_image_new_from_icon_name("network-wireless-signal-excellent-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), _wifi_icon, FALSE, FALSE, 0);
    if (!wifi_client_is_enabled()) {
        gtk_widget_set_no_show_all(_wifi_icon, TRUE);
    }
    
    /* Bluetooth icon - visible only if powered */
    _bluetooth_icon = gtk_image_new_from_icon_name("bluetooth-active-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), _bluetooth_icon, FALSE, FALSE, 0);
    if (!bluetooth_client_is_powered()) {
        gtk_widget_set_no_show_all(_bluetooth_icon, TRUE);
    }
    
    /* Battery icon - always visible */
    GtkWidget *battery = gtk_image_new_from_icon_name("battery-full-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), battery, FALSE, FALSE, 0);
    
    /* Add CSS class */
    GtkStyleContext *context = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(context, "system-icons");
    
    /* Setup periodic update timer (every 2 seconds) */
    g_timeout_add_seconds(2, update_system_icons, NULL);
    
    return box;
}
