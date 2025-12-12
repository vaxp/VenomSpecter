/*
 * sni-client.h
 *
 * Client for venom_sni daemon (System Tray).
 * Interface: org.venom.SNI
 */

#ifndef SNI_CLIENT_H
#define SNI_CLIENT_H

#include <gtk/gtk.h>
#include <gio/gio.h>

/* Helper struct for Tray Item */
typedef struct {
    gchar *id;
    gchar *title;
    gchar *icon_name;
    gchar *status;
    GdkPixbuf *icon_pixbuf; /* Can be NULL if using icon_name */
} TrayItem;

/* Callbacks */
typedef void (*TrayItemAddedCallback)(TrayItem *item, gpointer user_data);
typedef void (*TrayItemRemovedCallback)(const gchar *id, gpointer user_data);

/* Initialize/Cleanup */
void sni_client_init(void);
void sni_client_cleanup(void);

/* Get all current items */
GList* sni_client_get_items(void);

/* Actions */
void sni_client_activate(const gchar *id, gint x, gint y);
void sni_client_secondary_activate(const gchar *id, gint x, gint y);

/* Signals */
void sni_client_on_item_added(TrayItemAddedCallback callback, gpointer user_data);
void sni_client_on_item_removed(TrayItemRemovedCallback callback, gpointer user_data);

/* Helper to free list */
void tray_item_free(TrayItem *item);
void tray_item_list_free(GList *list);

/* Menu Item Struct */
typedef struct TrayMenuItem {
    gint id;
    gchar *label;
    gboolean enabled;
    gboolean visible;
    gchar *type; /* "separator", "standard" */
    gchar *toggle_type; /* "checkmark", "radio", "" */
    gint toggle_state; /* 0=off, 1=on */
    GList *children; /* List of TrayMenuItem* */
} TrayMenuItem;

/* Menu Actions */
GList* sni_client_get_menu(const gchar *id);
void sni_client_menu_click(const gchar *id, gint menu_id);

void tray_menu_item_free(TrayMenuItem *item);
void tray_menu_list_free(GList *list);

#endif /* SNI_CLIENT_H */

