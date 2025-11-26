/*
 * control-center.c
 *
 * venom-style Control Center for Linux panel.
 * Optimized for vaxp-os ecosystem.
 * * ✅ WORKING: Volume control (Non-blocking async call)
 * ✅ WORKING: Brightness control (sysfs with safety checks)
 * ✅ WORKING: Wi-Fi & Bluetooth status (NetworkManager + BlueZ D-Bus)
 */

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

/* NetworkManager & PulseAudio headers */
#include <NetworkManager.h>
/* Note: We use pactl via spawn for non-blocking UI, 
   but we keep the header if you want to extend functionality later */
#include <pulse/pulseaudio.h> 

#include "wifi-manager.h"
#include "bluetooth-manager.h"
#include "notification-service.h"
#include "brightness-manager.h"

// #include "control-center.h" /* Uncomment if you have this header */

/* Global flag to check if D-Bus is initialized */
static gboolean _dbus_initialized = FALSE;

/* Notifications UI Components */
static GtkWidget *_notifications_list = NULL;
static GtkWidget *_notifications_container = NULL;

/* Forward declarations */
static void on_volume_changed(GtkRange *range, gpointer data);
static void on_brightness_changed(GtkRange *range, gpointer data);
static void on_volume_changed(GtkRange *range, gpointer data);
static void on_brightness_changed(GtkRange *range, gpointer data);
static void on_notification_received(VenomNotification *notif, gpointer user_data);
static void on_wifi_clicked(GtkButton *button, gpointer data);
static void on_bluetooth_clicked(GtkButton *button, gpointer data);
static void show_bluetooth_dialog(void);
static gboolean on_bluetooth_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);

/* =====================================================================
 * BRIGHTNESS - Works if user has write permissions to sysfs
 * Tip: For vaxp-os, create a udev rule to allow group 'video' to write here.
 * ===================================================================== */

static void on_notification_received(VenomNotification *notif, gpointer user_data)
{
    (void)user_data;

    if (!_notifications_list) return;

    /* Create notification row */
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(row_box, 12);
    gtk_widget_set_margin_end(row_box, 12);
    gtk_widget_set_margin_top(row_box, 8);
    gtk_widget_set_margin_bottom(row_box, 8);

    /* App name + timestamp */
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row_box), header_box, FALSE, FALSE, 0);

    GtkWidget *app_label = gtk_label_new(notif->app_name);
    gtk_widget_set_halign(app_label, GTK_ALIGN_START);
    GtkStyleContext *app_ctx = gtk_widget_get_style_context(app_label);
    gtk_style_context_add_class(app_ctx, "notification-app");

    char time_str[64];
    time_t now = time(NULL);
    time_t diff = now - notif->timestamp;
    if (diff < 60) {
        snprintf(time_str, sizeof(time_str), "Just now");
    } else if (diff < 3600) {
        snprintf(time_str, sizeof(time_str), "%ld min ago", diff / 60);
    } else {
        snprintf(time_str, sizeof(time_str), "%ld hours ago", diff / 3600);
    }

    GtkWidget *time_label = gtk_label_new(time_str);
    gtk_widget_set_halign(time_label, GTK_ALIGN_END);
    GtkStyleContext *time_ctx = gtk_widget_get_style_context(time_label);
    gtk_style_context_add_class(time_ctx, "notification-time");

    gtk_box_pack_start(GTK_BOX(header_box), app_label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), time_label, FALSE, FALSE, 0);

    /* Summary (title) */
    GtkWidget *summary_label = gtk_label_new(notif->summary);
    gtk_widget_set_halign(summary_label, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(summary_label), TRUE);
    char *markup = g_strdup_printf("<b>%s</b>", notif->summary);
    gtk_label_set_markup(GTK_LABEL(summary_label), markup);
    g_free(markup);
    GtkStyleContext *summary_ctx = gtk_widget_get_style_context(summary_label);
    gtk_style_context_add_class(summary_ctx, "notification-summary");
    gtk_box_pack_start(GTK_BOX(row_box), summary_label, FALSE, FALSE, 0);

    /* Body text */
    if (notif->body && strlen(notif->body) > 0) {
        GtkWidget *body_label = gtk_label_new(notif->body);
        gtk_widget_set_halign(body_label, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(body_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(body_label), 35);
        GtkStyleContext *body_ctx = gtk_widget_get_style_context(body_label);
        gtk_style_context_add_class(body_ctx, "notification-body");
        gtk_box_pack_start(GTK_BOX(row_box), body_label, FALSE, FALSE, 0);
    }

    /* Separator */
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(row_box), separator, FALSE, FALSE, 0);

    /* Add to list at top (newest first) */
    gtk_box_pack_start(GTK_BOX(_notifications_list), row_box, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(_notifications_list), row_box, 0);
    gtk_widget_show_all(row_box);
}

/* =====================================================================
 * BRIGHTNESS - Delegated to brightness-manager
 * ===================================================================== */

static int get_brightness_percent(void)
{
    return brightness_manager_get_brightness();
}

static void set_brightness_percent(int percent)
{
    brightness_manager_set_brightness(percent);
}

/* =====================================================================
 * VOLUME - OPTIMIZED (Non-Blocking)
 * ===================================================================== */

/* * Optimized: Setting volume via PA Mainloop inside a GTK callback causes
 * UI lags/freezes. We use g_spawn_command_line_async for smooth sliding.
 */
static void set_volume_percent(int percent)
{
    char command[128];
    /* Using pactl is lighter and safer for the UI thread than raw PA API blocking calls */
    snprintf(command, sizeof(command), "pactl set-sink-volume @DEFAULT_SINK@ %d%%", percent);
    
    GError *error = NULL;
    if (!g_spawn_command_line_async(command, &error)) {
        g_warning("Failed to set volume: %s", error->message);
        g_error_free(error);
    }
}

static int get_volume_percent(void)
{
    /* Using popen allows reading the output easily */
    FILE *fp = popen("pactl get-sink-volume @DEFAULT_SINK@ | grep -oP '\\d+%' | head -1 | tr -d '%'", "r");
    if (!fp) return 0;
    
    char buf[16];
    int vol = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        vol = atoi(buf);
    }
    pclose(fp);
    return vol;
}

/* =====================================================================
 * Wi-Fi & Bluetooth - (NetworkManager + BlueZ D-Bus)
 * ===================================================================== */

static gboolean get_wifi_status(void)
{
    return wifi_manager_is_wireless_enabled();
}

static gboolean get_bluetooth_status(void)
{
    return bluetooth_manager_is_adapter_powered();
}

/* =====================================================================
 * UI COMPONENTS
 * ===================================================================== */

static GtkWidget *create_large_tile(const char *icon_name,
                                   const char *title,
                                   const char *subtitle,
                                   gboolean active)
{
    GtkWidget *button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_size_request(button, 150, 65); /* Adjusted size */

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_container_add(GTK_CONTAINER(button), box);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_DND);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);

    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    /* Center text vertically */
    gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), text_box, TRUE, TRUE, 0);

    GtkWidget *title_label = gtk_label_new(title);
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    GtkStyleContext *title_ctx = gtk_widget_get_style_context(title_label);
    gtk_style_context_add_class(title_ctx, "tile-title");
    gtk_box_pack_start(GTK_BOX(text_box), title_label, FALSE, FALSE, 0);

    GtkWidget *subtitle_label = gtk_label_new(subtitle);
    gtk_widget_set_halign(subtitle_label, GTK_ALIGN_START);
    GtkStyleContext *sub_ctx = gtk_widget_get_style_context(subtitle_label);
    gtk_style_context_add_class(sub_ctx, "tile-subtitle");
    gtk_box_pack_start(GTK_BOX(text_box), subtitle_label, FALSE, FALSE, 0);

    GtkStyleContext *ctx = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(ctx, "large-tile");
    if (active) gtk_style_context_add_class(ctx, "active");

    /* Store label for updates */
    g_object_set_data(G_OBJECT(button), "subtitle_label", subtitle_label);

    return button;
}

static GtkWidget *create_small_tile(const char *icon_name, const char *title)
{
    GtkWidget *button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_size_request(button, 70, 70);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(button), box);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new(title);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 10);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    GtkStyleContext *lbl_ctx = gtk_widget_get_style_context(label);
    gtk_style_context_add_class(lbl_ctx, "small-tile-label");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    GtkStyleContext *ctx = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(ctx, "small-tile");
    return button;
}

static GtkWidget *create_slider_section(const char *icon_name,
                                        const char *title,
                                        GCallback callback)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    
    /* Set initial values */
    if (g_strcmp0(title, "volume") == 0) {
        int cur = get_volume_percent();
        gtk_range_set_value(GTK_RANGE(scale), cur);
    } else {
        int cur = get_brightness_percent();
        gtk_range_set_value(GTK_RANGE(scale), cur);
    }

    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);
    
    GtkStyleContext *ctx = gtk_widget_get_style_context(scale);
    gtk_style_context_add_class(ctx, "cc-slider");
    
    if (callback) {
        g_signal_connect(scale, "value-changed", callback, NULL);
    }
    
    gtk_box_pack_start(GTK_BOX(box), scale, TRUE, TRUE, 0);
    return box;
}

/* =====================================================================
 * MAIN WINDOW
 * ===================================================================== */

GtkWidget *create_control_center(void)
{
    /* Initialize notification service with callback */
    notification_service_init(on_notification_received, NULL);
    
    /* Initialize Wi-Fi and Bluetooth managers */
    wifi_manager_init();
    /* Initialize Wi-Fi and Bluetooth managers */
    wifi_manager_init();
    bluetooth_manager_init();
    brightness_manager_init();
    
    _dbus_initialized = TRUE;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* ✅ 1. تفعيل الشفافية (Transparency Logic) */
    GdkScreen *screen = gtk_widget_get_screen(window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    
    if (visual != NULL && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(window, visual);
        gtk_widget_set_app_paintable(window, TRUE);
    }
    /* ✅ نهاية كود الشفافية */

    gtk_window_set_title(GTK_WINDOW(window), "Control Center");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window), 340, 520);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);

    /* Positioning logic (استخدمنا المتغير screen المعرف في الأعلى) */
    gint screen_width = gdk_screen_get_width(screen);
    gtk_window_move(GTK_WINDOW(window), screen_width - 360, 40);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    /* Row 1: Wi-Fi & Bluetooth */
    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(main_box), row1, FALSE, FALSE, 0);
    
    gboolean wifi_on = get_wifi_status();
    GtkWidget *wifi = create_large_tile("network-wireless-symbolic", "Wi-Fi", 
                                        wifi_on ? "On" : "Off", wifi_on);
    g_signal_connect(wifi, "clicked", G_CALLBACK(on_wifi_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(row1), wifi, TRUE, TRUE, 0);
    
    gboolean bt_on = get_bluetooth_status();
    GtkWidget *bluetooth = create_large_tile("bluetooth-symbolic", "Bluetooth", 
                                             bt_on ? "On" : "Off", bt_on);
    g_signal_connect(bluetooth, "clicked", G_CALLBACK(on_bluetooth_clicked), NULL);
    g_signal_connect(bluetooth, "button-press-event", G_CALLBACK(on_bluetooth_button_press), NULL);
    gtk_box_pack_start(GTK_BOX(row1), bluetooth, TRUE, TRUE, 0);

    /* Row 2: AirDrop (Placeholder for vaxp-os share feature) */
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(main_box), row2, FALSE, FALSE, 0);
    GtkWidget *airdrop = create_large_tile("folder-symbolic", "AirDrop", "Everyone", FALSE);
    gtk_box_pack_start(GTK_BOX(row2), airdrop, TRUE, TRUE, 0);

    /* Separators & Sliders */
    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_box), sep1, FALSE, FALSE, 8);

    /* Notifications Section Header */
    GtkWidget *notif_header = gtk_label_new("Notifications");
    gtk_widget_set_halign(notif_header, GTK_ALIGN_START);
    gtk_widget_set_margin_start(notif_header, 16);
    char *header_markup = g_strdup_printf("<b>%s</b>", "Notifications");
    gtk_label_set_markup(GTK_LABEL(notif_header), header_markup);
    g_free(header_markup);
    GtkStyleContext *header_ctx = gtk_widget_get_style_context(notif_header);
    gtk_style_context_add_class(header_ctx, "section-header");
    gtk_box_pack_start(GTK_BOX(main_box), notif_header, FALSE, FALSE, 0);

    /* Scrollable notifications list */
    GtkWidget *notif_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(notif_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(notif_scroll, -1, 150);
    gtk_box_pack_start(GTK_BOX(main_box), notif_scroll, FALSE, FALSE, 0);

    /* Notifications list container */
    _notifications_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(notif_scroll), _notifications_list);

    /* Empty state label */
    GtkWidget *empty_label = gtk_label_new("No notifications yet");
    gtk_style_context_add_class(gtk_widget_get_style_context(empty_label), "dim-label");
    gtk_box_pack_start(GTK_BOX(_notifications_list), empty_label, FALSE, FALSE, 0);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_box), sep2, FALSE, FALSE, 8);

    /* Brightness */
    GtkWidget *brightness_section = create_slider_section("display-brightness-symbolic", "brightness", G_CALLBACK(on_brightness_changed));
    gtk_box_pack_start(GTK_BOX(main_box), brightness_section, FALSE, FALSE, 0);

    GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_box), sep3, FALSE, FALSE, 8);

    /* Volume */
    GtkWidget *sound_section = create_slider_section("audio-volume-high-symbolic", "volume", G_CALLBACK(on_volume_changed));
    gtk_box_pack_start(GTK_BOX(main_box), sound_section, FALSE, FALSE, 0);

    /* CSS Class for styling */
    GtkStyleContext *ctx = gtk_widget_get_style_context(window);
    gtk_style_context_add_class(ctx, "control-center");

    /* Cleanup on window destroy */
    /* Cleanup on window destroy */
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(wifi_manager_cleanup), NULL);
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(bluetooth_manager_cleanup), NULL);
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(brightness_manager_cleanup), NULL);
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(notification_service_cleanup), NULL);

    return window;
}

/* =====================================================================
 * CALLBACKS
 * ===================================================================== */

static void on_volume_changed(GtkRange *range, gpointer data)
{
    (void)data;
    /* This function is now non-blocking and safe */
    int percent = (int)gtk_range_get_value(range);
    set_volume_percent(percent);
}

static void on_brightness_changed(GtkRange *range, gpointer data)
{
    (void)data;
    int percent = (int)gtk_range_get_value(range);
    set_brightness_percent(percent);
}

static void on_wifi_clicked(GtkButton *button, gpointer data)
{
    (void)data;
    gboolean current = wifi_manager_is_wireless_enabled();
    gboolean new_state = !current;
    
    wifi_manager_set_wireless_enabled(new_state);
    
    /* Update UI */
    GtkWidget *label = g_object_get_data(G_OBJECT(button), "subtitle_label");
    if (label) {
        gtk_label_set_text(GTK_LABEL(label), new_state ? "On" : "Off");
    }
    
    GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(button));
    if (new_state) {
        gtk_style_context_add_class(ctx, "active");
    } else {
        gtk_style_context_remove_class(ctx, "active");
    }
}

static void on_bluetooth_clicked(GtkButton *button, gpointer data)
{
    (void)data;
    gboolean current = bluetooth_manager_is_adapter_powered();
    gboolean new_state = !current;
    
    bluetooth_manager_set_adapter_powered(new_state);
    
    /* Update UI */
    GtkWidget *label = g_object_get_data(G_OBJECT(button), "subtitle_label");
    if (label) {
        gtk_label_set_text(GTK_LABEL(label), new_state ? "On" : "Off");
    }
    
    GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(button));
    if (new_state) {
        gtk_style_context_add_class(ctx, "active");
    } else {
        gtk_style_context_remove_class(ctx, "active");
    }
}

static gboolean on_bluetooth_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        show_bluetooth_dialog();
        return TRUE;
    }
    return FALSE;
}

static GtkWidget *_bt_dialog = NULL;
static GtkWidget *_bt_list_box = NULL;

static void on_device_connect_clicked(GtkButton *btn, gpointer data) {
    BluetoothDevice *device = (BluetoothDevice *)data;
    if (!device) return;
    
    if (device->connected) {
        bluetooth_manager_disconnect_device(device->address);
    } else {
        bluetooth_manager_connect_device(device->address);
    }
}

static void update_device_list(BluetoothDeviceList *list, gpointer user_data) {
    (void)user_data;
    if (!_bt_list_box) return;
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(_bt_list_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (list->count == 0) {
        GtkWidget *lbl = gtk_label_new("No devices found");
        gtk_box_pack_start(GTK_BOX(_bt_list_box), lbl, FALSE, FALSE, 10);
        gtk_widget_show(lbl);
        return;
    }

    for (guint i = 0; i < list->count; i++) {
        BluetoothDevice *dev = &list->devices[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        
        GtkWidget *lbl = gtk_label_new(dev->name);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        
        GtkWidget *btn = gtk_button_new_with_label(dev->connected ? "Disconnect" : "Connect");
        
        BluetoothDevice *dev_copy = g_new0(BluetoothDevice, 1);
        dev_copy->name = g_strdup(dev->name);
        dev_copy->address = g_strdup(dev->address);
        dev_copy->connected = dev->connected;
        dev_copy->paired = dev->paired;
        g_object_set_data_full(G_OBJECT(btn), "device", dev_copy, g_free);
        
        g_signal_connect(btn, "clicked", G_CALLBACK(on_device_connect_clicked), dev_copy);

        gtk_box_pack_start(GTK_BOX(row), lbl, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(_bt_list_box), row, FALSE, FALSE, 5);
        gtk_widget_show_all(row);
    }
}

static void on_bt_scan_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    gboolean scanning = gtk_toggle_button_get_active(btn);
    
    if (scanning) {
        bluetooth_manager_start_scan();
    } else {
        bluetooth_manager_stop_scan();
    }
}

static void show_bluetooth_dialog(void) {
    if (_bt_dialog) {
        gtk_window_present(GTK_WINDOW(_bt_dialog));
        return;
    }

    _bt_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(_bt_dialog), "Bluetooth Devices");
    gtk_window_set_default_size(GTK_WINDOW(_bt_dialog), 300, 400);
    gtk_window_set_position(GTK_WINDOW(_bt_dialog), GTK_WIN_POS_CENTER);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(_bt_dialog), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *scan_btn = gtk_toggle_button_new_with_label("Scan");
    g_signal_connect(scan_btn, "toggled", G_CALLBACK(on_bt_scan_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scan_btn, FALSE, FALSE, 0);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    _bt_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled), _bt_list_box);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    g_signal_connect(_bt_dialog, "destroy", G_CALLBACK(gtk_widget_destroyed), &_bt_dialog);
    
    BluetoothDeviceList *list = bluetooth_manager_get_devices();
    update_device_list(list, NULL);
    bluetooth_manager_free_devices(list);

    gtk_widget_show_all(_bt_dialog);
}
