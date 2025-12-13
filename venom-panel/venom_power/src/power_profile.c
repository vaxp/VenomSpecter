#include "power_profile.h"
#include "venom_power.h"
#include "dbus_service.h"
#include <stdio.h>
#include <string.h>

static gchar *current_profile = NULL;
static gchar *inhibited_reason = NULL;
static GList *available_profiles = NULL;
static guint signal_subscription_id = 0;
static gboolean ppd_available = FALSE;

static GVariant* call_ppd_method(const gchar *method, GVariant *params) {
    if (!power_state.system_conn) return NULL;
    
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        power_state.system_conn,
        PPD_DBUS_NAME,
        PPD_DBUS_PATH,
        "org.freedesktop.DBus.Properties",
        method,
        params,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error
    );
    
    if (error) {
        g_error_free(error);
        return NULL;
    }
    
    return result;
}

static void refresh_active_profile(void) {
    GVariant *result = call_ppd_method("Get", 
        g_variant_new("(ss)", PPD_DBUS_INTERFACE, "ActiveProfile"));
    
    if (result) {
        GVariant *value = NULL;
        g_variant_get(result, "(v)", &value);
        if (value) {
            g_free(current_profile);
            current_profile = g_strdup(g_variant_get_string(value, NULL));
            g_variant_unref(value);
        }
        g_variant_unref(result);
    }
}

static void refresh_inhibited_reason(void) {
    GVariant *result = call_ppd_method("Get",
        g_variant_new("(ss)", PPD_DBUS_INTERFACE, "PerformanceInhibited"));
    
    if (result) {
        GVariant *value = NULL;
        g_variant_get(result, "(v)", &value);
        if (value) {
            g_free(inhibited_reason);
            inhibited_reason = g_strdup(g_variant_get_string(value, NULL));
            g_variant_unref(value);
        }
        g_variant_unref(result);
    }
}

static void refresh_profiles_list(void) {
    g_list_free_full(available_profiles, g_free);
    available_profiles = NULL;
    
    GVariant *result = call_ppd_method("Get",
        g_variant_new("(ss)", PPD_DBUS_INTERFACE, "Profiles"));
    
    if (result) {
        GVariant *value = NULL;
        g_variant_get(result, "(v)", &value);
        if (value) {
            GVariantIter iter;
            GVariant *profile_dict;
            g_variant_iter_init(&iter, value);
            
            while ((profile_dict = g_variant_iter_next_value(&iter))) {
                GVariant *profile_name = g_variant_lookup_value(profile_dict, "Profile", G_VARIANT_TYPE_STRING);
                if (profile_name) {
                    available_profiles = g_list_append(available_profiles, 
                        g_strdup(g_variant_get_string(profile_name, NULL)));
                    g_variant_unref(profile_name);
                }
                g_variant_unref(profile_dict);
            }
            g_variant_unref(value);
        }
        g_variant_unref(result);
    }
}

static void on_ppd_properties_changed(GDBusConnection *connection,
                                       const gchar *sender_name,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *signal_name,
                                       GVariant *parameters,
                                       gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path;
    (void)interface_name; (void)signal_name; (void)user_data;
    
    GVariant *changed_props = NULL;
    const gchar *iface = NULL;
    
    g_variant_get(parameters, "(&s@a{sv}as)", &iface, &changed_props, NULL);
    
    if (changed_props) {
        GVariant *profile_v = g_variant_lookup_value(changed_props, "ActiveProfile", G_VARIANT_TYPE_STRING);
        if (profile_v) {
            const gchar *new_profile = g_variant_get_string(profile_v, NULL);
            
            if (g_strcmp0(new_profile, current_profile) != 0) {
                g_free(current_profile);
                current_profile = g_strdup(new_profile);
                printf("⚡ Power profile changed: %s\n", current_profile);
                dbus_emit_signal("ProfileChanged", g_variant_new("(s)", current_profile));
            }
            g_variant_unref(profile_v);
        }
        
        GVariant *inhibited_v = g_variant_lookup_value(changed_props, "PerformanceInhibited", G_VARIANT_TYPE_STRING);
        if (inhibited_v) {
            g_free(inhibited_reason);
            inhibited_reason = g_strdup(g_variant_get_string(inhibited_v, NULL));
            g_variant_unref(inhibited_v);
        }
        
        g_variant_unref(changed_props);
    }
}

gboolean power_profile_init(void) {
    if (!power_state.system_conn) {
        fprintf(stderr, "⚠️ Cannot init power profiles: no system bus\n");
        return FALSE;
    }
    
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        power_state.system_conn,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameHasOwner",
        g_variant_new("(s)", PPD_DBUS_NAME),
        G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error
    );
    
    if (error) {
        fprintf(stderr, "⚠️ Power profiles daemon not available: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gboolean has_owner = FALSE;
    g_variant_get(result, "(b)", &has_owner);
    g_variant_unref(result);
    
    if (!has_owner) {
        fprintf(stderr, "⚠️ power-profiles-daemon is not running\n");
        return FALSE;
    }
    
    refresh_active_profile();
    refresh_inhibited_reason();
    refresh_profiles_list();
    
    signal_subscription_id = g_dbus_connection_signal_subscribe(
        power_state.system_conn,
        PPD_DBUS_NAME,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        PPD_DBUS_PATH,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_ppd_properties_changed,
        NULL, NULL
    );
    
    ppd_available = TRUE;
    printf("⚡ Power profiles: %s (available: ", current_profile ? current_profile : "unknown");
    for (GList *l = available_profiles; l; l = l->next) {
        printf("%s%s", (gchar*)l->data, l->next ? ", " : "");
    }
    printf(")\n");
    
    return TRUE;
}

void power_profile_cleanup(void) {
    if (signal_subscription_id && power_state.system_conn) {
        g_dbus_connection_signal_unsubscribe(power_state.system_conn, signal_subscription_id);
        signal_subscription_id = 0;
    }
    g_free(current_profile);
    g_free(inhibited_reason);
    g_list_free_full(available_profiles, g_free);
    current_profile = NULL;
    inhibited_reason = NULL;
    available_profiles = NULL;
    ppd_available = FALSE;
}

const gchar* power_profile_get_active(void) {
    if (!ppd_available) return "unknown";
    refresh_active_profile();
    return current_profile ? current_profile : "unknown";
}

gboolean power_profile_set_active(const gchar *profile) {
    if (!ppd_available || !power_state.system_conn) return FALSE;
    
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        power_state.system_conn,
        PPD_DBUS_NAME,
        PPD_DBUS_PATH,
        "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", PPD_DBUS_INTERFACE, "ActiveProfile", 
                      g_variant_new_string(profile)),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error
    );
    
    if (error) {
        fprintf(stderr, "⚠️ Failed to set profile: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    if (result) g_variant_unref(result);
    
    g_free(current_profile);
    current_profile = g_strdup(profile);
    printf("⚡ Power profile set to: %s\n", profile);
    
    return TRUE;
}

GVariant* power_profile_get_list(void) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    
    if (ppd_available) {
        refresh_profiles_list();
        for (GList *l = available_profiles; l; l = l->next) {
            g_variant_builder_add(&builder, "s", (gchar*)l->data);
        }
    }
    
    return g_variant_builder_end(&builder);
}

const gchar* power_profile_get_inhibited_reason(void) {
    if (!ppd_available) return "";
    refresh_inhibited_reason();
    return inhibited_reason ? inhibited_reason : "";
}

gboolean power_profile_is_available(void) {
    return ppd_available;
}

PowerProfileType power_profile_string_to_type(const gchar *profile) {
    if (g_strcmp0(profile, "power-saver") == 0) return POWER_PROFILE_POWER_SAVER;
    if (g_strcmp0(profile, "balanced") == 0) return POWER_PROFILE_BALANCED;
    if (g_strcmp0(profile, "performance") == 0) return POWER_PROFILE_PERFORMANCE;
    return POWER_PROFILE_UNKNOWN;
}

const gchar* power_profile_type_to_string(PowerProfileType type) {
    switch (type) {
        case POWER_PROFILE_POWER_SAVER: return "power-saver";
        case POWER_PROFILE_BALANCED: return "balanced";
        case POWER_PROFILE_PERFORMANCE: return "performance";
        default: return "unknown";
    }
}
