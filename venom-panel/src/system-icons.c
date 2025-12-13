#include "system-icons.h"
#include "network-client.h"
#include "power-client.h"
#include <math.h>

/* Global references for updating */
static GtkWidget *_wifi_icon = NULL;
static GtkWidget *_bluetooth_icon = NULL;
static GtkWidget *_batt_icon = NULL;
static GtkWidget *_batt_label = NULL;

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

/* Battery Callback */
static void on_battery_update(gdouble percentage, gboolean charging, gint64 time_to_empty, gpointer user_data) {
    (void)time_to_empty; (void)user_data;
    
    if (!_batt_icon || !_batt_label) return;
    
    /* Update Label */
    int pct_int = (int)round(percentage);
    gchar *text = g_strdup_printf("%d%%", pct_int);
    gtk_label_set_text(GTK_LABEL(_batt_label), text);
    g_free(text);
    
    /* Determine Icon */
    /* Map 0-100 to 0, 10, 20... 100 */
    int level = (pct_int / 10) * 10;
    if (level == 0 && pct_int > 0) level = 10; /* Min 10 if not empty */
    
    const char *state = charging ? "-charging" : "";
    char icon_name[64];
    
    if (charging && pct_int >= 99) {
        snprintf(icon_name, sizeof(icon_name), "battery-level-100-charged-symbolic");
    } else {
        snprintf(icon_name, sizeof(icon_name), "battery-level-%d%s-symbolic", level, state);
    }
    
    gtk_image_set_from_icon_name(GTK_IMAGE(_batt_icon), icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
}

GtkWidget* create_system_icons(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    /* Initialize clients */
    network_client_init();
    /* power_client_init is called in venom-panel.c, but safe to call again if guarded, 
       otherwise we assume it's initialized. To be safe, rely on external init or add guard in client. 
       Actually, `ensure_proxy` guards it. */
    power_client_init(); 
    
    /* WiFi icon */
    _wifi_icon = gtk_image_new_from_icon_name("network-wireless-signal-excellent-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), _wifi_icon, FALSE, FALSE, 0);
    if (!wifi_client_is_enabled()) gtk_widget_set_no_show_all(_wifi_icon, TRUE);
    
    /* Bluetooth icon */
    _bluetooth_icon = gtk_image_new_from_icon_name("bluetooth-active-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), _bluetooth_icon, FALSE, FALSE, 0);
    if (!bluetooth_client_is_powered()) gtk_widget_set_no_show_all(_bluetooth_icon, TRUE);
    
    /* Battery Section */
    GtkWidget *batt_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    
    _batt_label = gtk_label_new("...");
    GtkStyleContext *vc = gtk_widget_get_style_context(_batt_label);
    gtk_style_context_add_class(vc, "battery-label");
    gtk_box_pack_start(GTK_BOX(batt_box), _batt_label, FALSE, FALSE, 0);
    
    _batt_icon = gtk_image_new_from_icon_name("battery-missing-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(batt_box), _batt_icon, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(box), batt_box, FALSE, FALSE, 0);
    
    /* Register Callback */
    power_client_on_battery_changed(on_battery_update, NULL);
    /* Initial Fetch */
    power_get_battery_info();
    
    /* Add CSS class */
    GtkStyleContext *context = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(context, "system-icons");
    
    /* Setup periodic update for NETWORK icons (Battery is pushed via signal) */
    g_timeout_add_seconds(2, update_system_icons, NULL);
    
    return box;
}
