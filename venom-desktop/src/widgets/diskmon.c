#include <gtk/gtk.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include "../../include/venom-widget-api.h"

static GtkWidget *lbl_disk = NULL;
static GtkWidget *prog_disk = NULL;

static gboolean is_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int widget_start_x = 0;
static int widget_start_y = 0;
static VenomDesktopAPI *api_handle = NULL;

static gboolean update_diskmon(gpointer data) {
    if (!lbl_disk || !prog_disk) return FALSE;
    
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) return TRUE;
    
    unsigned long long total = stat.f_blocks * stat.f_frsize;
    unsigned long long free = stat.f_bfree * stat.f_frsize;
    unsigned long long used = total - free;
    
    double percent = 0.0;
    if (total > 0) {
        percent = (double)used / (double)total * 100.0;
    }
    
    char disk_str[64];
    snprintf(disk_str, sizeof(disk_str), "Root (/): %.1f%%", percent);
    gtk_label_set_text(GTK_LABEL(lbl_disk), disk_str);
    
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog_disk), percent / 100.0);
    
    return TRUE; // Continue updating every 5 seconds
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
            api_handle->save_position("diskmon.so", x, y);
        }
        return TRUE;
    }
    return FALSE;
}

static GtkWidget* create_diskmon_ui(VenomDesktopAPI *desktop_api) {
    api_handle = desktop_api;
    GtkWidget *event_box = gtk_event_box_new();
    gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
    
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_widget_button_press), NULL);
    g_signal_connect(event_box, "motion-notify-event", G_CALLBACK(on_widget_motion), NULL);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_widget_button_release), NULL);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(event_box), frame);
    gtk_widget_set_size_request(frame, 200, 70);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "frame { background-color: rgba(0,0,0,0.23); border: 1px solid rgba(0,0,0,0.1); border-radius: 8px; padding: 10px; }"
        "label.title { color: #ecf0f1; font-weight: bold; font-family: monospace; font-size: 13px; }"
        "progressbar trough { background-color: rgba(0,0,0,0.6); border-radius: 4px; min-height: 8px; }"
        "progressbar progress { background-color: #9b59b6; border-radius: 4px; }", -1, NULL);
        
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), 800);
    g_object_unref(css);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    lbl_disk = gtk_label_new("Root (/): 0.0%");
    gtk_widget_set_halign(lbl_disk, GTK_ALIGN_START);
    GtkStyleContext *d_ctx = gtk_widget_get_style_context(lbl_disk);
    gtk_style_context_add_class(d_ctx, "title");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_disk, FALSE, FALSE, 0);

    prog_disk = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), prog_disk, FALSE, FALSE, 0);

    update_diskmon(NULL);
    g_timeout_add(5000, update_diskmon, NULL); // Update every 5 seconds since disk space changes slowly
    
    gtk_widget_show_all(event_box);
    return event_box;
}

VenomWidgetAPI* venom_widget_init(void) {
    static VenomWidgetAPI api;
    api.name = "Disk Monitor";
    api.description = "Root filesystem usage.";
    api.author = "Venom Core";
    api.create_widget = create_diskmon_ui;
    return &api;
}
