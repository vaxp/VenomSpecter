#include "sni.h"
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 Global State
// ═══════════════════════════════════════════════════════════════════════════

SNIState sni_state = {0};

// StatusNotifier interfaces
#define SNI_WATCHER_IFACE "org.kde.StatusNotifierWatcher"
#define SNI_ITEM_IFACE "org.kde.StatusNotifierItem"
#define SNI_WATCHER_PATH "/StatusNotifierWatcher"

// ═══════════════════════════════════════════════════════════════════════════
// 🔧 Helper Functions
// ═══════════════════════════════════════════════════════════════════════════

void sni_item_free(SNIItem *item) {
    if (!item) return;
    g_free(item->bus_name);
    g_free(item->object_path);
    g_free(item->id);
    g_free(item->title);
    g_free(item->icon_name);
    g_free(item->icon_theme_path);
    g_free(item->status);
    g_free(item->category);
    g_free(item->menu_path);
    if (item->icon_pixmap) g_bytes_unref(item->icon_pixmap);
    g_free(item);
}

static gchar* extract_bus_name(const gchar *service) {
    // Service can be "bus_name/object_path" or just ":1.234"
    if (!service) return NULL;
    
    const gchar *slash = strchr(service, '/');
    if (slash) {
        return g_strndup(service, slash - service);
    }
    return g_strdup(service);
}

static gchar* extract_object_path(const gchar *service) {
    if (!service) return g_strdup("/StatusNotifierItem");
    
    const gchar *slash = strchr(service, '/');
    if (slash) {
        return g_strdup(slash);
    }
    return g_strdup("/StatusNotifierItem");
}

// ═══════════════════════════════════════════════════════════════════════════
// 📡 Get Item Properties
// ═══════════════════════════════════════════════════════════════════════════

static gchar* get_string_property(const gchar *bus_name, const gchar *path, const gchar *prop) {
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        sni_state.conn, bus_name, path,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", SNI_ITEM_IFACE, prop),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_error_free(error);
        return NULL;
    }
    
    GVariant *value;
    g_variant_get(result, "(v)", &value);
    
    gchar *str = NULL;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
        str = g_strdup(g_variant_get_string(value, NULL));
    } else if (g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH)) {
        str = g_strdup(g_variant_get_string(value, NULL));
    }
    
    g_variant_unref(value);
    g_variant_unref(result);
    return str;
}

static gchar* get_tooltip_title(const gchar *bus_name, const gchar *path) {
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        sni_state.conn, bus_name, path,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", SNI_ITEM_IFACE, "ToolTip"),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_error_free(error);
        return NULL;
    }
    
    GVariant *value;
    g_variant_get(result, "(v)", &value);
    
    gchar *title = NULL;
    // ToolTip is (sa(iiay)ss) - icon_name, icon_data, title, description
    if (g_variant_is_of_type(value, G_VARIANT_TYPE("(sa(iiay)ss)"))) {
        const gchar *tooltip_title;
        g_variant_get_child(value, 2, "&s", &tooltip_title);
        if (tooltip_title && strlen(tooltip_title) > 0) {
            title = g_strdup(tooltip_title);
        }
    }
    
    g_variant_unref(value);
    g_variant_unref(result);
    return title;
}

static SNIItem* fetch_item_properties(const gchar *service) {
    gchar *bus_name = extract_bus_name(service);
    gchar *object_path = extract_object_path(service);
    
    if (!bus_name) {
        g_free(object_path);
        return NULL;
    }
    
    SNIItem *item = g_new0(SNIItem, 1);
    item->bus_name = bus_name;
    item->object_path = object_path;
    
    // Fetch properties
    item->id = get_string_property(bus_name, object_path, "Id");
    item->title = get_string_property(bus_name, object_path, "Title");
    item->icon_name = get_string_property(bus_name, object_path, "IconName");
    item->icon_theme_path = get_string_property(bus_name, object_path, "IconThemePath");
    item->status = get_string_property(bus_name, object_path, "Status");
    item->category = get_string_property(bus_name, object_path, "Category");
    item->menu_path = get_string_property(bus_name, object_path, "Menu");
    
    // Fallback for ID
    if (!item->id || strlen(item->id) == 0) {
        g_free(item->id);
        item->id = g_strdup(bus_name);
    }
    
    // Fallback for title using ToolTip
    if (!item->title || strlen(item->title) == 0) {
        g_free(item->title);
        item->title = get_tooltip_title(bus_name, object_path);
    }
    
    // Ultimate fallback for title
    if (!item->title || strlen(item->title) == 0) {
        g_free(item->title);
        item->title = g_strdup(item->id);
    }
    
    // Fallback menu path
    if (!item->menu_path || strlen(item->menu_path) == 0) {
        g_free(item->menu_path);
        item->menu_path = g_strdup("/com/canonical/dbusmenu");
    }
    
    printf("📦 SNI Item: %s (%s) icon=%s\n", 
           item->id, item->title, item->icon_name ? item->icon_name : "none");
    
    return item;
}

// ═══════════════════════════════════════════════════════════════════════════
// 👁️ Watcher Signal Handlers
// ═══════════════════════════════════════════════════════════════════════════

static void on_item_registered(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data) {
    (void)conn; (void)sender; (void)path; (void)iface; (void)signal; (void)data;
    
    const gchar *service;
    g_variant_get(params, "(&s)", &service);
    
    printf("✅ SNI Registered: %s\n", service);
    
    SNIItem *item = fetch_item_properties(service);
    if (item) {
        g_hash_table_insert(sni_state.items, g_strdup(item->id), item);
        if (sni_state.on_item_added) {
            sni_state.on_item_added(item->id);
        }
    }
}

static void on_item_unregistered(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data) {
    (void)conn; (void)sender; (void)path; (void)iface; (void)signal; (void)data;
    
    const gchar *service;
    g_variant_get(params, "(&s)", &service);
    
    printf("❌ SNI Unregistered: %s\n", service);
    
    gchar *bus_name = extract_bus_name(service);
    
    // Find and remove by bus_name
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, sni_state.items);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        SNIItem *item = value;
        if (g_strcmp0(item->bus_name, bus_name) == 0) {
            gchar *id = g_strdup(item->id);
            g_hash_table_iter_remove(&iter);
            if (sni_state.on_item_removed) {
                sni_state.on_item_removed(id);
            }
            g_free(id);
            break;
        }
    }
    
    g_free(bus_name);
}

static void on_item_property_changed(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data) {
    (void)conn; (void)iface; (void)params; (void)data;
    
    printf("🔄 SNI Property Changed: %s from %s (signal: %s)\n", path, sender, signal);
    
    // Find the item by its bus_name (sender)
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, sni_state.items);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        SNIItem *item = value;
        if (g_strcmp0(item->bus_name, sender) == 0) {
            // Re-fetch the changed property
            if (g_strcmp0(signal, "NewIcon") == 0 || g_strcmp0(signal, "NewIconThemePath") == 0) {
                g_free(item->icon_name);
                item->icon_name = get_string_property(sender, item->object_path, "IconName");
                g_free(item->icon_theme_path);
                item->icon_theme_path = get_string_property(sender, item->object_path, "IconThemePath");
            }
            else if (g_strcmp0(signal, "NewTitle") == 0 || g_strcmp0(signal, "NewToolTip") == 0) {
                g_free(item->title);
                item->title = get_string_property(sender, item->object_path, "Title");
                if (!item->title || strlen(item->title) == 0) {
                    g_free(item->title);
                    item->title = get_tooltip_title(sender, item->object_path);
                }
            }
            else if (g_strcmp0(signal, "NewStatus") == 0) {
                g_free(item->status);
                item->status = get_string_property(sender, item->object_path, "Status");
            }
            
            if (sni_state.on_item_changed) {
                sni_state.on_item_changed(item->id);
            }
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎬 Watcher Interface Implementation
// ═══════════════════════════════════════════════════════════════════════════

static const gchar *watcher_xml =
    "<!DOCTYPE node PUBLIC '-//freedesktop//DTD D-BUS Object Introspection 1.0//EN' "
    "'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd'>"
    "<node>"
    "  <interface name='org.kde.StatusNotifierWatcher'>"
    "    <method name='RegisterStatusNotifierItem'>"
    "      <arg type='s' direction='in'/>"
    "    </method>"
    "    <method name='RegisterStatusNotifierHost'>"
    "      <arg type='s' direction='in'/>"
    "    </method>"
    "    <property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
    "    <property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
    "    <property name='ProtocolVersion' type='i' access='read'/>"
    "    <signal name='StatusNotifierItemRegistered'><arg type='s'/></signal>"
    "    <signal name='StatusNotifierItemUnregistered'><arg type='s'/></signal>"
    "    <signal name='StatusNotifierHostRegistered'/>"
    "  </interface>"
    "</node>";

static void handle_watcher_method(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *method,
    GVariant *params, GDBusMethodInvocation *invoc, gpointer data) {
    (void)path; (void)iface; (void)data;
    
    if (g_strcmp0(method, "RegisterStatusNotifierItem") == 0) {
        const gchar *service;
        g_variant_get(params, "(&s)", &service);
        
        // Build full service name
        gchar *full_service;
        if (service[0] == '/') {
            full_service = g_strdup_printf("%s%s", sender, service);
        } else {
            full_service = g_strdup(service);
        }
        
        printf("📝 RegisterStatusNotifierItem: %s\n", full_service);
        
        SNIItem *item = fetch_item_properties(full_service);
        if (item) {
            g_hash_table_insert(sni_state.items, g_strdup(item->id), item);
            
            // Emit signal
            g_dbus_connection_emit_signal(conn, NULL, SNI_WATCHER_PATH,
                SNI_WATCHER_IFACE, "StatusNotifierItemRegistered",
                g_variant_new("(s)", full_service), NULL);
            
            if (sni_state.on_item_added) {
                sni_state.on_item_added(item->id);
            }
        }
        
        g_free(full_service);
        g_dbus_method_invocation_return_value(invoc, NULL);
    }
    else if (g_strcmp0(method, "RegisterStatusNotifierHost") == 0) {
        printf("📝 RegisterStatusNotifierHost\n");
        g_dbus_connection_emit_signal(conn, NULL, SNI_WATCHER_PATH,
            SNI_WATCHER_IFACE, "StatusNotifierHostRegistered", NULL, NULL);
        g_dbus_method_invocation_return_value(invoc, NULL);
    }
    else {
        g_dbus_method_invocation_return_error(invoc, G_DBUS_ERROR,
            G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    }
}

static GVariant* handle_watcher_get_property(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *prop,
    GError **error, gpointer data) {
    (void)conn; (void)sender; (void)path; (void)iface; (void)data; (void)error;
    
    if (g_strcmp0(prop, "RegisteredStatusNotifierItems") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, sni_state.items);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            SNIItem *item = value;
            gchar *service = g_strdup_printf("%s%s", item->bus_name, item->object_path);
            g_variant_builder_add(&builder, "s", service);
            g_free(service);
        }
        
        return g_variant_builder_end(&builder);
    }
    else if (g_strcmp0(prop, "IsStatusNotifierHostRegistered") == 0) {
        return g_variant_new_boolean(TRUE);
    }
    else if (g_strcmp0(prop, "ProtocolVersion") == 0) {
        return g_variant_new_int32(0);
    }
    
    return NULL;
}

static const GDBusInterfaceVTable watcher_vtable = {
    handle_watcher_method,
    handle_watcher_get_property,
    NULL
};

// ═══════════════════════════════════════════════════════════════════════════
// 🚀 Initialization
// ═══════════════════════════════════════════════════════════════════════════

static void fetch_existing_items(void) {
    // Try to get existing items from the system watcher
    GError *error = NULL;
    
    // First try org.kde.StatusNotifierWatcher
    GVariant *result = g_dbus_connection_call_sync(sni_state.conn,
        "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.kde.StatusNotifierWatcher", "RegisteredStatusNotifierItems"),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        g_error_free(error);
        error = NULL;
        // Try freedesktop variant
        result = g_dbus_connection_call_sync(sni_state.conn,
            "org.freedesktop.StatusNotifierWatcher", "/StatusNotifierWatcher",
            "org.freedesktop.DBus.Properties", "Get",
            g_variant_new("(ss)", "org.freedesktop.StatusNotifierWatcher", "RegisteredStatusNotifierItems"),
            G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
        if (error) {
            g_error_free(error);
            return;
        }
    }
    
    if (!result) return;
    
    GVariant *array_v;
    g_variant_get(result, "(v)", &array_v);
    
    GVariantIter iter;
    const gchar *service;
    g_variant_iter_init(&iter, array_v);
    while (g_variant_iter_next(&iter, "&s", &service)) {
        printf("📥 Found existing item: %s\n", service);
        SNIItem *item = fetch_item_properties(service);
        if (item) {
            g_hash_table_insert(sni_state.items, g_strdup(item->id), item);
        }
    }
    
    g_variant_unref(array_v);
    g_variant_unref(result);
    
    printf("📦 Total items loaded: %u\n", g_hash_table_size(sni_state.items));
}

gboolean sni_init(void) {
    GError *error = NULL;
    
    sni_state.conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (error) {
        printf("❌ Failed to connect to session bus: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    sni_state.items = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
        (GDestroyNotify)sni_item_free);
    
    // Register Watcher interface
    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(watcher_xml, &error);
    if (error) {
        printf("❌ Failed to parse watcher XML: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    sni_state.watcher_id = g_dbus_connection_register_object(
        sni_state.conn, SNI_WATCHER_PATH,
        node->interfaces[0], &watcher_vtable, NULL, NULL, &error);
    
    g_dbus_node_info_unref(node);
    
    if (error) {
        printf("❌ Failed to register watcher: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    // Subscribe to watcher signals BEFORE owning the name
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_WATCHER_IFACE, "StatusNotifierItemRegistered", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_registered, NULL, NULL);
    
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_WATCHER_IFACE, "StatusNotifierItemUnregistered", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_unregistered, NULL, NULL);
    
    // Subscribe to item change signals (NewIcon, NewTitle, NewStatus, etc.)
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_ITEM_IFACE, "NewIcon", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_property_changed, NULL, NULL);
    
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_ITEM_IFACE, "NewTitle", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_property_changed, NULL, NULL);
    
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_ITEM_IFACE, "NewStatus", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_property_changed, NULL, NULL);
    
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_ITEM_IFACE, "NewToolTip", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_property_changed, NULL, NULL);
    
    g_dbus_connection_signal_subscribe(sni_state.conn, NULL,
        SNI_ITEM_IFACE, "NewIconThemePath", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, on_item_property_changed, NULL, NULL);
    
    // Fetch existing items BEFORE potentially becoming watcher
    fetch_existing_items();
    
    // Own the watcher name (try kde first, then freedesktop)
    guint kde_id = g_bus_own_name_on_connection(sni_state.conn,
        "org.kde.StatusNotifierWatcher",
        G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL);
    
    guint fd_id = g_bus_own_name_on_connection(sni_state.conn,
        "org.freedesktop.StatusNotifierWatcher",
        G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL);
    
    (void)kde_id; (void)fd_id;
    
    sni_state.is_watcher = TRUE;
    printf("👁️ StatusNotifierWatcher started\n");
    
    return TRUE;
}

void sni_cleanup(void) {
    if (sni_state.watcher_id) {
        g_dbus_connection_unregister_object(sni_state.conn, sni_state.watcher_id);
    }
    if (sni_state.items) {
        g_hash_table_destroy(sni_state.items);
    }
    if (sni_state.conn) {
        g_object_unref(sni_state.conn);
    }
    memset(&sni_state, 0, sizeof(sni_state));
}

// ═══════════════════════════════════════════════════════════════════════════
// 📋 Item Getters
// ═══════════════════════════════════════════════════════════════════════════

GList* sni_get_items(void) {
    GList *list = NULL;
    
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, sni_state.items);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        list = g_list_append(list, value);
    }
    
    return list;
}

SNIItem* sni_get_item(const gchar *id) {
    return g_hash_table_lookup(sni_state.items, id);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎯 Actions
// ═══════════════════════════════════════════════════════════════════════════

gboolean sni_activate(const gchar *id, gint x, gint y) {
    SNIItem *item = sni_get_item(id);
    if (!item) return FALSE;
    
    g_dbus_connection_call(sni_state.conn, item->bus_name, item->object_path,
        SNI_ITEM_IFACE, "Activate", g_variant_new("(ii)", x, y),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    
    printf("🖱️ Activate: %s\n", id);
    return TRUE;
}

gboolean sni_secondary_activate(const gchar *id, gint x, gint y) {
    SNIItem *item = sni_get_item(id);
    if (!item) return FALSE;
    
    g_dbus_connection_call(sni_state.conn, item->bus_name, item->object_path,
        SNI_ITEM_IFACE, "SecondaryActivate", g_variant_new("(ii)", x, y),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    
    printf("🖱️ SecondaryActivate: %s\n", id);
    return TRUE;
}

gboolean sni_scroll(const gchar *id, gint delta, const gchar *orientation) {
    SNIItem *item = sni_get_item(id);
    if (!item) return FALSE;
    
    g_dbus_connection_call(sni_state.conn, item->bus_name, item->object_path,
        SNI_ITEM_IFACE, "Scroll", g_variant_new("(is)", delta, orientation),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    
    printf("🖱️ Scroll: %s (%d %s)\n", id, delta, orientation);
    return TRUE;
}

// ═══════════════════════════════════════════════════════════════════════════
// 📋 Menu (DBusMenu)
// ═══════════════════════════════════════════════════════════════════════════

GVariant* sni_get_menu(const gchar *id) {
    SNIItem *item = sni_get_item(id);
    if (!item || !item->menu_path) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(sni_state.conn,
        item->bus_name, item->menu_path,
        "com.canonical.dbusmenu", "GetLayout",
        g_variant_new("(iias)", 0, -1, NULL),
        G_VARIANT_TYPE("(u(ia{sv}av))"),
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    
    if (error) {
        printf("❌ GetLayout failed: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }
    
    return result;
}

gboolean sni_menu_event(const gchar *id, gint menu_id, const gchar *event) {
    SNIItem *item = sni_get_item(id);
    if (!item || !item->menu_path) return FALSE;
    
    g_dbus_connection_call(sni_state.conn, item->bus_name, item->menu_path,
        "com.canonical.dbusmenu", "Event",
        g_variant_new("(isvu)", menu_id, event, g_variant_new_int32(0), 0),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    
    printf("📋 Menu Event: %s item=%d event=%s\n", id, menu_id, event);
    return TRUE;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🖼️ Icon Pixmap
// ═══════════════════════════════════════════════════════════════════════════

GBytes* sni_get_icon_pixmap(const gchar *id, gint *width, gint *height) {
    SNIItem *item = sni_get_item(id);
    if (!item) return NULL;
    
    *width = 0;
    *height = 0;
    
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(sni_state.conn,
        item->bus_name, item->object_path,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", SNI_ITEM_IFACE, "IconPixmap"),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);
    
    if (error) {
        g_error_free(error);
        return NULL;
    }
    
    GVariant *value;
    g_variant_get(result, "(v)", &value);
    
    // IconPixmap is a(iiay) - array of (width, height, ARGB data)
    if (!g_variant_is_of_type(value, G_VARIANT_TYPE("a(iiay)"))) {
        g_variant_unref(value);
        g_variant_unref(result);
        return NULL;
    }
    
    GVariantIter iter;
    g_variant_iter_init(&iter, value);
    
    gint32 w, h;
    GVariant *data_v;
    gint best_size = 0;
    GBytes *best_data = NULL;
    gint best_w = 0, best_h = 0;
    
    // Find the best icon (prefer 32x32 or close to it)
    while (g_variant_iter_next(&iter, "(ii@ay)", &w, &h, &data_v)) {
        gint size = w * h;
        if (size > best_size && w >= 16 && h >= 16) {
            if (best_data) g_bytes_unref(best_data);
            
            gsize data_len;
            const guchar *data = g_variant_get_fixed_array(data_v, &data_len, 1);
            best_data = g_bytes_new(data, data_len);
            best_w = w;
            best_h = h;
            best_size = size;
        }
        g_variant_unref(data_v);
    }
    
    g_variant_unref(value);
    g_variant_unref(result);
    
    if (best_data) {
        *width = best_w;
        *height = best_h;
        printf("🖼️ Got IconPixmap: %s (%dx%d)\n", id, best_w, best_h);
    }
    
    return best_data;
}

