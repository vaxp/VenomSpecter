#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/venom-widget-api.h"

static GtkWidget *lbl_down = NULL;
static GtkWidget *lbl_up = NULL;

static unsigned long long last_rx = 0;
static unsigned long long last_tx = 0;

static gboolean is_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int widget_start_x = 0;
static int widget_start_y = 0;
static VenomDesktopAPI *api_handle = NULL;

static void get_network_bytes(unsigned long long *rx, unsigned long long *tx) {
    FILE *fp = fopen("/proc/net/dev", "r");
    char line[256];
    *rx = 0; *tx = 0;
    
    if (!fp) return;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strchr(line, ':')) {
            char *colon = strchr(line, ':');
            unsigned long long dev_rx = 0, dev_tx = 0;
            // The first 4 fields are RX data, the next 8 are TX data.
            // Format: Inter- | Receive | Transmit
            // face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed
            if (sscanf(colon + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &dev_rx, &dev_tx) == 2) {
                // Ignore loopback
                if (!strstr(line, "lo:")) {
                    *rx += dev_rx;
                    *tx += dev_tx;
                }
            }
        }
    }
    fclose(fp);
}

static char* format_speed(unsigned long long bytes_per_sec) {
    if (bytes_per_sec > 1024 * 1024) {
        return g_strdup_printf("%.1f MB/s", (double)bytes_per_sec / (1024.0 * 1024.0));
    } else if (bytes_per_sec > 1024) {
        return g_strdup_printf("%.1f KB/s", (double)bytes_per_sec / 1024.0);
    } else {
        return g_strdup_printf("%llu B/s", bytes_per_sec);
    }
}

static gboolean update_netmon(gpointer data) {
    if (!lbl_down || !lbl_up) return FALSE;
    
    unsigned long long current_rx, current_tx;
    get_network_bytes(&current_rx, &current_tx);
    
    if (last_rx > 0 && last_tx > 0) {
        unsigned long long diff_rx = current_rx > last_rx ? current_rx - last_rx : 0;
        unsigned long long diff_tx = current_tx > last_tx ? current_tx - last_tx : 0;
        
        char *str_down = format_speed(diff_rx);
        char *str_up = format_speed(diff_tx);
        
        char lbl_down_text[64];
        char lbl_up_text[64];
        snprintf(lbl_down_text, sizeof(lbl_down_text), "⬇ %s", str_down);
        snprintf(lbl_up_text, sizeof(lbl_up_text), "⬆ %s", str_up);
        
        gtk_label_set_text(GTK_LABEL(lbl_down), lbl_down_text);
        gtk_label_set_text(GTK_LABEL(lbl_up), lbl_up_text);
        
        g_free(str_down);
        g_free(str_up);
    }
    
    last_rx = current_rx;
    last_tx = current_tx;
    
    return TRUE; 
}

/* Mouse Interactions */
static gboolean on_widget_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) { 
        is_dragging = TRUE;
        drag_start_x = event->x_root;
        drag_start_y = event->y_root;
        gint wx, wy;
        gtk_widget_translate_coordinates(widget, gtk_widget_get_toplevel(widget), 0, 0, &wx, &wy);
        widget_start_x = wx;
        widget_start_y = wy;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_widget_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_dragging && api_handle && api_handle->layout_container) {
        int dx = event->x_root - drag_start_x;
        int dy = event->y_root - drag_start_y;
        gtk_layout_move(GTK_LAYOUT(api_handle->layout_container), widget, widget_start_x + dx, widget_start_y + dy);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_widget_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_dragging) {
        is_dragging = FALSE;
        if (api_handle && api_handle->save_position && api_handle->layout_container) {
            gint x, y;
            gtk_widget_translate_coordinates(widget, gtk_widget_get_toplevel(widget), 0, 0, &x, &y);
            api_handle->save_position("netmon.so", x, y);
        }
        return TRUE;
    }
    return FALSE;
}

static GtkWidget* create_netmon_ui(VenomDesktopAPI *desktop_api) {
    api_handle = desktop_api;
    GtkWidget *event_box = gtk_event_box_new();
    gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
    
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_widget_button_press), NULL);
    g_signal_connect(event_box, "motion-notify-event", G_CALLBACK(on_widget_motion), NULL);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_widget_button_release), NULL);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(event_box), frame);
    gtk_widget_set_size_request(frame, 160, 80);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "frame { background-color: rgba(0,0,0,0.23); border: 1px solid rgba(0,0,0,0.1); border-radius: 8px; padding: 10px; }"
        "label.title { color: #ecf0f1; font-weight: bold; font-size: 14px; }"
        "label.down { color: #2ecc71; font-weight: bold; font-family: monospace; font-size: 15px; }"
        "label.up { color: #e74c3c; font-weight: bold; font-family: monospace; font-size: 15px; }", -1, NULL);
        
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), 800);
    g_object_unref(css);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    GtkWidget *title = gtk_label_new("Network Mon");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    GtkStyleContext *t_ctx = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(t_ctx, "title");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *hbox_down = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    lbl_down = gtk_label_new("⬇ 0 B/s");
    gtk_widget_set_halign(lbl_down, GTK_ALIGN_START);
    GtkStyleContext *d_ctx = gtk_widget_get_style_context(lbl_down);
    gtk_style_context_add_class(d_ctx, "down");
    gtk_box_pack_start(GTK_BOX(hbox_down), lbl_down, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_down, FALSE, FALSE, 0);
    
    GtkWidget *hbox_up = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    lbl_up = gtk_label_new("⬆ 0 B/s");
    gtk_widget_set_halign(lbl_up, GTK_ALIGN_START);
    GtkStyleContext *u_ctx = gtk_widget_get_style_context(lbl_up);
    gtk_style_context_add_class(u_ctx, "up");
    gtk_box_pack_start(GTK_BOX(hbox_up), lbl_up, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_up, FALSE, FALSE, 0);

    update_netmon(NULL);
    g_timeout_add(1000, update_netmon, NULL);
    
    gtk_widget_show_all(event_box);
    return event_box;
}

VenomWidgetAPI* venom_widget_init(void) {
    static VenomWidgetAPI api;
    api.name = "Network Monitor";
    api.description = "Live DL/UL speed.";
    api.author = "Venom Core";
    api.create_widget = create_netmon_ui;
    return &api;
}
