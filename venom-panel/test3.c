#include <gio/gio.h>
#include <stdio.h>

int main() {
    GError *err = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (!bus) return 1;
    
    // Find chrome
    GVariant *res = g_dbus_connection_call_sync(bus,
                                                "org.freedesktop.DBus",
                                                "/org/freedesktop/DBus",
                                                "org.freedesktop.DBus",
                                                "ListNames",
                                                NULL,
                                                G_VARIANT_TYPE("(as)"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1, NULL, &err);
    GVariantIter *iter;
    g_variant_get(res, "(as)", &iter);
    char *name;
    char *chrome = NULL;
    while (g_variant_iter_next(iter, "s", &name)) {
        if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.chrom")) {
            chrome = name;
            break;
        }
        g_free(name);
    }
    
    if (!chrome) return 1;
    printf("Found: %s\n", chrome);
    
    res = g_dbus_connection_call_sync(bus, chrome, "/org/mpris/MediaPlayer2",
                                      "org.freedesktop.DBus.Properties",
                                      "Get", g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "PlaybackStatus"),
                                      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
    
    if (!res) {
        printf("Get failed\n");
        return 1; 
    }
    
    GVariant *v_val;
    g_variant_get(res, "(v)", &v_val);
    
    printf("v_val type: %s\n", g_variant_get_type_string(v_val));
    
    GVariant *unboxed = v_val;
    if (g_variant_is_of_type(v_val, G_VARIANT_TYPE_VARIANT)) {
        unboxed = g_variant_get_variant(v_val);
        g_variant_unref(v_val);
    }
    
    printf("unboxed type: %s\n", g_variant_get_type_string(unboxed));
    
    if (g_variant_is_of_type(unboxed, G_VARIANT_TYPE_STRING)) {
        printf("Status: %s\n", g_variant_get_string(unboxed, NULL));
    }
    
    g_variant_unref(unboxed);
    g_variant_unref(res);
    
    return 0;
}
