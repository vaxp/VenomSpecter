#include "audio-client.h"
#include <stdio.h>

#define BUS_NAME "org.venom.Audio"
#define OBJECT_PATH "/org/venom/Audio"
#define INTERFACE_NAME "org.venom.Audio"

static GDBusProxy *_proxy = NULL;
static AudioVolumeCallback _vol_cb = NULL;
static gpointer _vol_user_data = NULL;
static AudioMuteCallback _mute_cb = NULL;
static gpointer _mute_user_data = NULL;

static void on_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
                      GVariant *parameters, gpointer user_data) {
    (void)proxy; (void)sender_name; (void)user_data;
    
    if (g_strcmp0(signal_name, "VolumeChanged") == 0) {
        if (_vol_cb) {
            gint vol;
            g_variant_get(parameters, "(i)", &vol);
            _vol_cb(vol, _vol_user_data);
        }
    } else if (g_strcmp0(signal_name, "MuteChanged") == 0) {
        if (_mute_cb) {
            gboolean muted;
            g_variant_get(parameters, "(b)", &muted);
            _mute_cb(muted, _mute_user_data);
        }
    }
}

static void ensure_proxy(void) {
    if (_proxy) return;
    
    GError *error = NULL;
    _proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          BUS_NAME,
                                          OBJECT_PATH,
                                          INTERFACE_NAME,
                                          NULL, &error);
    if (error) {
        g_warning("Audio: Failed to connect to daemon: %s", error->message);
        g_error_free(error);
        _proxy = NULL;
    } else {
        g_signal_connect(_proxy, "g-signal", G_CALLBACK(on_signal), NULL);
    }
}

void audio_client_init(void) {
    ensure_proxy();
}

void audio_client_cleanup(void) {
    if (_proxy) {
        g_object_unref(_proxy);
        _proxy = NULL;
    }
}

/* Volume */
gint audio_get_volume(void) {
    ensure_proxy();
    if (!_proxy) return 0;
    
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetVolume", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    if (res) {
        gint vol;
        g_variant_get(res, "(i)", &vol);
        g_variant_unref(res);
        return vol;
    }
    return 0;
}

void audio_set_volume(gint percent) {
    ensure_proxy();
    if (!_proxy) return;
    
    g_dbus_proxy_call(_proxy, "SetVolume", g_variant_new("(i)", percent), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

/* Mute */
gboolean audio_get_muted(void) {
    ensure_proxy();
    if (!_proxy) return FALSE;
    
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetMuted", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    if (res) {
        gboolean muted;
        g_variant_get(res, "(b)", &muted);
        g_variant_unref(res);
        return muted;
    }
    return FALSE;
}

void audio_set_muted(gboolean muted) {
    ensure_proxy();
    if (!_proxy) return;
    g_dbus_proxy_call(_proxy, "SetMuted", g_variant_new("(b)", muted), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

/* Limits */
gint audio_get_max_volume(void) {
    ensure_proxy();
    if (!_proxy) return 100;
    
    GVariant *res = g_dbus_proxy_call_sync(_proxy, "GetMaxVolume", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    if (res) {
        gint max;
        g_variant_get(res, "(i)", &max);
        g_variant_unref(res);
        return max;
    }
    return 100;
}

/* Callbacks */
void audio_client_on_volume_changed(AudioVolumeCallback cb, gpointer user_data) {
    _vol_cb = cb;
    _vol_user_data = user_data;
}

void audio_client_on_mute_changed(AudioMuteCallback cb, gpointer user_data) {
    _mute_cb = cb;
    _mute_user_data = user_data;
}
