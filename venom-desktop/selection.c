/*
 * selection.c
 * Selection management and rubber band
 */

#include "selection.h"
#include "wallpaper.h"
#include "menu.h"
#include <math.h>

/* Selection State */
double start_x = 0;
double start_y = 0;
double current_x = 0;
double current_y = 0;
gboolean is_selecting = FALSE;
GList *selected_items = NULL;

/* --- Selection Logic --- */

gboolean is_selected(GtkWidget *item) {
    return g_list_find(selected_items, item) != NULL;
}

void select_item(GtkWidget *item) {
    if (!is_selected(item)) {
        selected_items = g_list_append(selected_items, item);
        GtkStyleContext *context = gtk_widget_get_style_context(item);
        gtk_style_context_add_class(context, "selected");
        gtk_widget_queue_draw(item);
    }
}

void deselect_item(GtkWidget *item) {
    GList *l = g_list_find(selected_items, item);
    if (l) {
        selected_items = g_list_delete_link(selected_items, l);
        GtkStyleContext *context = gtk_widget_get_style_context(item);
        gtk_style_context_remove_class(context, "selected");
        gtk_widget_queue_draw(item);
    }
}

void deselect_all(void) {
    for (GList *l = selected_items; l != NULL; l = l->next) {
        GtkWidget *item = GTK_WIDGET(l->data);
        if (GTK_IS_WIDGET(item)) {
            GtkStyleContext *context = gtk_widget_get_style_context(item);
            gtk_style_context_remove_class(context, "selected");
            gtk_widget_queue_draw(item);
        }
    }
    g_list_free(selected_items);
    selected_items = NULL;
}

/* --- Drawing --- */

gboolean on_layout_draw_fg(GtkWidget *widget, cairo_t *cr, gpointer data) {
    if (is_selecting) {
        double x = MIN(start_x, current_x);
        double y = MIN(start_y, current_y);
        double w = fabs(current_x - start_x);
        double h = fabs(current_y - start_y);

        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3);
        cairo_rectangle(cr, x, y, w, h);
        cairo_fill(cr);

        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.8);
        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, x, y, w, h);
        cairo_stroke(cr);
    }
    return FALSE;
}

/* --- Event Handlers --- */

gboolean on_bg_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) {
        deselect_all();
        is_selecting = TRUE;
        start_x = event->x;
        start_y = event->y;
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(icon_layout);
        return TRUE;
    }
    if (event->button == 3) {
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *new_folder = gtk_menu_item_new_with_label("Create Folder");
        GtkWidget *term = gtk_menu_item_new_with_label("Open Terminal Here");
        GtkWidget *paste = gtk_menu_item_new_with_label("Paste");
        GtkWidget *sep1 = gtk_separator_menu_item_new();
        GtkWidget *refresh = gtk_menu_item_new_with_label("Refresh");
        GtkWidget *sep2 = gtk_separator_menu_item_new();
        GtkWidget *quit = gtk_menu_item_new_with_label("Quit Desktop");

        g_signal_connect(new_folder, "activate", G_CALLBACK(on_create_folder), widget);
        g_signal_connect(term, "activate", G_CALLBACK(on_open_terminal), NULL);
        g_signal_connect(paste, "activate", G_CALLBACK(on_bg_paste), NULL);
        g_signal_connect(refresh, "activate", G_CALLBACK(on_refresh_clicked), NULL);
        g_signal_connect(quit, "activate", G_CALLBACK(gtk_main_quit), NULL);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_folder);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), term);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep1);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), refresh);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep2);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE;
    }
    return FALSE;
}

gboolean on_bg_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_selecting) {
        current_x = event->x;
        current_y = event->y;
        gtk_widget_queue_draw(icon_layout);
        
        double x = MIN(start_x, current_x);
        double y = MIN(start_y, current_y);
        double w = fabs(current_x - start_x);
        double h = fabs(current_y - start_y);
        
        GList *children = gtk_container_get_children(GTK_CONTAINER(icon_layout));
        for (GList *l = children; l != NULL; l = l->next) {
            GtkWidget *item = GTK_WIDGET(l->data);
            GtkAllocation alloc;
            gtk_widget_get_allocation(item, &alloc);
            
            if (alloc.x < x + w && alloc.x + alloc.width > x &&
                alloc.y < y + h && alloc.y + alloc.height > y) {
                select_item(item);
            } else {
                deselect_item(item);
            }
        }
        g_list_free(children);
    }
    return FALSE;
}

gboolean on_bg_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_selecting) {
        is_selecting = FALSE;
        gtk_widget_queue_draw(icon_layout);
    }
    return FALSE;
}
