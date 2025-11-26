#ifndef SEARCH_H
#define SEARCH_H

#include <gtk/gtk.h>

/* Execution Functions */
void execute_vater(const gchar *cmd, GtkWidget *window);
void execute_math(const gchar *expr, GtkWidget *window);
void execute_file_search(const gchar *term, GtkWidget *window);
void execute_web_search(const gchar *term, const gchar *engine, GtkWidget *window, GtkWidget **stack_ptr, GtkWidget **entry_ptr, GtkWidget **results_ptr);

/* Main Search Function */
void perform_search(const gchar *text, GtkWidget *stack, GtkWidget *results_view, GtkWidget *window);

#endif
