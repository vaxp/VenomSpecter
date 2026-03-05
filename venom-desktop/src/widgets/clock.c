#include <gtk/gtk.h>
#include <time.h>
#include "../../include/venom-widget-api.h"

/* Widget State */
static GtkWidget *lbl_time = NULL;
static GtkWidget *lbl_date = NULL;

static gboolean is_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int widget_start_x = 0;
static int widget_start_y = 0;
static VenomDesktopAPI *api_handle = NULL;

static gboolean update_clock(gpointer data) {
    if (!lbl_time || !lbl_date) return FALSE;
    
    time_t rawtime;
    struct tm *info;
    char time_str[64];
    char date_str[64];

    time(&rawtime);
    info = localtime(&rawtime);

    strftime(time_str, sizeof(time_str), "%H:%M:%S", info);
    strftime(date_str, sizeof(date_str), "%A, %B %d", info);

    gtk_label_set_text(GTK_LABEL(lbl_time), time_str);
    gtk_label_set_text(GTK_LABEL(lbl_date), date_str);
    
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
        
        int new_x = widget_start_x + dx;
        int new_y = widget_start_y + dy;
        
        gtk_layout_move(GTK_LAYOUT(api_handle->layout_container), widget, new_x, new_y);
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
            api_handle->save_position("clock.so", x, y);
        }
        return TRUE;
    }
    return FALSE;
}

static GtkWidget* create_clock_ui(VenomDesktopAPI *desktop_api) {
    api_handle = desktop_api;
    GtkWidget *event_box = gtk_event_box_new();
    gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
    
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_widget_button_press), NULL);
    g_signal_connect(event_box, "motion-notify-event", G_CALLBACK(on_widget_motion), NULL);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_widget_button_release), NULL);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(event_box), frame);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "frame { "
        "   background-color: transparent; "
        "   border: none; "
        "   padding: 10px; "
        "}"
        "#lbl_time { color: #ecf0f1; font-weight: 900; font-size: 48px; text-shadow: 2px 2px 4px rgba(0,0,0,0.6); }"
        "#lbl_date { color: #bdc3c7; font-weight: bold; font-size: 18px; text-shadow: 1px 1px 3px rgba(0,0,0,0.5); }", -1, NULL);
        
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), 800);
    g_object_unref(css);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    lbl_time = gtk_label_new("00:00:00");
    gtk_widget_set_name(lbl_time, "lbl_time");
    gtk_widget_set_halign(lbl_time, GTK_ALIGN_CENTER);
    
    lbl_date = gtk_label_new("Monday, January 1");
    gtk_widget_set_name(lbl_date, "lbl_date");
    gtk_widget_set_halign(lbl_date, GTK_ALIGN_CENTER);

    gtk_box_pack_start(GTK_BOX(vbox), lbl_time, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_date, FALSE, FALSE, 0);

    update_clock(NULL); /* Initial Paint */
    g_timeout_add(1000, update_clock, NULL);
    
    gtk_widget_show_all(event_box);
    return event_box;
}

VenomWidgetAPI* venom_widget_init(void) {
    static VenomWidgetAPI api;
    api.name = "Digital Clock";
    api.description = "A large digital clock and date readout.";
    api.author = "Venom Core";
    api.create_widget = create_clock_ui;
    return &api;
}
