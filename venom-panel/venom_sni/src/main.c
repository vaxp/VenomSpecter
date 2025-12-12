#include "sni.h"
#include <stdio.h>
#include <signal.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔌 External Functions
// ═══════════════════════════════════════════════════════════════════════════

extern const gchar* dbus_get_xml(void);
extern void dbus_set_connection(GDBusConnection *conn);
extern const GDBusInterfaceVTable* dbus_get_vtable(void);
extern void dbus_emit_item_added(const gchar *id);
extern void dbus_emit_item_removed(const gchar *id);
extern void dbus_emit_item_changed(const gchar *id);

// ═══════════════════════════════════════════════════════════════════════════
// 🌐 Global State
// ═══════════════════════════════════════════════════════════════════════════

static GMainLoop *main_loop = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// 📡 Callbacks
// ═══════════════════════════════════════════════════════════════════════════

static void on_item_added(const gchar *id) {
    dbus_emit_item_added(id);
}

static void on_item_removed(const gchar *id) {
    dbus_emit_item_removed(id);
}

static void on_item_changed(const gchar *id) {
    dbus_emit_item_changed(id);
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
    g_dbus_connection_register_object(conn, "/org/venom/SNI",
        node->interfaces[0], dbus_get_vtable(), NULL, NULL, &error);
    
    if (error) {
        printf("❌ Failed to register: %s\n", error->message);
        g_error_free(error);
    } else {
        printf("🔧 SNI interface registered\n");
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
    printf("🔧 ═══════════════════════════════════════════════════════════════\n");
    printf("🔧 Venom SNI Daemon v1.0\n");
    printf("🔧 Status Notifier Items (System Tray)\n");
    printf("🔧 ═══════════════════════════════════════════════════════════════\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize SNI
    if (!sni_init()) {
        printf("❌ Failed to initialize SNI\n");
        return 1;
    }
    
    // Set callbacks
    sni_state.on_item_added = on_item_added;
    sni_state.on_item_removed = on_item_removed;
    sni_state.on_item_changed = on_item_changed;
    
    // Start main loop
    main_loop = g_main_loop_new(NULL, FALSE);
    
    printf("🚀 Starting main loop...\n");
    
    guint owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
        "org.venom.SNI",
        G_BUS_NAME_OWNER_FLAGS_REPLACE,
        on_bus_acquired,
        on_name_acquired,
        on_name_lost,
        NULL, NULL);
    
    g_main_loop_run(main_loop);
    
    // Cleanup
    g_bus_unown_name(owner_id);
    g_main_loop_unref(main_loop);
    sni_cleanup();
    
    printf("👋 Goodbye!\n");
    return 0;
}
