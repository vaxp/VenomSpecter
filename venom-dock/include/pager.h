#ifndef PAGER_H
#define PAGER_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>

/* Initialize Pager module (X11 atoms) */
void pager_init(Display *display, Window root);

/* Create the Pager UI widget */
GtkWidget *pager_create_widget(void);

/* Callback for when a workspace is clicked */
typedef void (*PagerClickCallback)(int desktop_idx, gpointer user_data);
void pager_set_click_callback(PagerClickCallback callback, gpointer user_data);

/* Update pager state (called on X11 events) */
void pager_update(void);

#endif
