#include "sni.h"
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 📄 D-Bus XML
// ═══════════════════════════════════════════════════════════════════════════

static const gchar* dbus_xml =
    "<!DOCTYPE node PUBLIC '-//freedesktop//DTD D-BUS Object Introspection 1.0//EN' "
    "'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd'>"
    "<node>"
    "  <interface name='org.venom.SNI'>"
    "    <method name='GetItems'><arg type='a(ssss)' direction='out'/></method>"
    "    <method name='GetItem'><arg type='s' direction='in'/><arg type='(sssss)' direction='out'/></method>"
    "    <method name='GetIconPixmap'><arg type='s' direction='in'/><arg type='ii' direction='out'/><arg type='ay' direction='out'/></method>"
    "    <method name='Activate'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='SecondaryActivate'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='Scroll'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='GetMenu'><arg type='s' direction='in'/><arg type='a(isa{sv})' direction='out'/></method>"
    "    <method name='MenuClick'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "    <signal name='ItemAdded'><arg type='s'/></signal>"
    "    <signal name='ItemRemoved'><arg type='s'/></signal>"
    "    <signal name='ItemChanged'><arg type='s'/></signal>"
    "  </interface>"
    "</node>";

// ═══════════════════════════════════════════════════════════════════════════
// 🔌 D-Bus State
// ═══════════════════════════════════════════════════════════════════════════

static GDBusConnection *dbus_conn = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// 📡 Signal Emission
// ═══════════════════════════════════════════════════════════════════════════

void dbus_emit_item_added(const gchar *id) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/SNI",
        "org.venom.SNI", "ItemAdded", g_variant_new("(s)", id), NULL);
}

void dbus_emit_item_removed(const gchar *id) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/SNI",
        "org.venom.SNI", "ItemRemoved", g_variant_new("(s)", id), NULL);
}

void dbus_emit_item_changed(const gchar *id) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/SNI",
        "org.venom.SNI", "ItemChanged", g_variant_new("(s)", id), NULL);
}

// ═══════════════════════════════════════════════════════════════════════════
// 📋 Parse DBusMenu Layout
// ═══════════════════════════════════════════════════════════════════════════

static void parse_menu_item(GVariantBuilder *b, GVariant *item) {
    gint32 id;
    GVariant *props;
    GVariant *children;
    
    g_variant_get(item, "(i@a{sv}@av)", &id, &props, &children);
    
    // Get label
    const gchar *label = "";
    GVariant *label_v = g_variant_lookup_value(props, "label", G_VARIANT_TYPE_STRING);
    if (label_v) {
        label = g_variant_get_string(label_v, NULL);
    }
    
    // Skip separator-only (empty label and no children)
    GVariant *type_v = g_variant_lookup_value(props, "type", G_VARIANT_TYPE_STRING);
    gboolean is_separator = type_v && g_strcmp0(g_variant_get_string(type_v, NULL), "separator") == 0;
    
    if (!is_separator || (label && strlen(label) > 0)) {
        g_variant_builder_add(b, "(is@a{sv})", id, label, props);
    }
    
    if (label_v) g_variant_unref(label_v);
    if (type_v) g_variant_unref(type_v);
    
    // Recurse into children
    GVariantIter iter;
    GVariant *child;
    g_variant_iter_init(&iter, children);
    while ((child = g_variant_iter_next_value(&iter))) {
        GVariant *inner;
        g_variant_get(child, "v", &inner);
        parse_menu_item(b, inner);
        g_variant_unref(inner);
        g_variant_unref(child);
    }
    
    g_variant_unref(props);
    g_variant_unref(children);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎛️ Method Handler
// ═══════════════════════════════════════════════════════════════════════════

static void handle_method(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *interface, const gchar *method,
    GVariant *params, GDBusMethodInvocation *invoc, gpointer data) {
    (void)conn; (void)sender; (void)path; (void)interface; (void)data;
    
    printf("🔧 SNI: %s\n", method);
    
    if (g_strcmp0(method, "GetItems") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ssss)"));
        
        GList *items = sni_get_items();
        for (GList *l = items; l; l = l->next) {
            SNIItem *item = l->data;
            g_variant_builder_add(&b, "(ssss)",
                item->id ? item->id : "",
                item->title ? item->title : "",
                item->icon_name ? item->icon_name : "",
                item->status ? item->status : "Active");
        }
        g_list_free(items);
        
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(ssss))", &b));
    }
    else if (g_strcmp0(method, "GetItem") == 0) {
        const gchar *id;
        g_variant_get(params, "(&s)", &id);
        
        SNIItem *item = sni_get_item(id);
        if (item) {
            g_dbus_method_invocation_return_value(invoc, g_variant_new("((sssss))",
                item->id ? item->id : "",
                item->title ? item->title : "",
                item->icon_name ? item->icon_name : "",
                item->status ? item->status : "Active",
                item->category ? item->category : "ApplicationStatus"));
        } else {
            g_dbus_method_invocation_return_error(invoc, G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED, "Item not found");
        }
    }
    else if (g_strcmp0(method, "GetIconPixmap") == 0) {
        const gchar *id;
        g_variant_get(params, "(&s)", &id);
        
        gint width = 0, height = 0;
        GBytes *pixmap = sni_get_icon_pixmap(id, &width, &height);
        
        if (pixmap) {
            gsize len;
            const guchar *data = g_bytes_get_data(pixmap, &len);
            GVariantBuilder b;
            g_variant_builder_init(&b, G_VARIANT_TYPE("ay"));
            for (gsize i = 0; i < len; i++) {
                g_variant_builder_add(&b, "y", data[i]);
            }
            g_dbus_method_invocation_return_value(invoc, 
                g_variant_new("(iiay)", width, height, &b));
            g_bytes_unref(pixmap);
        } else {
            GVariantBuilder b;
            g_variant_builder_init(&b, G_VARIANT_TYPE("ay"));
            g_dbus_method_invocation_return_value(invoc, 
                g_variant_new("(iiay)", 0, 0, &b));
        }
    }
    else if (g_strcmp0(method, "Activate") == 0) {
        const gchar *id; gint x, y;
        g_variant_get(params, "(&sii)", &id, &x, &y);
        g_dbus_method_invocation_return_value(invoc, 
            g_variant_new("(b)", sni_activate(id, x, y)));
    }
    else if (g_strcmp0(method, "SecondaryActivate") == 0) {
        const gchar *id; gint x, y;
        g_variant_get(params, "(&sii)", &id, &x, &y);
        g_dbus_method_invocation_return_value(invoc, 
            g_variant_new("(b)", sni_secondary_activate(id, x, y)));
    }
    else if (g_strcmp0(method, "Scroll") == 0) {
        const gchar *id; gint delta; const gchar *orientation;
        g_variant_get(params, "(&si&s)", &id, &delta, &orientation);
        g_dbus_method_invocation_return_value(invoc, 
            g_variant_new("(b)", sni_scroll(id, delta, orientation)));
    }
    else if (g_strcmp0(method, "GetMenu") == 0) {
        const gchar *id;
        g_variant_get(params, "(&s)", &id);
        
        GVariant *menu = sni_get_menu(id);
        if (menu) {
            // Parse the menu layout
            GVariantBuilder b;
            g_variant_builder_init(&b, G_VARIANT_TYPE("a(isa{sv})"));
            
            guint32 revision;
            GVariant *layout;
            g_variant_get(menu, "(u@(ia{sv}av))", &revision, &layout);
            parse_menu_item(&b, layout);
            
            g_variant_unref(layout);
            g_variant_unref(menu);
            
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(isa{sv}))", &b));
        } else {
            GVariantBuilder b;
            g_variant_builder_init(&b, G_VARIANT_TYPE("a(isa{sv})"));
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(isa{sv}))", &b));
        }
    }
    else if (g_strcmp0(method, "MenuClick") == 0) {
        const gchar *id; gint menu_id;
        g_variant_get(params, "(&si)", &id, &menu_id);
        g_dbus_method_invocation_return_value(invoc, 
            g_variant_new("(b)", sni_menu_event(id, menu_id, "clicked")));
    }
    else {
        g_dbus_method_invocation_return_error(invoc, G_DBUS_ERROR,
            G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    }
}

static const GDBusInterfaceVTable vtable = { handle_method, NULL, NULL };

// ═══════════════════════════════════════════════════════════════════════════
// 🚀 D-Bus Setup
// ═══════════════════════════════════════════════════════════════════════════

const gchar* dbus_get_xml(void) {
    return dbus_xml;
}

void dbus_set_connection(GDBusConnection *conn) {
    dbus_conn = conn;
}

const GDBusInterfaceVTable* dbus_get_vtable(void) {
    return &vtable;
}
