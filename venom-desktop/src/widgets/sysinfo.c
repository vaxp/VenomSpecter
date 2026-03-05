#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "../../include/venom-widget-api.h"

static GtkWidget *lbl_uptime = NULL;

static gboolean is_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int widget_start_x = 0;
static int widget_start_y = 0;
static VenomDesktopAPI *api_handle = NULL;

static void get_os_name(char *buffer, size_t max_len) {
    snprintf(buffer, max_len, "💿 Unknown OS");
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) return;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *start = line + 12;
            char *end = line + strlen(line) - 1;
            if (*end == '\n') *end = '\0';
            if (*start == '"') {
                start++;
                end--;
                if (*end == '"') *end = '\0';
            }
            snprintf(buffer, max_len, "💿 %s", start);
            break;
        }
    }
    fclose(fp);
}

static void update_uptime_text() {
    if (!lbl_uptime) return;
    
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) return;
    
    double uptime_secs;
    if (fscanf(fp, "%lf", &uptime_secs) == 1) {
        int days = uptime_secs / (60 * 60 * 24);
        int hours = ((int)uptime_secs % (60 * 60 * 24)) / (60 * 60);
        int minutes = ((int)uptime_secs % (60 * 60)) / 60;
        
        char up_str[64];
        if (days > 0) {
            snprintf(up_str, sizeof(up_str), "Up: %dd, %dh, %dm", days, hours, minutes);
        } else {
            snprintf(up_str, sizeof(up_str), "Up: %dh, %dm", hours, minutes);
        }
        gtk_label_set_text(GTK_LABEL(lbl_uptime), up_str);
    }
    fclose(fp);
}

static gboolean update_sysinfo(gpointer data) {
    update_uptime_text();
    return TRUE; // Update every minute
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
            api_handle->save_position("sysinfo.so", x, y);
        }
        return TRUE;
    }
    return FALSE;
}

static GtkWidget* create_sysinfo_ui(VenomDesktopAPI *desktop_api) {
    api_handle = desktop_api;
    GtkWidget *event_box = gtk_event_box_new();
    gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
    
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_widget_button_press), NULL);
    g_signal_connect(event_box, "motion-notify-event", G_CALLBACK(on_widget_motion), NULL);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_widget_button_release), NULL);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(event_box), frame);
    gtk_widget_set_size_request(frame, 220, 115);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "frame { background-color: rgba(0,0,0,0.23); border: 1px solid rgba(0,0,0,0.1); border-radius: 8px; padding: 10px; }"
        "label.host { color: #f39c12; font-weight: bold; font-size: 14px; }"
        "label.info { color: #ecf0f1; font-family: sans; font-size: 13px; }", -1, NULL);
        
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), 800);
    g_object_unref(css);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    struct utsname buffer;
    char host_str[128] = "Unknown Host";
    char ker_str[128] = "Unknown Kernel";
    char os_str[128];
    
    get_os_name(os_str, sizeof(os_str));
    
    if (uname(&buffer) == 0) {
        snprintf(host_str, sizeof(host_str), "🖥 %s", buffer.nodename);
        snprintf(ker_str, sizeof(ker_str), "🐧 %s", buffer.release);
    }

    GtkWidget *lbl_os = gtk_label_new(os_str);
    gtk_widget_set_halign(lbl_os, GTK_ALIGN_START);
    GtkStyleContext *sos_ctx = gtk_widget_get_style_context(lbl_os);
    gtk_style_context_add_class(sos_ctx, "info");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_os, FALSE, FALSE, 0);

    GtkWidget *lbl_host = gtk_label_new(host_str);
    gtk_widget_set_halign(lbl_host, GTK_ALIGN_START);
    GtkStyleContext *sh_ctx = gtk_widget_get_style_context(lbl_host);
    gtk_style_context_add_class(sh_ctx, "host");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_host, FALSE, FALSE, 0);

    GtkWidget *lbl_ker = gtk_label_new(ker_str);
    gtk_widget_set_halign(lbl_ker, GTK_ALIGN_START);
    GtkStyleContext *sk_ctx = gtk_widget_get_style_context(lbl_ker);
    gtk_style_context_add_class(sk_ctx, "info");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_ker, FALSE, FALSE, 0);

    lbl_uptime = gtk_label_new("Up: 0m");
    gtk_widget_set_halign(lbl_uptime, GTK_ALIGN_START);
    GtkStyleContext *su_ctx = gtk_widget_get_style_context(lbl_uptime);
    gtk_style_context_add_class(su_ctx, "info");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_uptime, FALSE, FALSE, 0);

    update_uptime_text(); // Initial call
    g_timeout_add(60000, update_sysinfo, NULL); // Update every 60 seconds
    
    gtk_widget_show_all(event_box);
    return event_box;
}

VenomWidgetAPI* venom_widget_init(void) {
    static VenomWidgetAPI api;
    api.name = "System Info";
    api.description = "Host, Kernel and Uptime.";
    api.author = "Venom Core";
    api.create_widget = create_sysinfo_ui;
    return &api;
}
