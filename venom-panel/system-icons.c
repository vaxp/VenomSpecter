#include "system-icons.h"

GtkWidget* create_system_icons(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    /* WiFi icon */
    GtkWidget *wifi = gtk_image_new_from_icon_name("network-wireless-signal-excellent-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), wifi, FALSE, FALSE, 0);
    
    /* Bluetooth icon */
    GtkWidget *bluetooth = gtk_image_new_from_icon_name("bluetooth-active-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), bluetooth, FALSE, FALSE, 0);
    
    /* Battery icon */
    GtkWidget *battery = gtk_image_new_from_icon_name("battery-full-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), battery, FALSE, FALSE, 0);
    
    /* Add CSS class */
    GtkStyleContext *context = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(context, "system-icons");
    
    return box;
}
