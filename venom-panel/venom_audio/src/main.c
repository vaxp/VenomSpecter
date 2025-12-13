#include "audio.h"
#include <gio/gio.h>
#include <stdio.h>
#include <signal.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔌 External Functions (from dbus_service.c)
// ═══════════════════════════════════════════════════════════════════════════

extern const gchar* dbus_get_xml(void);
extern void dbus_set_connection(GDBusConnection *conn);
extern const GDBusInterfaceVTable* dbus_get_vtable(void);
extern void dbus_emit_volume_changed(gint volume);
extern void dbus_emit_mute_changed(gboolean muted);
extern void dbus_emit_devices_changed(void);

// ═══════════════════════════════════════════════════════════════════════════
// 🌐 Global State
// ═══════════════════════════════════════════════════════════════════════════

static GMainLoop *main_loop = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// 📡 Callbacks
// ═══════════════════════════════════════════════════════════════════════════

static void on_volume_changed(gint volume) {
    dbus_emit_volume_changed(volume);
}

static void on_mute_changed(gboolean muted) {
    dbus_emit_mute_changed(muted);
}

static void on_devices_changed(void) {
    dbus_emit_devices_changed();
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔌 D-Bus Callbacks
// ═══════════════════════════════════════════════════════════════════════════

static void on_bus_acquired(GDBusConnection *conn, const gchar *name, gpointer data) {
    (void)name; (void)data;
    printf("📡 D-Bus connection acquired\n");
    
    dbus_set_connection(conn);
    
    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(dbus_get_xml(), NULL);
    if (!node) {
        printf("❌ Failed to parse D-Bus XML\n");
        return;
    }
    
    GError *error = NULL;
    g_dbus_connection_register_object(conn, "/org/venom/Audio",
        node->interfaces[0], dbus_get_vtable(), NULL, NULL, &error);
    
    if (error) {
        printf("❌ Failed to register: %s\n", error->message);
        g_error_free(error);
    } else {
        printf("🔊 Audio interface registered\n");
    }
    
    g_dbus_node_info_unref(node);
}

static void on_name_acquired(GDBusConnection *conn, const gchar *name, gpointer data) {
    (void)conn; (void)data;
    printf("✅ D-Bus name acquired: %s\n", name);
}

static void on_name_lost(GDBusConnection *conn, const gchar *name, gpointer data) {
    (void)conn; (void)name; (void)data;
    printf("❌ D-Bus name lost\n");
    g_main_loop_quit(main_loop);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🛑 Signal Handler
// ═══════════════════════════════════════════════════════════════════════════

static void signal_handler(int sig) {
    (void)sig;
    printf("\n🛑 Shutting down...\n");
    if (main_loop) g_main_loop_quit(main_loop);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🚀 Main
// ═══════════════════════════════════════════════════════════════════════════

int main(void) {
    printf("🔊 ═══════════════════════════════════════════════════════════════\n");
    printf("🔊 Venom Audio Daemon v1.0\n");
    printf("🔊 ═══════════════════════════════════════════════════════════════\n");
    
    // Signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize audio
    if (!audio_init()) {
        printf("❌ Failed to initialize audio\n");
        return 1;
    }
    
    // Set callbacks
    audio_state.on_volume_changed = on_volume_changed;
    audio_state.on_mute_changed = on_mute_changed;
    audio_state.on_devices_changed = on_devices_changed;
    
    // Start main loop
    main_loop = g_main_loop_new(NULL, FALSE);
    
    printf("🚀 Starting main loop...\n");
    
    guint owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
        "org.venom.Audio",
        G_BUS_NAME_OWNER_FLAGS_REPLACE,
        on_bus_acquired,
        on_name_acquired,
        on_name_lost,
        NULL, NULL);
    
    g_main_loop_run(main_loop);
    
    // Cleanup
    g_bus_unown_name(owner_id);
    g_main_loop_unref(main_loop);
    audio_cleanup();
    
    printf("👋 Goodbye!\n");
    return 0;
}
