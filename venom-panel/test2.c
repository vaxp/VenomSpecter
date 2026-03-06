#include <gio/gio.h>
#include <stdio.h>

static gchar* extract_string(GVariant *dict, const char *key) {
    if (!dict) return NULL;
    GVariant *v = g_variant_lookup_value(dict, key, NULL);
    if (!v) return NULL;
    
    gchar *result = NULL;
    
    // Check if the value is a variant box, if so unbox it
    GVariant *real_val = v;
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_VARIANT)) {
        real_val = g_variant_get_variant(v);
        g_variant_unref(v);
    }
    
    if (g_variant_is_of_type(real_val, G_VARIANT_TYPE_STRING)) {
        result = g_strdup(g_variant_get_string(real_val, NULL));
    } else {
        printf("extract_string: key %s is not string, it is %s\n", key, g_variant_get_type_string(real_val));
    }
    
    g_variant_unref(real_val);
    return result;
}

static gchar* extract_first_array_string(GVariant *dict, const char *key) {
    if (!dict) return NULL;
    GVariant *v = g_variant_lookup_value(dict, key, NULL);
    if (!v) return NULL;
    
    gchar *result = NULL;
    
    GVariant *real_val = v;
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_VARIANT)) {
        real_val = g_variant_get_variant(v);
        g_variant_unref(v);
    }
    
    if (g_variant_is_of_type(real_val, G_VARIANT_TYPE_STRING_ARRAY)) {
        GVariantIter iter;
        g_variant_iter_init(&iter, real_val);
        gchar *str = NULL;
        if (g_variant_iter_next(&iter, "s", &str)) {
            result = str; /* takes ownership */
            gchar *dummy;
            while (g_variant_iter_next(&iter, "s", &dummy)) g_free(dummy);
        }
    } else {
        printf("extract_first_array_string: key %s is not array string, it is %s\n", key, g_variant_get_type_string(real_val));
    }
    
    g_variant_unref(real_val);
    return result;
}

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
    
    GVariant *tuple = res;
    GVariant *dict = g_variant_get_child_value(tuple, 0); 
    
    printf("dict type: %s\n", g_variant_get_type_string(dict));
    
    /* 1. جلب حالة التشغيل */
    gchar *status_str = extract_string(dict, "PlaybackStatus");
    if (status_str) {
        printf("Playing Status: %s\n", status_str);
        g_free(status_str);
    }
    
    /* 2. جلب الميتاداتا */
    GVariant *meta_v = g_variant_lookup_value(dict, "Metadata", NULL);
    if (meta_v) {
        GVariant *meta_dict = meta_v;
        if (g_variant_is_of_type(meta_v, G_VARIANT_TYPE_VARIANT)) {
            meta_dict = g_variant_get_variant(meta_v);
            g_variant_unref(meta_v);
        }
        
        printf("meta dict type: %s\n", g_variant_get_type_string(meta_dict));
        
        gchar *title = extract_string(meta_dict, "xesam:title");
        if (title) {
            printf("Title: %s\n", title);
            g_free(title);
        }
        
        gchar *artist = extract_first_array_string(meta_dict, "xesam:artist");
        if (artist) {
            printf("Artist: %s\n", artist);
            g_free(artist);
        }
        
        g_variant_unref(meta_dict);
    }
    
    g_variant_unref(dict);
    g_variant_unref(tuple);
    
    return 0;
}
