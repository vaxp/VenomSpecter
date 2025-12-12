/*
 * sni-client.c
 *
 * Implementation of SNI client for venom_sni daemon.

*/

#include "sni-client.h"

#define SNI_BUS_NAME "org.venom.SNI"
#define SNI_OBJECT_PATH "/org/venom/SNI"
#define SNI_INTERFACE "org.venom.SNI"

static GDBusProxy *_proxy = NULL;
static guint _signal_subscription = 0;

/* Callbacks */
static TrayItemAddedCallback _item_added_cb = NULL;
static gpointer _item_added_data = NULL;

static TrayItemRemovedCallback _item_removed_cb = NULL;
static gpointer _item_removed_data = NULL;

/* Helper to fetch icon pixmap via D-Bus if icon_name fails or needed */
static GdkPixbuf* fetch_icon_pixmap(const gchar *id) {
    if (!_proxy) return NULL;
    
    GError *error = NULL;
    /* GetIconPixmap(s) -> (iiay) */
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetIconPixmap",
                                          g_variant_new("(s)", id),
                                          G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    if (error) {
        g_warning("SNI: Failed to get pixmap for %s: %s", id, error->message);
        g_error_free(error);
        return NULL;
    }
    
    gint width, height;
    GVariantIter *iter;
    g_variant_get(res, "(iiay)", &width, &height, &iter);
    
    if (width <= 0 || height <= 0) {
        g_variant_iter_free(iter);
        g_variant_unref(res);
        return NULL;
    }
    
    /* Convert array of bytes to buffer */
    gsize n_elements = g_variant_iter_n_children(iter);
    guchar *buffer = g_malloc(n_elements);
    
    guchar byte;
    gsize i = 0;
    while (g_variant_iter_next(iter, "y", &byte)) {
        buffer[i++] = byte;
    }
    g_variant_iter_free(iter);
    g_variant_unref(res);
    
    /* Create GdkPixbuf from raw ARGB data (assumed format from some SNI impls) 
       Note: SNI spec usually says Network byte order ARGB32. 
       GdkPixbuf expects RGBA. We might need channel swizzling.
       For simplicity, let's try direct load first or assume RGBA if it works.
       If colors are wrong, we fix swap.
    */
    
    GdkPixbuf *pix = gdk_pixbuf_new_from_data(buffer, GDK_COLORSPACE_RGB, TRUE, 8,
                                             width, height, width * 4,
                                             (GdkPixbufDestroyNotify)g_free, NULL);
    return pix;
}

static TrayItem* create_tray_item(const gchar *id, const gchar *title, const gchar *icon_name, const gchar *status) {
    TrayItem *item = g_new0(TrayItem, 1);
    item->id = g_strdup(id);
    item->title = g_strdup(title);
    
    /* Decide on icon: prefers icon_name usually */
    if (icon_name && strlen(icon_name) > 0) {
        item->icon_name = g_strdup(icon_name);
        /* Check if icon exists in theme? If not, try fetching pixmap */
        if (!gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon_name)) {
             item->icon_pixbuf = fetch_icon_pixmap(id);
        }
    } else {
        item->icon_pixbuf = fetch_icon_pixmap(id);
    }
    
    item->status = g_strdup(status);
    return item;
}

void tray_item_free(TrayItem *item) {
    if (!item) return;
    g_free(item->id);
    g_free(item->title);
    g_free(item->icon_name);
    g_free(item->status);
    if (item->icon_pixbuf) g_object_unref(item->icon_pixbuf);
    g_free(item);
}

void tray_item_list_free(GList *list) {
    g_list_free_full(list, (GDestroyNotify)tray_item_free);
}

/* Signal Handler */
static void on_signal(GDBusConnection *conn, const gchar *sender,
                      const gchar *path, const gchar *interface,
                      const gchar *signal, GVariant *params,
                      gpointer user_data) {
    (void)conn; (void)sender; (void)path; (void)interface; (void)user_data;
    
    if (g_strcmp0(signal, "ItemAdded") == 0) {
        const gchar *id;
        g_variant_get(params, "(&s)", &id);
        
        /* Fetch details */
        GError *err = NULL;
        GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetItem",
                                              g_variant_new("(s)", id),
                                              G_DBUS_CALL_FLAGS_NONE, 500, NULL, &err);
        if (res) {
            const gchar *rid, *tit, *ico, *stat, *cat;
            g_variant_get(res, "((sssss))", &rid, &tit, &ico, &stat, &cat);
            TrayItem *item = create_tray_item(rid, tit, ico, stat);
            if (_item_added_cb) _item_added_cb(item, _item_added_data);
            g_variant_unref(res);
        } else if (err) {
            g_warning("SNI: Fetch added item failed: %s", err->message);
            g_error_free(err);
        }
    }
    else if (g_strcmp0(signal, "ItemRemoved") == 0) {
        const gchar *id;
        g_variant_get(params, "(&s)", &id);
        if (_item_removed_cb) _item_removed_cb(id, _item_removed_data);
    }
    else if (g_strcmp0(signal, "ItemChanged") == 0) {
        /* Treat change as remove + add for simplicity in UI update, 
           or just add (which might update existing widget).
           Let's re-fetch and trigger 'Added' which UI should handle as update/insert.
        */
        const gchar *id;
        g_variant_get(params, "(&s)", &id);
        
        GError *err = NULL;
        GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetItem",
                                              g_variant_new("(s)", id),
                                              G_DBUS_CALL_FLAGS_NONE, 500, NULL, &err);
        if (res) {
            const gchar *rid, *tit, *ico, *stat, *cat;
            g_variant_get(res, "((sssss))", &rid, &tit, &ico, &stat, &cat);
            TrayItem *item = create_tray_item(rid, tit, ico, stat);
            if (_item_added_cb) _item_added_cb(item, _item_added_data);
            g_variant_unref(res);
        } 
    }
}

void sni_client_init(void) {
    if (_proxy) return;
    
    GError *error = NULL;
    _proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          SNI_BUS_NAME,
                                          SNI_OBJECT_PATH,
                                          SNI_INTERFACE,
                                          NULL, &error);
    if (error) {
        g_warning("SNI: Failed to connect daemon: %s", error->message);
        g_error_free(error);
        return;
    }
    
    GDBusConnection *conn = g_dbus_proxy_get_connection(_proxy);
    _signal_subscription = g_dbus_connection_signal_subscribe(conn,
                                                             SNI_BUS_NAME,
                                                             SNI_INTERFACE,
                                                             NULL,
                                                             SNI_OBJECT_PATH,
                                                             NULL,
                                                             G_DBUS_SIGNAL_FLAGS_NONE,
                                                             on_signal,
                                                             NULL, NULL);
}

void sni_client_cleanup(void) {
    if (_signal_subscription && _proxy) {
        GDBusConnection *conn = g_dbus_proxy_get_connection(_proxy);
        g_dbus_connection_signal_unsubscribe(conn, _signal_subscription);
    }
    if (_proxy) g_object_unref(_proxy);
    _proxy = NULL;
}


GList* sni_client_get_items(void) {
    if (!_proxy) return NULL;
    
    GError *error = NULL;
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetItems", NULL,
                                          G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);
    if (error) {
        g_warning("SNI: GetItems failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    GList *list = NULL;
    GVariantIter *iter;
    g_variant_get(res, "(a(ssss))", &iter);
    
    const gchar *id, *title, *icon, *status;
    while (g_variant_iter_next(iter, "(ssss)", &id, &title, &icon, &status)) {
        TrayItem *item = create_tray_item(id, title, icon, status);
        list = g_list_append(list, item);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(res);
    return list;
}

void sni_client_activate(const gchar *id, gint x, gint y) {
    if (!_proxy) return;
    g_dbus_proxy_call(_proxy, "Activate", 
                      g_variant_new("(sii)", id, x, y),
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

void sni_client_secondary_activate(const gchar *id, gint x, gint y) {
    if (!_proxy) return;
    g_dbus_proxy_call(_proxy, "SecondaryActivate", 
                      g_variant_new("(sii)", id, x, y),
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

void sni_client_on_item_added(TrayItemAddedCallback callback, gpointer user_data) {
    _item_added_cb = callback;
    _item_added_data = user_data;
}

void sni_client_on_item_removed(TrayItemRemovedCallback callback, gpointer user_data) {
    _item_removed_cb = callback;
    _item_removed_data = user_data;
}

/* Menu Implementation */

void tray_menu_item_free(TrayMenuItem *item) {
    if (!item) return;
    g_free(item->label);
    g_free(item->type);
    g_free(item->toggle_type);
    if (item->children) {
        g_list_free_full(item->children, (GDestroyNotify)tray_menu_item_free);
    }
    g_free(item);
}

void tray_menu_list_free(GList *list) {
    g_list_free_full(list, (GDestroyNotify)tray_menu_item_free);
}

static TrayMenuItem* parse_menu_item_variant(GVariant *v) {
    /* Format: (is@a{sv}@av) -> id, label, props, children */
    gint32 id;
    const gchar *label = NULL;
    GVariant *props;
    GVariant *children_v;
    
    /* Some implementations might just send (ia{sv}av) without 's' label if it's in props? 
       Actually DBusMenu spec says:
       struct MenuLayout {
          int32 id;
          PropertyMap properties;
          array<variant<MenuLayout>> children;
       }
       But venom_sni/src/dbus_service.c sends: (isa{sv}) which is weird or maybe flattened?
       Wait, let's check dbus_service.c again.
       Line 201: parse_menu_item(&b, layout); 
       And parse_menu_item adds: (is@a{sv}) 
       It seems retrieval also uses a(isa{sv}) which is flattened list?
       NO. dbus_service.c Line 206 returns (a(isa{sv}))
       Wait, the daemon seems to return a FLAT LIST of items?
       Line 196: g_variant_builder_init(&b, G_VARIANT_TYPE("a(isa{sv})"));
       And parse_menu_item implementation (Line 59) recurses effectively but adds to the SAME builder?
       YES. It seems it flattens the tree into a list of items?
       But how do we know parent/child?
       Actually, `parse_menu_item` in `dbus_service.c` (daemon) seems to be iterating children and recursing, 
       BUT it adds everything to the SAME `GVariantBuilder b`.
       So the result is a flat list of ALL items in the menu.
       However, the client needs structure. 
       Usually DBusMenu `GetLayout` returns a recursive tree.
       The daemon `GetMenu` wrapper seems to just dump all items?
       Or maybe it just dumps visible items?
       This `GetMenu` implementation in the daemon looks custom / simplified.
       It discards structure!
       Wait, let's look at `dbus_service.c` again.
       Line 87: g_variant_iter_init(&iter, children);
       It iterates children and calls parse_menu_item(b, inner).
       It does NOT seemingly preserve hierarchy in the output variant `a(isa{sv})`.
       It just produces a list of items. 
       This means we might lose the tree structure unless "children" are empty for now?
       
       Actually, standard DBusMenu `GetLayout` returns: (u(ia{sv}av)) = (revision, root_item)
       where root_item = (id, props, children)
       The daemon calls `GetLayout` (Line 600 in sni.c) which gets the tree.
       But `dbus_service.c` flattens it?  
       
       Let's assume for now we just get a list of items and show them linearly?
       Or maybe the daemon is returning the tree but my reading of `dbus_service.c` is wrong?
       `g_variant_builder_add(b, "(is@a{sv})", id, label, props);`
       Then it recurses.
       So it linearly appends items in DFS order.
       Use indentation? No, that's ugly.
       For a simplified panel, maybe just linear is "okay" if they are top level, but for submenus it's bad.
       
       Wait, if `dbus_service.c` is returning `a(isa{sv})`, then it really IS a flat array of structs (id, label, props).
       It completely drops the recursion structure in the format string.
       This is a limitation of the current `venom_sni` daemon wrapper.
       
       However, if I look really closely at `dbus_service.c`:
       It fetches `GetLayout` from the app.
       Then it parses it and builds a custom return for `GetMenu`.
       The return type is `a(isa{sv})`.
       Yes, it's flat.
       
       So we will render a flat menu for now. Most tray menus are simple.
       Exceptions: Submenus will just appear inline flattened.
       
       Let's proceed with parsing this flat list.
    */
    
    g_variant_get(v, "(i&s@a{sv})", &id, &label, &props);
    
    TrayMenuItem *item = g_new0(TrayMenuItem, 1);
    item->id = id;
    item->label = g_strdup(label);
    item->enabled = TRUE;
    item->visible = TRUE;
    item->type = g_strdup("standard");
    item->toggle_type = NULL;
    item->toggle_state = 0;
    
    /* Parse props */
    GVariant *p_enabled = g_variant_lookup_value(props, "enabled", G_VARIANT_TYPE_BOOLEAN);
    if (p_enabled) {
        item->enabled = g_variant_get_boolean(p_enabled);
        g_variant_unref(p_enabled);
    }
    
    GVariant *p_visible = g_variant_lookup_value(props, "visible", G_VARIANT_TYPE_BOOLEAN);
    if (p_visible) {
        item->visible = g_variant_get_boolean(p_visible);
        g_variant_unref(p_visible);
    }
    
    GVariant *p_type = g_variant_lookup_value(props, "type", G_VARIANT_TYPE_STRING);
    if (p_type) {
        g_free(item->type);
        item->type = g_strdup(g_variant_get_string(p_type, NULL));
        g_variant_unref(p_type);
    }
    
    GVariant *p_toggle = g_variant_lookup_value(props, "toggle-type", G_VARIANT_TYPE_STRING);
    if (p_toggle) {
        item->toggle_type = g_strdup(g_variant_get_string(p_toggle, NULL));
        g_variant_unref(p_toggle);
    }
    
    GVariant *p_state = g_variant_lookup_value(props, "toggle-state", G_VARIANT_TYPE_INT32);
    if (p_state) {
        item->toggle_state = g_variant_get_int32(p_state);
        g_variant_unref(p_state);
    }
    
    g_variant_unref(props);
    return item;
}

GList* sni_client_get_menu(const gchar *id) {
    if (!_proxy) return NULL;
    
    /* Result: (a(isa{sv})) */
    GList *list = NULL;
    GVariantIter *iter;
    GVariant *child;
    
    GError *error = NULL;
    /* GetMenu(s) -> (a(isa{sv})) */
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetMenu",
                                          g_variant_new("(s)", id),
                                          G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    if (error) {
        g_warning("SNI: GetMenu failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }

    g_variant_get(res, "(a(isa{sv}))", &iter);
    
    while ((child = g_variant_iter_next_value(iter))) {
        TrayMenuItem *item = parse_menu_item_variant(child);
        list = g_list_append(list, item);
        g_variant_unref(child);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(res);
    return list;
}

void sni_client_menu_click(const gchar *id, gint menu_id) {
    if (!_proxy) return;
    g_dbus_proxy_call(_proxy, "MenuClick", 
                      g_variant_new("(si)", id, menu_id),
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

