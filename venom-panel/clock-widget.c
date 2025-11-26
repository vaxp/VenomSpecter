#include "clock-widget.h"
#include <time.h>

static gboolean update_clock(gpointer data) {
    GtkLabel *label = GTK_LABEL(data);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%a %b %d  %I:%M %p", tm_info);
    
    gtk_label_set_text(label, buffer);
    
    return TRUE; /* Continue calling */
}

GtkWidget* create_clock_widget(void) {
    GtkWidget *label = gtk_label_new("");
    
    /* Style */
    GtkStyleContext *context = gtk_widget_get_style_context(label);
    gtk_style_context_add_class(context, "clock-label");
    
    /* Initial update */
    update_clock(label);
    
    /* Update every second */
    g_timeout_add_seconds(1, update_clock, label);
    
    return label;
}
