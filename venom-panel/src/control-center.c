/*
 * control-center.c
 *
 * venom-style Control Center for Linux panel.
 * Optimized for vaxp-os ecosystem.
 * ✅ WORKING: Volume control (Non-blocking async call)
 * ✅ WORKING: Brightness control (sysfs with safety checks)
 * ✅ WORKING: Notifications (D-Bus notification service)
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

#include "notification-client.h"
#include "brightness-manager.h"
#include "network-client.h"
#include "shot-client.h"
#include "shot-client.h"

// #include "control-center.h" /* Uncomment if you have this header */

/* Global flag to check if D-Bus is initialized */
static gboolean _dbus_initialized = FALSE;

/* Notifications UI Components */
static GtkWidget *_notifications_list = NULL;
static GtkWidget *_notifications_container = NULL;

/* Network tile buttons */
static GtkWidget *_wifi_button = NULL;
static GtkWidget *_bluetooth_button = NULL;

/* Forward declarations */
static void on_volume_changed(GtkRange *range, gpointer data);
static void on_brightness_changed(GtkRange *range, gpointer data);
static void on_history_updated(gpointer user_data);
static void refresh_notifications_list(void);
static void on_wifi_clicked(GtkButton *button, gpointer data);
static gboolean on_wifi_button_press(GtkWidget *button, GdkEventButton *event, gpointer data);
static void on_bluetooth_clicked(GtkButton *button, gpointer data);
static gboolean on_bluetooth_button_press(GtkWidget *button, GdkEventButton *event, gpointer data);
static void on_ethernet_clicked(GtkButton *button, gpointer data);
static gboolean on_ethernet_button_press(GtkWidget *button, GdkEventButton *event, gpointer data);
static void on_dnd_clicked(GtkButton *button, gpointer data);
static void on_clear_notifications_clicked(GtkButton *button, gpointer data);
static void on_dnd_changed(gboolean enabled, gpointer data);

/* DND button reference */
static GtkWidget *_dnd_button = NULL;


/* =====================================================================
 * BRIGHTNESS - Works if user has write permissions to sysfs
 * Tip: For vaxp-os, create a udev rule to allow group 'video' to write here.
 * ===================================================================== */

/* Callback when notification history updates */
static void on_history_updated(gpointer user_data) {
    (void)user_data;
    refresh_notifications_list();
}

/* Build a notification row widget */
static GtkWidget* create_notification_row(NotificationItem *notif) {
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(row_box, 12);
    gtk_widget_set_margin_end(row_box, 12);
    gtk_widget_set_margin_top(row_box, 8);
    gtk_widget_set_margin_bottom(row_box, 8);

    /* App name header */
    GtkWidget *app_label = gtk_label_new(notif->app_name);
    gtk_widget_set_halign(app_label, GTK_ALIGN_START);
    GtkStyleContext *app_ctx = gtk_widget_get_style_context(app_label);
    gtk_style_context_add_class(app_ctx, "notification-app");
    gtk_box_pack_start(GTK_BOX(row_box), app_label, FALSE, FALSE, 0);

    /* Summary (title) */
    if (notif->summary && strlen(notif->summary) > 0) {
        GtkWidget *summary_label = gtk_label_new(NULL);
        gtk_widget_set_halign(summary_label, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(summary_label), TRUE);
        char *markup = g_strdup_printf("<b>%s</b>", notif->summary);
        gtk_label_set_markup(GTK_LABEL(summary_label), markup);
        g_free(markup);
        gtk_box_pack_start(GTK_BOX(row_box), summary_label, FALSE, FALSE, 0);
    }

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

    return row_box;
}

/* Refresh notifications list from daemon */
static void refresh_notifications_list(void) {
    if (!_notifications_list) return;
    
    /* Clear existing children */
    GList *children = gtk_container_get_children(GTK_CONTAINER(_notifications_list));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    
    /* Get history from daemon */
    GList *history = notification_client_get_history();
    
    if (history == NULL) {
        GtkWidget *empty = gtk_label_new("No notifications");
        gtk_widget_set_margin_top(empty, 20);
        gtk_box_pack_start(GTK_BOX(_notifications_list), empty, FALSE, FALSE, 0);
        gtk_widget_show(empty);
        return;
    }
    
    for (GList *l = history; l != NULL; l = l->next) {
        NotificationItem *item = (NotificationItem *)l->data;
        GtkWidget *row = create_notification_row(item);
        gtk_box_pack_start(GTK_BOX(_notifications_list), row, FALSE, FALSE, 0);
        gtk_widget_show_all(row);
    }
    
    notification_item_list_free(history);
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

/* =====================================================================
 * TABS & PAGES
 * ===================================================================== */

static GtkWidget *_stack = NULL;
static GtkWidget *_controls_btn = NULL;
static GtkWidget *_notifs_btn = NULL;

static void on_tab_switch(GtkButton *btn, gpointer data) {
    const char *page_name = (const char *)data;
    if (_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(_stack), page_name);
    }
    
    /* Update styling for active tab */
    GtkStyleContext *ctx_c = gtk_widget_get_style_context(_controls_btn);
    GtkStyleContext *ctx_n = gtk_widget_get_style_context(_notifs_btn);
    
    gtk_style_context_remove_class(ctx_c, "active-tab");
    gtk_style_context_remove_class(ctx_n, "active-tab");
    
    if (g_strcmp0(page_name, "controls") == 0) {
        gtk_style_context_add_class(ctx_c, "active-tab");
    } else {
        gtk_style_context_add_class(ctx_n, "active-tab");
    }
}

static GtkWidget* create_controls_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);

    /* Row 1: Wi-Fi & Bluetooth */
    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(box), row1, FALSE, FALSE, 0);
    
    gboolean wifi_on = wifi_client_is_enabled();
    GtkWidget *wifi = create_large_tile("network-wireless-symbolic", "Wi-Fi", 
                                        wifi_on ? "On" : "Off", wifi_on);
    g_signal_connect(wifi, "clicked", G_CALLBACK(on_wifi_clicked), NULL);
    g_signal_connect(wifi, "button-press-event", G_CALLBACK(on_wifi_button_press), NULL);
    _wifi_button = wifi;
    gtk_box_pack_start(GTK_BOX(row1), wifi, TRUE, TRUE, 0);
    
    gboolean bt_on = bluetooth_client_is_powered();
    GtkWidget *bluetooth = create_large_tile("bluetooth-symbolic", "Bluetooth", 
                                             bt_on ? "On" : "Off", bt_on);
    g_signal_connect(bluetooth, "clicked", G_CALLBACK(on_bluetooth_clicked), NULL);
    g_signal_connect(bluetooth, "button-press-event", G_CALLBACK(on_bluetooth_button_press), NULL);
    _bluetooth_button = bluetooth;
    gtk_box_pack_start(GTK_BOX(row1), bluetooth, TRUE, TRUE, 0);

    /* Row 2: Ethernet & Do Not Disturb */
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(box), row2, FALSE, FALSE, 0);
    
    gboolean eth_on = ethernet_client_is_connected();
    GtkWidget *ethernet = create_large_tile("network-wired-symbolic", "Ethernet", 
                                            eth_on ? "Connected" : "Off", eth_on);
    g_signal_connect(ethernet, "clicked", G_CALLBACK(on_ethernet_clicked), NULL);
    g_signal_connect(ethernet, "button-press-event", G_CALLBACK(on_ethernet_button_press), NULL);
    gtk_box_pack_start(GTK_BOX(row2), ethernet, TRUE, TRUE, 0);
    
    gboolean dnd_on = notification_client_get_dnd();
    GtkWidget *dnd = create_large_tile("preferences-system-notifications-symbolic", "Do Not Disturb", 
                                       dnd_on ? "On" : "Off", dnd_on);
    g_signal_connect(dnd, "clicked", G_CALLBACK(on_dnd_clicked), NULL);
    _dnd_button = dnd;
    gtk_box_pack_start(GTK_BOX(row2), dnd, TRUE, TRUE, 0);

    /* Separators & Sliders */
    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), sep1, FALSE, FALSE, 8);

    /* Brightness */
    GtkWidget *brightness_section = create_slider_section("display-brightness-symbolic", "brightness", G_CALLBACK(on_brightness_changed));
    gtk_box_pack_start(GTK_BOX(box), brightness_section, FALSE, FALSE, 0);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), sep2, FALSE, FALSE, 8);

    /* Volume */
    GtkWidget *sound_section = create_slider_section("audio-volume-high-symbolic", "volume", G_CALLBACK(on_volume_changed));
    gtk_box_pack_start(GTK_BOX(box), sound_section, FALSE, FALSE, 0);

    /* Separator */
    GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), sep3, FALSE, FALSE, 8);

    /* Row 3: Capture Tools */
    GtkWidget *row3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(row3, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), row3, FALSE, FALSE, 0);

    /* Full Screenshot */
    GtkWidget *btn_shot_full = create_small_tile("camera-photo-symbolic", "Full Shot");
    g_signal_connect(btn_shot_full, "clicked", G_CALLBACK(shot_take_full_screenshot), NULL);
    gtk_box_pack_start(GTK_BOX(row3), btn_shot_full, FALSE, FALSE, 0);

    /* Region Screenshot */
    GtkWidget *btn_shot_region = create_small_tile("transform-crop-symbolic", "Area Shot");
    g_signal_connect(btn_shot_region, "clicked", G_CALLBACK(shot_take_region_screenshot), NULL);
    gtk_box_pack_start(GTK_BOX(row3), btn_shot_region, FALSE, FALSE, 0);

    /* Full Record */
    GtkWidget *btn_rec_full = create_small_tile("camera-video-symbolic", "Full Rec");
    g_signal_connect(btn_rec_full, "clicked", G_CALLBACK(shot_start_full_record), NULL);
    gtk_box_pack_start(GTK_BOX(row3), btn_rec_full, FALSE, FALSE, 0);

    /* Region Record */
    GtkWidget *btn_rec_region = create_small_tile("media-record-symbolic", "Area Rec");
    g_signal_connect(btn_rec_region, "clicked", G_CALLBACK(shot_start_region_record), NULL);
    gtk_box_pack_start(GTK_BOX(row3), btn_rec_region, FALSE, FALSE, 0);
    
    return box;
}

static GtkWidget* create_notifications_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Header Container */
    GtkWidget *header_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(header_container, 16);
    gtk_widget_set_margin_end(header_container, 16);
    gtk_widget_set_margin_top(header_container, 12);
    gtk_widget_set_margin_bottom(header_container, 8);
    gtk_box_pack_start(GTK_BOX(box), header_container, FALSE, FALSE, 0);

    /* Notifications Title */
    GtkWidget *title = gtk_label_new("Recent");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    GtkStyleContext *ctx = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(ctx, "section-header");
    gtk_box_pack_start(GTK_BOX(header_container), title, TRUE, TRUE, 0);

    /* Clear Button */
    GtkWidget *clear_btn = gtk_button_new_from_icon_name("edit-clear-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text(clear_btn, "Clear All");
    gtk_widget_set_halign(clear_btn, GTK_ALIGN_END);
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_notifications_clicked), NULL);
    GtkStyleContext *clear_ctx = gtk_widget_get_style_context(clear_btn);
    gtk_style_context_add_class(clear_ctx, GTK_STYLE_CLASS_FLAT);
    gtk_box_pack_end(GTK_BOX(header_container), clear_btn, FALSE, FALSE, 0);

    /* Scrollable Area */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
    
    /* List Container */
    _notifications_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(scroll), _notifications_list);
    
    /* Initial Load */
    refresh_notifications_list();
    
    return box;
}

/* =====================================================================
 * MAIN WINDOW
 * ===================================================================== */

GtkWidget *create_control_center(void)
{
    /* Initialize clients */
    brightness_manager_init();
    network_client_init();
    notification_client_init();
    shot_client_init();
    notification_client_on_history_update(on_history_updated, NULL);
    
    _dbus_initialized = TRUE;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* ✅ 1. Activate Transparency */
    GdkScreen *screen = gtk_widget_get_screen(window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    
    if (visual != NULL && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(window, visual);
        gtk_widget_set_app_paintable(window, TRUE);
    }
    /* ✅ End Transparency */

    gtk_window_set_title(GTK_WINDOW(window), "Control Center");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window), 360, 560); /* Slightly larger for tabs */
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);

    /* Positioning logic */
    gint screen_width = gdk_screen_get_width(screen);
    gtk_window_move(GTK_WINDOW(window), screen_width - 380, 40);

    /* Main Container */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    /* --- HEADER BAR (Venom Label + Tabs) --- */
    GtkWidget *header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(header_bar, 16);
    gtk_widget_set_margin_bottom(header_bar, 8);
    gtk_widget_set_margin_start(header_bar, 16);
    gtk_widget_set_margin_end(header_bar, 16);
    gtk_box_pack_start(GTK_BOX(main_box), header_bar, FALSE, FALSE, 0);

    /* Branding */
    GtkWidget *brand_label = gtk_label_new("▼ Venom");
    GtkStyleContext *brand_ctx = gtk_widget_get_style_context(brand_label);
    gtk_style_context_add_class(brand_ctx, "brand-label");
    gtk_box_pack_start(GTK_BOX(header_bar), brand_label, FALSE, FALSE, 0);

    /* Tab Switcher (Right Aligned) */
    GtkWidget *tabs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(tabs_box, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(header_bar), tabs_box, FALSE, FALSE, 0);
    
    _controls_btn = gtk_button_new_with_label("Controls");
    _notifs_btn = gtk_button_new_with_label("Notifications");
    
    /* Make buttons look like labels/tabs */
    gtk_style_context_add_class(gtk_widget_get_style_context(_controls_btn), "tab-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(_notifs_btn), "tab-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(_controls_btn), "active-tab"); /* Default active */
    
    g_signal_connect(_controls_btn, "clicked", G_CALLBACK(on_tab_switch), "controls");
    g_signal_connect(_notifs_btn, "clicked", G_CALLBACK(on_tab_switch), "notifications");
    
    gtk_box_pack_start(GTK_BOX(tabs_box), _controls_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tabs_box), _notifs_btn, FALSE, FALSE, 0);

    /* --- STACK --- */
    _stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(_stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(_stack), 250);
    gtk_box_pack_start(GTK_BOX(main_box), _stack, TRUE, TRUE, 0);
    
    /* Add Pages */
    gtk_stack_add_named(GTK_STACK(_stack), create_controls_page(), "controls");
    gtk_stack_add_named(GTK_STACK(_stack), create_notifications_page(), "notifications");

    /* CSS Class for styling */
    GtkStyleContext *ctx = gtk_widget_get_style_context(window);
    gtk_style_context_add_class(ctx, "control-center");

    /* Cleanup on window destroy */
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(brightness_manager_cleanup), NULL);
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(notification_client_cleanup), NULL);
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(network_client_cleanup), NULL);
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(shot_client_cleanup), NULL);
    
    /* Register for DND updates */
    notification_client_on_dnd_change(on_dnd_changed, NULL);

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

/* =====================================================================
 * WIFI POPUP MENU
 * ===================================================================== */

/* Password dialog for secured networks */
static gchar* show_password_dialog(GtkWidget *parent, const gchar *ssid) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Enter Password",
        GTK_WINDOW(gtk_widget_get_toplevel(parent)),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Connect", GTK_RESPONSE_ACCEPT,
        NULL);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);
    
    /* Network name label */
    gchar *markup = g_strdup_printf("<b>%s</b>", ssid);
    GtkWidget *ssid_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(ssid_label), markup);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(content), ssid_label, FALSE, FALSE, 5);
    
    /* Password entry */
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 5);
    
    /* Show password checkbox */
    GtkWidget *show_pass = gtk_check_button_new_with_label("Show password");
    g_signal_connect_swapped(show_pass, "toggled", G_CALLBACK(gtk_entry_set_visibility), entry);
    gtk_box_pack_start(GTK_BOX(content), show_pass, FALSE, FALSE, 5);
    
    gtk_widget_show_all(dialog);
    
    gchar *password = NULL;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        password = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
    
    gtk_widget_destroy(dialog);
    return password;
}

/* Connect to network menu item callback */
static void on_network_item_activate(GtkMenuItem *item, gpointer user_data) {
    WiFiNetwork *net = (WiFiNetwork *)user_data;
    
    if (net->connected) {
        /* Disconnect */
        wifi_client_disconnect();
    } else {
        /* Connect */
        if (net->secured) {
            gchar *password = show_password_dialog(GTK_WIDGET(item), net->ssid);
            if (password) {
                wifi_client_connect(net->ssid, password);
                g_free(password);
            }
        } else {
            wifi_client_connect(net->ssid, NULL);
        }
    }
    
    /* Update WiFi button UI */
    if (_wifi_button) {
        gboolean enabled = wifi_client_is_enabled();
        GtkWidget *label = g_object_get_data(G_OBJECT(_wifi_button), "subtitle_label");
        if (label) {
            gchar *ssid = wifi_client_get_ssid();
            gtk_label_set_text(GTK_LABEL(label), ssid ? ssid : (enabled ? "On" : "Off"));
            g_free(ssid);
        }
    }
}

/* Build WiFi popup menu */
static void show_wifi_popup_menu(GtkWidget *button, GdkEventButton *event) {
    GtkWidget *menu = gtk_menu_new();
    
    /* Scan / Refresh header */
    GtkWidget *scan_item = gtk_menu_item_new_with_label("🔄 Scan Networks");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), scan_item);
    
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    
    /* Get network list */
    GList *networks = wifi_client_get_networks();
    
    if (networks == NULL) {
        GtkWidget *no_networks = gtk_menu_item_new_with_label("No networks found");
        gtk_widget_set_sensitive(no_networks, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), no_networks);
    } else {
        for (GList *l = networks; l != NULL; l = l->next) {
            WiFiNetwork *net = (WiFiNetwork *)l->data;
            
            /* Create label with signal strength icon */
            gchar *icon = "📶";
            if (net->strength < 25) icon = "▁";
            else if (net->strength < 50) icon = "▂";
            else if (net->strength < 75) icon = "▃";
            
            gchar *label_text = g_strdup_printf("%s %s%s%s",
                icon,
                net->ssid,
                net->secured ? " 🔒" : "",
                net->connected ? " ✓" : "");
            
            GtkWidget *item = gtk_menu_item_new_with_label(label_text);
            g_free(label_text);
            
            /* Store network data and connect signal */
            g_object_set_data_full(G_OBJECT(item), "network", net, NULL);
            g_signal_connect(item, "activate", G_CALLBACK(on_network_item_activate), net);
            
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        }
        
        /* Don't free individual networks - they're stored in menu items */
        g_list_free(networks);
    }
    
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent*)event);
}

/* Handle right-click on WiFi button */
static gboolean on_wifi_button_press(GtkWidget *button, GdkEventButton *event, gpointer data) {
    (void)data;
    
    if (event->button == 3) {  /* Right click */
        show_wifi_popup_menu(button, event);
        return TRUE;
    }
    return FALSE;
}

static void on_wifi_clicked(GtkButton *button, gpointer data)
{
    (void)data;
    gboolean current = wifi_client_is_enabled();
    gboolean new_state = !current;
    
    wifi_client_set_enabled(new_state);
    
    /* Update UI */
    GtkWidget *label = g_object_get_data(G_OBJECT(button), "subtitle_label");
    if (label) {
        if (new_state) {
            gchar *ssid = wifi_client_get_ssid();
            gtk_label_set_text(GTK_LABEL(label), ssid ? ssid : "On");
            g_free(ssid);
        } else {
            gtk_label_set_text(GTK_LABEL(label), "Off");
        }
    }
    
    GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(button));
    if (new_state) {
        gtk_style_context_add_class(ctx, "active");
    } else {
        gtk_style_context_remove_class(ctx, "active");
    }
}

/* =====================================================================
 * BLUETOOTH POPUP MENU
 * ===================================================================== */


static gboolean _bt_scanning = FALSE;

/* Bluetooth device menu item callback */
static void on_bt_device_item_activate(GtkMenuItem *item, gpointer user_data) {
    BluetoothDevice *dev = (BluetoothDevice *)user_data;
    (void)item;
    
    if (dev->connected) {
        /* Disconnect */
        bluetooth_client_disconnect(dev->address);
    } else if (dev->paired) {
        /* Connect */
        bluetooth_client_connect(dev->address);
    } else {
        /* Pair first */
        if (bluetooth_client_pair(dev->address)) {
            bluetooth_client_connect(dev->address);
        }
    }
}

/* Scan toggle callback */
static void on_bt_scan_toggled(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    
    if (_bt_scanning) {
        bluetooth_client_stop_scan();
        _bt_scanning = FALSE;
    } else {
        bluetooth_client_start_scan();
        _bt_scanning = TRUE;
    }
}

/* Build Bluetooth popup menu */
static void show_bluetooth_popup_menu(GtkWidget *button, GdkEventButton *event) {
    GtkWidget *menu = gtk_menu_new();
    
    /* Scan toggle */
    gchar *scan_label = g_strdup_printf("%s Scan", _bt_scanning ? "⏹ Stop" : "🔍 Start");
    GtkWidget *scan_item = gtk_menu_item_new_with_label(scan_label);
    g_signal_connect(scan_item, "activate", G_CALLBACK(on_bt_scan_toggled), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), scan_item);
    g_free(scan_label);
    
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    
    /* Get device list */
    GList *devices = bluetooth_client_get_devices();
    
    if (devices == NULL) {
        GtkWidget *no_devices = gtk_menu_item_new_with_label("No devices found");
        gtk_widget_set_sensitive(no_devices, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), no_devices);
    } else {
        for (GList *l = devices; l != NULL; l = l->next) {
            BluetoothDevice *dev = (BluetoothDevice *)l->data;
            
            /* Create label with status icons */
            gchar *status = "";
            if (dev->connected) status = " 🔗";
            else if (dev->paired) status = " ✓";
            
            gchar *icon = "📱";
            if (dev->icon) {
                if (g_strstr_len(dev->icon, -1, "audio")) icon = "🎧";
                else if (g_strstr_len(dev->icon, -1, "input")) icon = "⌨️";
                else if (g_strstr_len(dev->icon, -1, "phone")) icon = "📱";
                else if (g_strstr_len(dev->icon, -1, "computer")) icon = "💻";
            }
            
            gchar *label_text = g_strdup_printf("%s %s%s", icon, dev->name, status);
            
            GtkWidget *item = gtk_menu_item_new_with_label(label_text);
            g_free(label_text);
            
            g_object_set_data_full(G_OBJECT(item), "device", dev, NULL);
            g_signal_connect(item, "activate", G_CALLBACK(on_bt_device_item_activate), dev);
            
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        }
        
        /* Don't free individual devices - they're stored in menu items */
        g_list_free(devices);
    }
    
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent*)event);
}

/* Handle right-click on Bluetooth button */
static gboolean on_bluetooth_button_press(GtkWidget *button, GdkEventButton *event, gpointer data) {
    (void)data;
    
    if (event->button == 3) {  /* Right click */
        show_bluetooth_popup_menu(button, event);
        return TRUE;
    }
    return FALSE;
}

static void on_bluetooth_clicked(GtkButton *button, gpointer data)
{
    (void)data;
    gboolean current = bluetooth_client_is_powered();
    gboolean new_state = !current;
    
    bluetooth_client_set_powered(new_state);
    
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

/* =====================================================================
 * ETHERNET POPUP MENU
 * ===================================================================== */

/* Build Ethernet popup menu */
static void show_ethernet_popup_menu(GtkWidget *button, GdkEventButton *event) {
    GtkWidget *menu = gtk_menu_new();
    
    /* Get interface list */
    GList *interfaces = ethernet_client_get_interfaces();
    
    if (interfaces == NULL) {
        GtkWidget *no_iface = gtk_menu_item_new_with_label("No interfaces found");
        gtk_widget_set_sensitive(no_iface, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), no_iface);
    } else {
        for (GList *l = interfaces; l != NULL; l = l->next) {
            EthernetInterface *iface = (EthernetInterface *)l->data;
            
            /* Interface header */
            gchar *status = iface->connected ? "🟢" : "⚫";
            gchar *header = g_strdup_printf("%s %s (%s)", status, iface->name, 
                                           iface->connected ? "Connected" : "Disconnected");
            GtkWidget *header_item = gtk_menu_item_new_with_label(header);
            gtk_widget_set_sensitive(header_item, FALSE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), header_item);
            g_free(header);
            
            if (iface->connected && iface->ip_address && strlen(iface->ip_address) > 0) {
                /* IP Address */
                gchar *ip_label = g_strdup_printf("   📍 IP: %s", iface->ip_address);
                GtkWidget *ip_item = gtk_menu_item_new_with_label(ip_label);
                gtk_widget_set_sensitive(ip_item, FALSE);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), ip_item);
                g_free(ip_label);
                
                /* Gateway */
                if (iface->gateway && strlen(iface->gateway) > 0) {
                    gchar *gw_label = g_strdup_printf("   🌐 Gateway: %s", iface->gateway);
                    GtkWidget *gw_item = gtk_menu_item_new_with_label(gw_label);
                    gtk_widget_set_sensitive(gw_item, FALSE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gw_item);
                    g_free(gw_label);
                }
                
                /* MAC Address */
                if (iface->mac_address && strlen(iface->mac_address) > 0) {
                    gchar *mac_label = g_strdup_printf("   🔗 MAC: %s", iface->mac_address);
                    GtkWidget *mac_item = gtk_menu_item_new_with_label(mac_label);
                    gtk_widget_set_sensitive(mac_item, FALSE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mac_item);
                    g_free(mac_label);
                }
                
                /* Speed */
                if (iface->speed > 0) {
                    gchar *speed_label = g_strdup_printf("   ⚡ Speed: %d Mbps", iface->speed);
                    GtkWidget *speed_item = gtk_menu_item_new_with_label(speed_label);
                    gtk_widget_set_sensitive(speed_item, FALSE);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), speed_item);
                    g_free(speed_label);
                }
            }
            
            /* Separator between interfaces */
            if (l->next != NULL) {
                GtkWidget *sep = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
            }
        }
        
        ethernet_interface_list_free(interfaces);
    }
    
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent*)event);
}

/* Handle right-click on Ethernet button */
static gboolean on_ethernet_button_press(GtkWidget *button, GdkEventButton *event, gpointer data) {
    (void)data;
    
    if (event->button == 3) {  /* Right click */
        show_ethernet_popup_menu(button, event);
        return TRUE;
    }
    return FALSE;
}

static void on_ethernet_clicked(GtkButton *button, gpointer data)
{
    (void)data;
    /* Ethernet is typically always on - just refresh status */
    gboolean connected = ethernet_client_is_connected();
    
    /* Update UI */
    GtkWidget *label = g_object_get_data(G_OBJECT(button), "subtitle_label");
    if (label) {
        gtk_label_set_text(GTK_LABEL(label), connected ? "Connected" : "Off");
    }
    
    GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(button));
    if (connected) {
        gtk_style_context_add_class(ctx, "active");
    } else {
        gtk_style_context_remove_class(ctx, "active");
    }
}

/* =====================================================================
 * DO NOT DISTURB & NOTIFICATIONS
 * ===================================================================== */

static void on_dnd_clicked(GtkButton *button, gpointer data)
{
    (void)data;
    gboolean current = notification_client_get_dnd();
    gboolean new_state = !current;
    
    notification_client_set_dnd(new_state);
    
    /* Update UI immediately (signal will also trigger, but this feels faster) */
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

static void on_dnd_changed(gboolean enabled, gpointer data) {
    (void)data;
    if (_dnd_button) {
        GtkWidget *label = g_object_get_data(G_OBJECT(_dnd_button), "subtitle_label");
        if (label) {
            gtk_label_set_text(GTK_LABEL(label), enabled ? "On" : "Off");
        }
        
        GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(_dnd_button));
        if (enabled) {
            gtk_style_context_add_class(ctx, "active");
        } else {
            gtk_style_context_remove_class(ctx, "active");
        }
    }
}

static void on_clear_notifications_clicked(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    notification_client_clear_history();
}
