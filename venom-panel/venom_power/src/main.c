#include "venom_power.h"
#include "dbus_service.h"
#include "battery.h"
#include "logind.h"
#include "idle.h"
#include "config.h"
#include "keyboard.h"
#include "input_monitor.h"
#include "power_profile.h"
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🎯 المتغيرات العامة
// ═══════════════════════════════════════════════════════════════════════════

PowerState power_state = {0};
GMainLoop *main_loop = NULL;

#define DAEMON_VERSION "2.1.0"

// ═══════════════════════════════════════════════════════════════════════════
// 🚀 تهيئة الخدمة
// ═══════════════════════════════════════════════════════════════════════════

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    (void)name; (void)user_data;
    
    GError *error = NULL;
    GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(dbus_get_introspection_xml(), &error);
    
    if (error) {
        fprintf(stderr, "Failed to parse introspection: %s\n", error->message);
        g_error_free(error);
        return;
    }
    
    // تسجيل الخدمة
    g_dbus_connection_register_object(connection, "/org/venom/Power",
                                      node_info->interfaces[0],
                                      &dbus_interface_vtable, NULL, NULL, NULL);
    
    power_state.session_conn = connection;
    
    printf("\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("⚡ Venom Power Daemon v%s\n", DAEMON_VERSION);
    printf("══════════════════════════════════════════════════════════════\n");
    
    // تحميل الإعدادات
    PowerConfig *cfg = config_get();
    
    // الاتصال بـ System Bus
    power_state.system_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!power_state.system_conn) {
        fprintf(stderr, "⚠️ Could not connect to system bus: %s\n", error->message);
        g_error_free(error);
    } else {
        battery_setup_monitoring();
        logind_setup_monitoring();
        power_profile_init();
    }
    
    // تهيئة أوقات الخمول من الإعدادات
    power_state.dim_timeout = power_state.on_battery ? 
        cfg->dim_timeout_battery : cfg->dim_timeout_ac;
    power_state.blank_timeout = power_state.on_battery ? 
        cfg->blank_timeout_battery : cfg->blank_timeout_ac;
    power_state.suspend_timeout = cfg->suspend_timeout_battery;
    
    // قراءة الحالة الأولية للبطارية
    gdouble pct;
    gboolean charging;
    gint64 tte;
    battery_get_info(&pct, &charging, &tte);
    power_state.battery_percentage = pct;
    power_state.on_battery = !charging;
    
    // طباعة معلومات البدء
    printf("📁 Config: %s\n", cfg->lock_screen_cmd ? "Loaded" : "Defaults");
    printf("🔋 Battery: %.0f%% (%s)\n", pct, charging ? "Charging" : "Discharging");
    printf("⏰ Idle: dim=%us, blank=%us, suspend=%us\n",
           power_state.dim_timeout, power_state.blank_timeout, power_state.suspend_timeout);
    
    // فحص دعم إضاءة الكيبورد
    if (keyboard_is_supported()) {
        printf("⌨️ Keyboard backlight: Supported (level=%d)\n", keyboard_get_brightness());
    } else {
        printf("⌨️ Keyboard backlight: Not available\n");
    }
    
    printf("══════════════════════════════════════════════════════════════\n\n");
    
    // بدء مؤقتات الخمول
    idle_reset_timers();
    
    g_dbus_node_info_unref(node_info);
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    (void)connection; (void)user_data;
    fprintf(stderr, "❌ Lost D-Bus name: %s\n", name);
    g_main_loop_quit(main_loop);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎯 نقطة الدخول الرئيسية
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("🚀 Starting Venom Power Daemon...\n");
    
    g_bus_own_name(G_BUS_TYPE_SESSION, "org.venom.Power", G_BUS_NAME_OWNER_FLAGS_NONE,
                   on_bus_acquired, NULL, on_name_lost, NULL, NULL);
    
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    
    // تنظيف
    idle_cancel_timers();
    input_monitor_stop();
    power_profile_cleanup();
    config_free();
    if (power_state.system_conn) g_object_unref(power_state.system_conn);
    
    return 0;
}
