#include "audio.h"
#include <gio/gio.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 📄 D-Bus XML
// ═══════════════════════════════════════════════════════════════════════════

static const gchar* dbus_xml =
    "<!DOCTYPE node PUBLIC '-//freedesktop//DTD D-BUS Object Introspection 1.0//EN' "
    "'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd'>"
    "<node>"
    "  <interface name='org.venom.Audio'>"
    // Volume
    "    <method name='GetVolume'><arg type='i' direction='out'/></method>"
    "    <method name='SetVolume'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='GetMuted'><arg type='b' direction='out'/></method>"
    "    <method name='SetMuted'><arg type='b' direction='in'/><arg type='b' direction='out'/></method>"
    // Microphone
    "    <method name='GetMicVolume'><arg type='i' direction='out'/></method>"
    "    <method name='SetMicVolume'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='GetMicMuted'><arg type='b' direction='out'/></method>"
    "    <method name='SetMicMuted'><arg type='b' direction='in'/><arg type='b' direction='out'/></method>"
    // Sinks
    "    <method name='GetSinks'><arg type='a(ssib)' direction='out'/></method>"
    "    <method name='SetDefaultSink'><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='SetSinkVolume'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    // Sources
    "    <method name='GetSources'><arg type='a(ssib)' direction='out'/></method>"
    "    <method name='SetDefaultSource'><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='SetSourceVolume'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    // App Streams
    "    <method name='GetAppStreams'><arg type='a(ussib)' direction='out'/></method>"
    "    <method name='SetAppVolume'><arg type='u' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='SetAppMuted'><arg type='u' direction='in'/><arg type='b' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='MoveAppToSink'><arg type='u' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
    // Cards & Profiles
    "    <method name='GetCards'><arg type='a(ss)' direction='out'/></method>"
    "    <method name='GetProfiles'><arg type='s' direction='in'/><arg type='a(ssb)' direction='out'/></method>"
    "    <method name='SetProfile'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
    // Over-amplification
    "    <method name='GetOveramplification'><arg type='b' direction='out'/></method>"
    "    <method name='SetOveramplification'><arg type='b' direction='in'/><arg type='b' direction='out'/></method>"
    "    <method name='GetMaxVolume'><arg type='i' direction='out'/></method>"
    // Signals
    "    <signal name='VolumeChanged'><arg type='i'/></signal>"
    "    <signal name='MuteChanged'><arg type='b'/></signal>"
    "    <signal name='DevicesChanged'/>"
    "    <signal name='AppsChanged'/>"
    "  </interface>"
    "</node>";

// ═══════════════════════════════════════════════════════════════════════════
// 🔌 D-Bus State
// ═══════════════════════════════════════════════════════════════════════════

static GDBusConnection *dbus_conn = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// 📡 Signal Emission
// ═══════════════════════════════════════════════════════════════════════════

void dbus_emit_volume_changed(gint volume) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/Audio",
        "org.venom.Audio", "VolumeChanged", g_variant_new("(i)", volume), NULL);
}

void dbus_emit_mute_changed(gboolean muted) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/Audio",
        "org.venom.Audio", "MuteChanged", g_variant_new("(b)", muted), NULL);
}

void dbus_emit_devices_changed(void) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/Audio",
        "org.venom.Audio", "DevicesChanged", NULL, NULL);
}

void dbus_emit_apps_changed(void) {
    if (!dbus_conn) return;
    g_dbus_connection_emit_signal(dbus_conn, NULL, "/org/venom/Audio",
        "org.venom.Audio", "AppsChanged", NULL, NULL);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎛️ Method Handler
// ═══════════════════════════════════════════════════════════════════════════

static void handle_method(GDBusConnection *conn, const gchar *sender,
    const gchar *path, const gchar *interface, const gchar *method,
    GVariant *params, GDBusMethodInvocation *invoc, gpointer data) {
    (void)conn; (void)sender; (void)path; (void)interface; (void)data;
    
    printf("🔊 Audio: %s\n", method);
    
    // ═══════════════════════════════════════════════════════════════════════
    // Volume
    // ═══════════════════════════════════════════════════════════════════════
    if (g_strcmp0(method, "GetVolume") == 0) {
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", audio_get_volume()));
    }
    else if (g_strcmp0(method, "SetVolume") == 0) {
        gint vol;
        g_variant_get(params, "(i)", &vol);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_volume(vol)));
    }
    else if (g_strcmp0(method, "GetMuted") == 0) {
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_get_muted()));
    }
    else if (g_strcmp0(method, "SetMuted") == 0) {
        gboolean muted;
        g_variant_get(params, "(b)", &muted);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_muted(muted)));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // Microphone
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method, "GetMicVolume") == 0) {
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", audio_get_mic_volume()));
    }
    else if (g_strcmp0(method, "SetMicVolume") == 0) {
        gint vol;
        g_variant_get(params, "(i)", &vol);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_mic_volume(vol)));
    }
    else if (g_strcmp0(method, "GetMicMuted") == 0) {
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_get_mic_muted()));
    }
    else if (g_strcmp0(method, "SetMicMuted") == 0) {
        gboolean muted;
        g_variant_get(params, "(b)", &muted);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_mic_muted(muted)));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // Sinks
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method, "GetSinks") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ssib)"));
        GList *sinks = audio_get_sinks();
        for (GList *l = sinks; l; l = l->next) {
            AudioDevice *d = l->data;
            g_variant_builder_add(&b, "(ssib)", d->name ? d->name : "",
                d->description ? d->description : "", d->volume, d->is_default);
            audio_device_free(d);
        }
        g_list_free(sinks);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(ssib))", &b));
    }
    else if (g_strcmp0(method, "SetDefaultSink") == 0) {
        const gchar *name;
        g_variant_get(params, "(&s)", &name);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_default_sink(name)));
    }
    else if (g_strcmp0(method, "SetSinkVolume") == 0) {
        const gchar *name; gint vol;
        g_variant_get(params, "(&si)", &name, &vol);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_sink_volume(name, vol)));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // Sources
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method, "GetSources") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ssib)"));
        GList *sources = audio_get_sources();
        for (GList *l = sources; l; l = l->next) {
            AudioDevice *d = l->data;
            g_variant_builder_add(&b, "(ssib)", d->name ? d->name : "",
                d->description ? d->description : "", d->volume, d->is_default);
            audio_device_free(d);
        }
        g_list_free(sources);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(ssib))", &b));
    }
    else if (g_strcmp0(method, "SetDefaultSource") == 0) {
        const gchar *name;
        g_variant_get(params, "(&s)", &name);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_default_source(name)));
    }
    else if (g_strcmp0(method, "SetSourceVolume") == 0) {
        const gchar *name; gint vol;
        g_variant_get(params, "(&si)", &name, &vol);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_source_volume(name, vol)));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // App Streams
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method, "GetAppStreams") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ussib)"));
        GList *apps = audio_get_app_streams();
        for (GList *l = apps; l; l = l->next) {
            AppStream *a = l->data;
            g_variant_builder_add(&b, "(ussib)", a->index, a->name ? a->name : "",
                a->icon ? a->icon : "", a->volume, a->muted);
            audio_app_stream_free(a);
        }
        g_list_free(apps);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(ussib))", &b));
    }
    else if (g_strcmp0(method, "SetAppVolume") == 0) {
        guint32 idx; gint vol;
        g_variant_get(params, "(ui)", &idx, &vol);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_app_volume(idx, vol)));
    }
    else if (g_strcmp0(method, "SetAppMuted") == 0) {
        guint32 idx; gboolean muted;
        g_variant_get(params, "(ub)", &idx, &muted);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_app_muted(idx, muted)));
    }
    else if (g_strcmp0(method, "MoveAppToSink") == 0) {
        guint32 idx; const gchar *sink;
        g_variant_get(params, "(u&s)", &idx, &sink);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_move_app_to_sink(idx, sink)));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // Cards & Profiles
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method, "GetCards") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ss)"));
        GList *cards = audio_get_cards();
        for (GList *l = cards; l; l = l->next) {
            AudioDevice *c = l->data;
            g_variant_builder_add(&b, "(ss)", c->name ? c->name : "", c->description ? c->description : "");
            audio_device_free(c);
        }
        g_list_free(cards);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(ss))", &b));
    }
    else if (g_strcmp0(method, "GetProfiles") == 0) {
        const gchar *card;
        g_variant_get(params, "(&s)", &card);
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ssb)"));
        GList *profiles = audio_get_profiles(card);
        for (GList *l = profiles; l; l = l->next) {
            AudioProfile *p = l->data;
            g_variant_builder_add(&b, "(ssb)", p->name ? p->name : "", p->description ? p->description : "", p->available);
            audio_profile_free(p);
        }
        g_list_free(profiles);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(a(ssb))", &b));
    }
    else if (g_strcmp0(method, "SetProfile") == 0) {
        const gchar *card, *profile;
        g_variant_get(params, "(&s&s)", &card, &profile);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_profile(card, profile)));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // Over-amplification
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method, "GetOveramplification") == 0) {
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_get_overamplification()));
    }
    else if (g_strcmp0(method, "SetOveramplification") == 0) {
        gboolean enabled;
        g_variant_get(params, "(b)", &enabled);
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", audio_set_overamplification(enabled)));
    }
    else if (g_strcmp0(method, "GetMaxVolume") == 0) {
        g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", audio_state.overamplification ? 150 : 100));
    }
    else {
        g_dbus_method_invocation_return_error(invoc, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
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

