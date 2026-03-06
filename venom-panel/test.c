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
                                      "GetAll", g_variant_new("(s)", "org.mpris.MediaPlayer2.Player"),
                                      G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
    
    if (!res) return 1;
    
    g_variant_get(res, "(a{sv})", &iter);
    const gchar *key;
    GVariant *val;
    while (g_variant_iter_loop(iter, "{&sv}", &key, &val)) {
        printf("Key: %s\n", key);
        printf("  val type: %s\n", g_variant_get_type_string(val));
        
        if (g_strcmp0(key, "Metadata") == 0) {
            GVariantIter *miter;
            g_variant_get(val, "a{sv}", &miter);
            const gchar *mkey;
            GVariant *mval;
            while (g_variant_iter_loop(miter, "{&sv}", &mkey, &mval)) {
                printf("  Meta Key: %s\n", mkey);
                printf("    mval type: %s\n", g_variant_get_type_string(mval));
            }
            g_variant_iter_free(miter);
        }
    }
    
    return 0;
}
