#ifndef VENOM_SNI_H
#define VENOM_SNI_H

#include <glib.h>
#include <gio/gio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 📦 Types
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    gchar *bus_name;        // e.g., :1.234 or org.kde.StatusNotifierItem-1234-1
    gchar *object_path;     // e.g., /StatusNotifierItem
    gchar *id;              // Application ID
    gchar *title;           // Display title
    gchar *icon_name;       // Icon name (freedesktop)
    gchar *icon_theme_path; // Custom icon theme path
    gchar *status;          // Passive, Active, NeedsAttention
    gchar *category;        // ApplicationStatus, Communications, etc.
    gchar *menu_path;       // DBusMenu object path
    GBytes *icon_pixmap;    // Icon data if no icon name
    gint icon_width;
    gint icon_height;
} SNIItem;

typedef struct {
    GDBusConnection *conn;
    GHashTable *items;          // bus_name -> SNIItem
    guint watcher_id;
    guint host_id;
    gboolean is_watcher;
    
    // Callbacks
    void (*on_item_added)(const gchar *id);
    void (*on_item_removed)(const gchar *id);
    void (*on_item_changed)(const gchar *id);
} SNIState;

extern SNIState sni_state;

// ═══════════════════════════════════════════════════════════════════════════
// 🔧 Functions
// ═══════════════════════════════════════════════════════════════════════════

// Initialization
gboolean sni_init(void);
void sni_cleanup(void);

// Items
GList* sni_get_items(void);
SNIItem* sni_get_item(const gchar *id);

// Actions
gboolean sni_activate(const gchar *id, gint x, gint y);
gboolean sni_secondary_activate(const gchar *id, gint x, gint y);
gboolean sni_scroll(const gchar *id, gint delta, const gchar *orientation);

// Menu (DBusMenu)
GVariant* sni_get_menu(const gchar *id);
gboolean sni_menu_event(const gchar *id, gint menu_id, const gchar *event);

// Icon Pixmap
GBytes* sni_get_icon_pixmap(const gchar *id, gint *width, gint *height);

// Cleanup
void sni_item_free(SNIItem *item);

#endif // VENOM_SNI_H
