#include "dbus_service.h"
#include "venom_power.h"
#include "backlight.h"
#include "battery.h"
#include "idle.h"
#include "inhibitor.h"
#include "logind.h"
#include "keyboard.h"
#include "config.h"
#include "power_profile.h"
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════
// 📡 خدمة D-Bus - واجهة API كاملة
// ═══════════════════════════════════════════════════════════════════════════

static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.venom.Power'>"
// ═══════════════════════════════════════════════════════════════════════════
// أوامر الطاقة الأساسية
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='Shutdown'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='Reboot'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='Suspend'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='Hibernate'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='Logout'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='LockScreen'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// التحكم بسطوع الشاشة
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetBrightness'>"
"      <arg type='i' name='level' direction='out'/>"
"    </method>"
"    <method name='SetBrightness'>"
"      <arg type='i' name='level' direction='in'/>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='GetMaxBrightness'>"
"      <arg type='i' name='max_level' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// التحكم بإضاءة الكيبورد
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetKeyboardBrightness'>"
"      <arg type='i' name='level' direction='out'/>"
"    </method>"
"    <method name='SetKeyboardBrightness'>"
"      <arg type='i' name='level' direction='in'/>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='GetKeyboardMaxBrightness'>"
"      <arg type='i' name='max_level' direction='out'/>"
"    </method>"
"    <method name='IsKeyboardBacklightSupported'>"
"      <arg type='b' name='supported' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// معلومات البطارية
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetBatteryInfo'>"
"      <arg type='d' name='percentage' direction='out'/>"
"      <arg type='b' name='charging' direction='out'/>"
"      <arg type='x' name='time_to_empty' direction='out'/>"
"    </method>"
"    <method name='GetPowerSource'>"
"      <arg type='b' name='on_battery' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// حالة الأجهزة
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetLidState'>"
"      <arg type='b' name='closed' direction='out'/>"
"    </method>"
"    <method name='GetIdleState'>"
"      <arg type='b' name='is_idle' direction='out'/>"
"      <arg type='b' name='screen_dimmed' direction='out'/>"
"      <arg type='b' name='screen_blanked' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// نظام المنع (Inhibitors)
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='Inhibit'>"
"      <arg type='s' name='what' direction='in'/>"
"      <arg type='s' name='who' direction='in'/>"
"      <arg type='s' name='why' direction='in'/>"
"      <arg type='u' name='cookie' direction='out'/>"
"    </method>"
"    <method name='UnInhibit'>"
"      <arg type='u' name='cookie' direction='in'/>"
"    </method>"
"    <method name='ListInhibitors'>"
"      <arg type='a(uss)' name='inhibitors' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// إعدادات الخمول
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='SetIdleTimeouts'>"
"      <arg type='u' name='dim' direction='in'/>"
"      <arg type='u' name='blank' direction='in'/>"
"      <arg type='u' name='suspend' direction='in'/>"
"    </method>"
"    <method name='GetIdleTimeouts'>"
"      <arg type='u' name='dim' direction='out'/>"
"      <arg type='u' name='blank' direction='out'/>"
"      <arg type='u' name='suspend' direction='out'/>"
"    </method>"
"    <method name='SimulateActivity'/>"
// ═══════════════════════════════════════════════════════════════════════════
// إعدادات البطارية
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetBatteryLevels'>"
"      <arg type='u' name='low' direction='out'/>"
"      <arg type='u' name='critical' direction='out'/>"
"      <arg type='u' name='danger' direction='out'/>"
"    </method>"
"    <method name='SetBatteryLevels'>"
"      <arg type='u' name='low' direction='in'/>"
"      <arg type='u' name='critical' direction='in'/>"
"      <arg type='u' name='danger' direction='in'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// إعدادات الإجراءات
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetLidAction'>"
"      <arg type='s' name='action_ac' direction='out'/>"
"      <arg type='s' name='action_battery' direction='out'/>"
"    </method>"
"    <method name='SetLidAction'>"
"      <arg type='s' name='action_ac' direction='in'/>"
"      <arg type='s' name='action_battery' direction='in'/>"
"    </method>"
"    <method name='GetPowerButtonAction'>"
"      <arg type='s' name='action' direction='out'/>"
"    </method>"
"    <method name='SetPowerButtonAction'>"
"      <arg type='s' name='action' direction='in'/>"
"    </method>"
"    <method name='GetCriticalAction'>"
"      <arg type='s' name='action' direction='out'/>"
"    </method>"
"    <method name='SetCriticalAction'>"
"      <arg type='s' name='action' direction='in'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// إدارة الإعدادات
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='SaveConfig'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='ReloadConfig'>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='ResetConfig'/>"
// ═══════════════════════════════════════════════════════════════════════════
// معلومات العفريت
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetVersion'>"
"      <arg type='s' name='version' direction='out'/>"
"    </method>"
"    <method name='GetCapabilities'>"
"      <arg type='as' name='capabilities' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// بروفايلات الطاقة
// ═══════════════════════════════════════════════════════════════════════════
"    <method name='GetActiveProfile'>"
"      <arg type='s' name='profile' direction='out'/>"
"    </method>"
"    <method name='SetActiveProfile'>"
"      <arg type='s' name='profile' direction='in'/>"
"      <arg type='b' name='success' direction='out'/>"
"    </method>"
"    <method name='GetProfiles'>"
"      <arg type='as' name='profiles' direction='out'/>"
"    </method>"
"    <method name='GetPerformanceInhibited'>"
"      <arg type='s' name='reason' direction='out'/>"
"    </method>"
"    <method name='IsProfilesAvailable'>"
"      <arg type='b' name='available' direction='out'/>"
"    </method>"
// ═══════════════════════════════════════════════════════════════════════════
// الإشارات (Signals)
// ═══════════════════════════════════════════════════════════════════════════
"    <signal name='BatteryWarning'>"
"      <arg type='d' name='percentage'/>"
"    </signal>"
"    <signal name='BatteryCritical'>"
"      <arg type='d' name='percentage'/>"
"    </signal>"
"    <signal name='BatteryChanged'>"
"      <arg type='d' name='percentage'/>"
"      <arg type='b' name='charging'/>"
"    </signal>"
"    <signal name='LidStateChanged'>"
"      <arg type='b' name='closed'/>"
"    </signal>"
"    <signal name='PowerSourceChanged'>"
"      <arg type='b' name='on_battery'/>"
"    </signal>"
"    <signal name='ScreenDimmed'>"
"      <arg type='b' name='dimmed'/>"
"    </signal>"
"    <signal name='ScreenBlanked'>"
"      <arg type='b' name='blanked'/>"
"    </signal>"
"    <signal name='BrightnessChanged'>"
"      <arg type='i' name='level'/>"
"    </signal>"
"    <signal name='KeyboardBrightnessChanged'>"
"      <arg type='i' name='level'/>"
"    </signal>"
"    <signal name='IdleStateChanged'>"
"      <arg type='b' name='is_idle'/>"
"    </signal>"
"    <signal name='IdleTimeoutsChanged'>"
"      <arg type='u' name='dim'/>"
"      <arg type='u' name='blank'/>"
"      <arg type='u' name='suspend'/>"
"    </signal>"
"    <signal name='ConfigChanged'/>"
"    <signal name='ProfileChanged'>"
"      <arg type='s' name='profile'/>"
"    </signal>"
"  </interface>"
"</node>";

#define DAEMON_VERSION "2.1.0"

const gchar* dbus_get_introspection_xml(void) {
    return introspection_xml;
}

void dbus_emit_signal(const gchar *signal_name, GVariant *params) {
    if (power_state.session_conn) {
        g_dbus_connection_emit_signal(
            power_state.session_conn,
            NULL,
            "/org/venom/Power",
            "org.venom.Power",
            signal_name,
            params,
            NULL
        );
    }
}

void dbus_send_notification(const gchar *title, const gchar *body, const gchar *urgency) {
    gchar *cmd = g_strdup_printf("notify-send -u %s '%s' '%s' 2>/dev/null", 
                                  urgency, title, body);
    system(cmd);
    g_free(cmd);
}

void dbus_handle_method_call(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *method_name, GVariant *parameters,
                             GDBusMethodInvocation *invocation, gpointer user_data) {
    
    (void)connection; (void)sender; (void)object_path; 
    (void)interface_name; (void)user_data;
    
    PowerConfig *cfg = config_get();
    
    // ═══════════════════════════════════════════════════════════════════════
    // أوامر الطاقة الأساسية
    // ═══════════════════════════════════════════════════════════════════════
    if (g_strcmp0(method_name, "Shutdown") == 0) {
        gboolean success = logind_call("PowerOff", g_variant_new("(b)", TRUE));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "Reboot") == 0) {
        gboolean success = logind_call("Reboot", g_variant_new("(b)", TRUE));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "Suspend") == 0) {
        if (cfg->lock_on_suspend) logind_lock_screen();
        gboolean success = logind_call("Suspend", g_variant_new("(b)", TRUE));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "Hibernate") == 0) {
        if (cfg->lock_on_suspend) logind_lock_screen();
        gboolean success = logind_call("Hibernate", g_variant_new("(b)", TRUE));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "Logout") == 0) {
        gboolean success = logind_logout();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "LockScreen") == 0) {
        gboolean success = logind_lock_screen();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // التحكم بسطوع الشاشة
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetBrightness") == 0) {
        gint level = backlight_get_brightness();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", level));
    }
    else if (g_strcmp0(method_name, "SetBrightness") == 0) {
        gint level;
        g_variant_get(parameters, "(i)", &level);
        gboolean success = backlight_set_brightness(level);
        if (success) dbus_emit_signal("BrightnessChanged", g_variant_new("(i)", level));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "GetMaxBrightness") == 0) {
        gint max = backlight_get_max_brightness();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", max));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // التحكم بإضاءة الكيبورد
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetKeyboardBrightness") == 0) {
        gint level = keyboard_get_brightness();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", level));
    }
    else if (g_strcmp0(method_name, "SetKeyboardBrightness") == 0) {
        gint level;
        g_variant_get(parameters, "(i)", &level);
        gboolean success = keyboard_set_brightness(level);
        if (success) dbus_emit_signal("KeyboardBrightnessChanged", g_variant_new("(i)", level));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "GetKeyboardMaxBrightness") == 0) {
        gint max = keyboard_get_max_brightness();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", max));
    }
    else if (g_strcmp0(method_name, "IsKeyboardBacklightSupported") == 0) {
        gboolean supported = keyboard_is_supported();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", supported));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // معلومات البطارية
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetBatteryInfo") == 0) {
        gdouble percentage;
        gboolean charging;
        gint64 time_to_empty;
        battery_get_info(&percentage, &charging, &time_to_empty);
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(dbx)", percentage, charging, time_to_empty));
    }
    else if (g_strcmp0(method_name, "GetPowerSource") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(b)", power_state.on_battery));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // حالة الأجهزة
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetLidState") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(b)", power_state.lid_closed));
    }
    else if (g_strcmp0(method_name, "GetIdleState") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(bbb)", power_state.is_idle, 
                          power_state.screen_dimmed, power_state.screen_blanked));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // نظام المنع
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "Inhibit") == 0) {
        const gchar *what, *who, *why;
        g_variant_get(parameters, "(&s&s&s)", &what, &who, &why);
        guint cookie = inhibitor_add(what, who, why);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", cookie));
    }
    else if (g_strcmp0(method_name, "UnInhibit") == 0) {
        guint cookie;
        g_variant_get(parameters, "(u)", &cookie);
        inhibitor_remove(cookie);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0(method_name, "ListInhibitors") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(uss)"));
        inhibitor_list_to_variant(&builder);
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(a(uss))", &builder));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // إعدادات الخمول
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "SetIdleTimeouts") == 0) {
        guint dim, blank, suspend;
        g_variant_get(parameters, "(uuu)", &dim, &blank, &suspend);
        idle_set_timeouts(dim, blank, suspend);
        printf("📨 D-Bus: SetIdleTimeouts received (dim=%u, blank=%u, suspend=%u)\n", dim, blank, suspend);
        dbus_emit_signal("IdleTimeoutsChanged", g_variant_new("(uuu)", dim, blank, suspend));
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0(method_name, "GetIdleTimeouts") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(uuu)", power_state.dim_timeout, 
                          power_state.blank_timeout, power_state.suspend_timeout));
    }
    else if (g_strcmp0(method_name, "SimulateActivity") == 0) {
        idle_reset_timers();
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    // ═══════════════════════════════════════════════════════════════════════
    // إعدادات البطارية
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetBatteryLevels") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(uuu)", cfg->battery_low_level, 
                          cfg->battery_critical_level, cfg->battery_danger_level));
    }
    else if (g_strcmp0(method_name, "SetBatteryLevels") == 0) {
        g_variant_get(parameters, "(uuu)", &cfg->battery_low_level,
                      &cfg->battery_critical_level, &cfg->battery_danger_level);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    // ═══════════════════════════════════════════════════════════════════════
    // إعدادات الإجراءات
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetLidAction") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(ss)", cfg->lid_action_ac, cfg->lid_action_battery));
    }
    else if (g_strcmp0(method_name, "SetLidAction") == 0) {
        const gchar *ac, *battery;
        g_variant_get(parameters, "(&s&s)", &ac, &battery);
        g_free(cfg->lid_action_ac);
        g_free(cfg->lid_action_battery);
        cfg->lid_action_ac = g_strdup(ac);
        cfg->lid_action_battery = g_strdup(battery);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0(method_name, "GetPowerButtonAction") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(s)", cfg->power_button_action));
    }
    else if (g_strcmp0(method_name, "SetPowerButtonAction") == 0) {
        const gchar *action;
        g_variant_get(parameters, "(&s)", &action);
        g_free(cfg->power_button_action);
        cfg->power_button_action = g_strdup(action);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if (g_strcmp0(method_name, "GetCriticalAction") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(s)", cfg->critical_action));
    }
    else if (g_strcmp0(method_name, "SetCriticalAction") == 0) {
        const gchar *action;
        g_variant_get(parameters, "(&s)", &action);
        g_free(cfg->critical_action);
        cfg->critical_action = g_strdup(action);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    // ═══════════════════════════════════════════════════════════════════════
    // إدارة الإعدادات
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "SaveConfig") == 0) {
        gboolean success = config_save();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "ReloadConfig") == 0) {
        gboolean success = config_load();
        dbus_emit_signal("ConfigChanged", NULL);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "ResetConfig") == 0) {
        config_reset_defaults();
        dbus_emit_signal("ConfigChanged", NULL);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    // ═══════════════════════════════════════════════════════════════════════
    // معلومات العفريت
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetVersion") == 0) {
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(s)", DAEMON_VERSION));
    }
    else if (g_strcmp0(method_name, "GetCapabilities") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        g_variant_builder_add(&builder, "s", "power-control");
        g_variant_builder_add(&builder, "s", "screen-brightness");
        if (keyboard_is_supported()) 
            g_variant_builder_add(&builder, "s", "keyboard-backlight");
        g_variant_builder_add(&builder, "s", "battery-monitor");
        g_variant_builder_add(&builder, "s", "lid-monitor");
        g_variant_builder_add(&builder, "s", "idle-management");
        g_variant_builder_add(&builder, "s", "inhibitors");
        g_variant_builder_add(&builder, "s", "config-management");
        if (power_profile_is_available())
            g_variant_builder_add(&builder, "s", "power-profiles");
        g_dbus_method_invocation_return_value(invocation, 
            g_variant_new("(as)", &builder));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // بروفايلات الطاقة
    // ═══════════════════════════════════════════════════════════════════════
    else if (g_strcmp0(method_name, "GetActiveProfile") == 0) {
        const gchar *profile = power_profile_get_active();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", profile));
    }
    else if (g_strcmp0(method_name, "SetActiveProfile") == 0) {
        const gchar *profile;
        g_variant_get(parameters, "(&s)", &profile);
        gboolean success = power_profile_set_active(profile);
        if (success) {
            dbus_emit_signal("ProfileChanged", g_variant_new("(s)", profile));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
    }
    else if (g_strcmp0(method_name, "GetProfiles") == 0) {
        GVariant *profiles = power_profile_get_list();
        g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&profiles, 1));
    }
    else if (g_strcmp0(method_name, "GetPerformanceInhibited") == 0) {
        const gchar *reason = power_profile_get_inhibited_reason();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", reason));
    }
    else if (g_strcmp0(method_name, "IsProfilesAvailable") == 0) {
        gboolean available = power_profile_is_available();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", available));
    }
    // ═══════════════════════════════════════════════════════════════════════
    // خطأ: طريقة غير معروفة
    // ═══════════════════════════════════════════════════════════════════════
    else {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, 
            G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method: %s", method_name);
    }
}

const GDBusInterfaceVTable dbus_interface_vtable = { dbus_handle_method_call, NULL, NULL };
