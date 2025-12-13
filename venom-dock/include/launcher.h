#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <gtk/gtk.h>

/* Global variables exposed for panel management */
extern GtkWidget *launcher_button;
extern GtkWidget *launcher_window;

/* Function prototypes */
void create_launcher_button(GtkWidget *box);
void on_launcher_app_clicked(GtkWidget *widget, gpointer data);

#endif /* LAUNCHER_H */
