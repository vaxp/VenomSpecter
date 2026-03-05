#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib.h>
#include <string.h>
#include "../../include/venom-widget-api.h"

#define NOTES_FILE_PATH "/home/x/.config/venom/quick_notes.txt"

static GtkWidget *textview = NULL;

static gboolean is_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int widget_start_x = 0;
static int widget_start_y = 0;
static VenomDesktopAPI *api_handle = NULL;

static void save_notes(GtkTextBuffer *buffer, gpointer data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    if (text) {
        g_file_set_contents(NOTES_FILE_PATH, text, -1, NULL);
        g_free(text);
    }
}

static void load_notes() {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    char *contents = NULL;
    gsize length = 0;
    
    if (g_file_get_contents(NOTES_FILE_PATH, &contents, &length, NULL)) {
        gtk_text_buffer_set_text(buffer, contents, length);
        g_free(contents);
    } else {
        gtk_text_buffer_set_text(buffer, "Type quick notes here...", -1);
    }
    
    /* Connect after loading to avoid triggering save immediately */
    g_signal_connect(buffer, "changed", G_CALLBACK(save_notes), NULL);
}

/* Mouse Interactions strictly attached to the header */
static gboolean on_header_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) { 
        is_dragging = TRUE;
        drag_start_x = event->x_root;
        drag_start_y = event->y_root;
        
        GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
        /* The widget data points to the main event box that contains the whole widget */
        GtkWidget *root_ev_box = GTK_WIDGET(data);
        
        gint wx, wy;
        gtk_widget_translate_coordinates(root_ev_box, toplevel, 0, 0, &wx, &wy);
        widget_start_x = wx;
        widget_start_y = wy;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_header_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_dragging && api_handle && api_handle->layout_container) {
        int dx = event->x_root - drag_start_x;
        int dy = event->y_root - drag_start_y;
        GtkWidget *root_ev_box = GTK_WIDGET(data);
        gtk_layout_move(GTK_LAYOUT(api_handle->layout_container), root_ev_box, widget_start_x + dx, widget_start_y + dy);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_header_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_dragging) {
        is_dragging = FALSE;
        if (api_handle && api_handle->save_position && api_handle->layout_container) {
            GtkWidget *root_ev_box = GTK_WIDGET(data);
            gint x, y;
            gtk_widget_translate_coordinates(root_ev_box, gtk_widget_get_toplevel(widget), 0, 0, &x, &y);
            api_handle->save_position("notes.so", x, y);
        }
        return TRUE;
    }
    return FALSE;
}

static GtkWidget* create_notes_ui(VenomDesktopAPI *desktop_api) {
    api_handle = desktop_api;
    
    /* Root container that gets moved around. */
    GtkWidget *root_ev_box = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(root_ev_box), FALSE);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(root_ev_box), frame);
    gtk_widget_set_size_request(frame, 250, 200);
    
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "frame { background-color: rgba(253, 227, 167, 0.3); border: 1px solid rgba(0,0,0,0.2); border-radius: 4px; box-shadow: 2px 2px 8px rgba(0,0,0,0.3); }"
        "#header { background-color: rgba(243, 157, 18, 0.2); padding: 4px; border-top-left-radius: 3px; border-top-right-radius: 3px; cursor: move; }"
        "label.title { color: #0000001f; font-weight: bold; font-size: 11px; }"
        "textview text { background-color: transparent; color: #2c3e50; font-family: sans; font-size: 13px; }", -1, NULL);
        
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), 800);
    g_object_unref(css);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* Draggable Header EventBox */
    GtkWidget *header_ev = gtk_event_box_new();
    gtk_widget_set_name(header_ev, "header");
    gtk_widget_set_events(header_ev, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    /* Pass root_ev_box as user data so the handler moves the whole widget, not just the header */
    g_signal_connect(header_ev, "button-press-event", G_CALLBACK(on_header_button_press), root_ev_box);
    g_signal_connect(header_ev, "motion-notify-event", G_CALLBACK(on_header_motion), root_ev_box);
    g_signal_connect(header_ev, "button-release-event", G_CALLBACK(on_header_button_release), root_ev_box);

    GtkWidget *header_lbl = gtk_label_new("Quick Notes");
    gtk_widget_set_halign(header_lbl, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(header_ev), header_lbl);
    GtkStyleContext *h_ctx = gtk_widget_get_style_context(header_lbl);
    gtk_style_context_add_class(h_ctx, "title");
    
    gtk_box_pack_start(GTK_BOX(vbox), header_ev, FALSE, FALSE, 0);

    /* Text Area */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    textview = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(textview), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(textview), 8);
    gtk_container_add(GTK_CONTAINER(scroll), textview);

    load_notes();
    
    gtk_widget_show_all(root_ev_box);
    return root_ev_box;
}

VenomWidgetAPI* venom_widget_init(void) {
    static VenomWidgetAPI api;
    api.name = "Quick Notes";
    api.description = "A sticky note for quick text.";
    api.author = "Venom Core";
    api.create_widget = create_notes_ui;
    return &api;
}
