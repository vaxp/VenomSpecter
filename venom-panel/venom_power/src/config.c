#include "config.h"
#include "venom_power.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// ⚙️ إدارة الإعدادات
// ═══════════════════════════════════════════════════════════════════════════

static PowerConfig config = {0};
static gboolean config_loaded = FALSE;

void config_reset_defaults(void) {
    // إعدادات الخمول
    config.dim_timeout_ac = 300;           // 5 دقائق على الكهرباء
    config.dim_timeout_battery = 120;      // 2 دقيقة على البطارية
    config.blank_timeout_ac = 600;         // 10 دقائق
    config.blank_timeout_battery = 300;    // 5 دقائق
    config.suspend_timeout_battery = 900;  // 15 دقيقة
    
    // مستويات البطارية
    config.battery_low_level = 10;
    config.battery_critical_level = 5;
    config.battery_danger_level = 2;
    
    // إجراءات
    g_free(config.lid_action_ac);
    g_free(config.lid_action_battery);
    g_free(config.power_button_action);
    g_free(config.critical_action);
    g_free(config.lock_screen_cmd);
    
    config.lid_action_ac = g_strdup("lock");
    config.lid_action_battery = g_strdup("suspend");
    config.power_button_action = g_strdup("interactive");
    config.critical_action = g_strdup("hibernate");
    
    // سطوع
    config.brightness_ac = -1;             // -1 = لا تغيير
    config.brightness_battery = -1;
    config.dim_on_idle = TRUE;
    config.dim_level_percent = 30;
    
    // إضاءة الكيبورد
    config.kbd_backlight_auto_off = TRUE;
    config.kbd_backlight_timeout = 30;     // 30 ثانية
    
    // متنوع
    config.lock_on_suspend = TRUE;
    config.lock_on_lid = TRUE;
    config.lock_screen_cmd = g_strdup(LOCK_SCREEN_CMD);
    
    printf("⚙️ Config reset to defaults\n");
}

PowerConfig* config_get(void) {
    if (!config_loaded) {
        config_load();
    }
    return &config;
}

gboolean config_load(void) {
    // تعيين القيم الافتراضية أولاً
    config_reset_defaults();
    
    // محاولة قراءة ملف المستخدم أولاً
    gchar *user_path = g_build_filename(g_get_home_dir(), ".config", "venom", "power.conf", NULL);
    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;
    
    gboolean loaded = g_key_file_load_from_file(keyfile, user_path, G_KEY_FILE_NONE, &error);
    
    if (!loaded) {
        g_clear_error(&error);
        // محاولة قراءة ملف النظام
        loaded = g_key_file_load_from_file(keyfile, CONFIG_PATH, G_KEY_FILE_NONE, &error);
    }
    
    if (loaded) {
        // قراءة إعدادات الخمول
        if (g_key_file_has_key(keyfile, "Idle", "DimTimeoutAC", NULL))
            config.dim_timeout_ac = g_key_file_get_integer(keyfile, "Idle", "DimTimeoutAC", NULL);
        if (g_key_file_has_key(keyfile, "Idle", "DimTimeoutBattery", NULL))
            config.dim_timeout_battery = g_key_file_get_integer(keyfile, "Idle", "DimTimeoutBattery", NULL);
        if (g_key_file_has_key(keyfile, "Idle", "BlankTimeoutAC", NULL))
            config.blank_timeout_ac = g_key_file_get_integer(keyfile, "Idle", "BlankTimeoutAC", NULL);
        if (g_key_file_has_key(keyfile, "Idle", "BlankTimeoutBattery", NULL))
            config.blank_timeout_battery = g_key_file_get_integer(keyfile, "Idle", "BlankTimeoutBattery", NULL);
        if (g_key_file_has_key(keyfile, "Idle", "SuspendTimeoutBattery", NULL))
            config.suspend_timeout_battery = g_key_file_get_integer(keyfile, "Idle", "SuspendTimeoutBattery", NULL);
        
        // قراءة مستويات البطارية
        if (g_key_file_has_key(keyfile, "Battery", "LowLevel", NULL))
            config.battery_low_level = g_key_file_get_integer(keyfile, "Battery", "LowLevel", NULL);
        if (g_key_file_has_key(keyfile, "Battery", "CriticalLevel", NULL))
            config.battery_critical_level = g_key_file_get_integer(keyfile, "Battery", "CriticalLevel", NULL);
        if (g_key_file_has_key(keyfile, "Battery", "DangerLevel", NULL))
            config.battery_danger_level = g_key_file_get_integer(keyfile, "Battery", "DangerLevel", NULL);
        
        // قراءة الإجراءات
        if (g_key_file_has_key(keyfile, "Actions", "LidActionAC", NULL)) {
            g_free(config.lid_action_ac);
            config.lid_action_ac = g_key_file_get_string(keyfile, "Actions", "LidActionAC", NULL);
        }
        if (g_key_file_has_key(keyfile, "Actions", "LidActionBattery", NULL)) {
            g_free(config.lid_action_battery);
            config.lid_action_battery = g_key_file_get_string(keyfile, "Actions", "LidActionBattery", NULL);
        }
        if (g_key_file_has_key(keyfile, "Actions", "PowerButtonAction", NULL)) {
            g_free(config.power_button_action);
            config.power_button_action = g_key_file_get_string(keyfile, "Actions", "PowerButtonAction", NULL);
        }
        if (g_key_file_has_key(keyfile, "Actions", "CriticalAction", NULL)) {
            g_free(config.critical_action);
            config.critical_action = g_key_file_get_string(keyfile, "Actions", "CriticalAction", NULL);
        }
        
        // قراءة إعدادات السطوع
        if (g_key_file_has_key(keyfile, "Brightness", "AC", NULL))
            config.brightness_ac = g_key_file_get_integer(keyfile, "Brightness", "AC", NULL);
        if (g_key_file_has_key(keyfile, "Brightness", "Battery", NULL))
            config.brightness_battery = g_key_file_get_integer(keyfile, "Brightness", "Battery", NULL);
        if (g_key_file_has_key(keyfile, "Brightness", "DimOnIdle", NULL))
            config.dim_on_idle = g_key_file_get_boolean(keyfile, "Brightness", "DimOnIdle", NULL);
        if (g_key_file_has_key(keyfile, "Brightness", "DimLevelPercent", NULL))
            config.dim_level_percent = g_key_file_get_integer(keyfile, "Brightness", "DimLevelPercent", NULL);
        
        // قراءة إعدادات الكيبورد
        if (g_key_file_has_key(keyfile, "Keyboard", "AutoOff", NULL))
            config.kbd_backlight_auto_off = g_key_file_get_boolean(keyfile, "Keyboard", "AutoOff", NULL);
        if (g_key_file_has_key(keyfile, "Keyboard", "Timeout", NULL))
            config.kbd_backlight_timeout = g_key_file_get_integer(keyfile, "Keyboard", "Timeout", NULL);
        
        // قراءة إعدادات القفل
        if (g_key_file_has_key(keyfile, "Lock", "OnSuspend", NULL))
            config.lock_on_suspend = g_key_file_get_boolean(keyfile, "Lock", "OnSuspend", NULL);
        if (g_key_file_has_key(keyfile, "Lock", "OnLid", NULL))
            config.lock_on_lid = g_key_file_get_boolean(keyfile, "Lock", "OnLid", NULL);
        if (g_key_file_has_key(keyfile, "Lock", "Command", NULL)) {
            g_free(config.lock_screen_cmd);
            config.lock_screen_cmd = g_key_file_get_string(keyfile, "Lock", "Command", NULL);
        }
        
        printf("⚙️ Config loaded from file\n");
    } else {
        printf("⚙️ Using default config (no config file found)\n");
    }
    
    g_key_file_free(keyfile);
    g_free(user_path);
    config_loaded = TRUE;
    
    return loaded;
}

gboolean config_save(void) {
    gchar *dir_path = g_build_filename(g_get_home_dir(), ".config", "venom", NULL);
    gchar *file_path = g_build_filename(dir_path, "power.conf", NULL);
    
    // إنشاء المجلد إذا لم يكن موجوداً
    g_mkdir_with_parents(dir_path, 0755);
    
    GKeyFile *keyfile = g_key_file_new();
    
    // كتابة إعدادات الخمول
    g_key_file_set_integer(keyfile, "Idle", "DimTimeoutAC", config.dim_timeout_ac);
    g_key_file_set_integer(keyfile, "Idle", "DimTimeoutBattery", config.dim_timeout_battery);
    g_key_file_set_integer(keyfile, "Idle", "BlankTimeoutAC", config.blank_timeout_ac);
    g_key_file_set_integer(keyfile, "Idle", "BlankTimeoutBattery", config.blank_timeout_battery);
    g_key_file_set_integer(keyfile, "Idle", "SuspendTimeoutBattery", config.suspend_timeout_battery);
    
    // كتابة مستويات البطارية
    g_key_file_set_integer(keyfile, "Battery", "LowLevel", config.battery_low_level);
    g_key_file_set_integer(keyfile, "Battery", "CriticalLevel", config.battery_critical_level);
    g_key_file_set_integer(keyfile, "Battery", "DangerLevel", config.battery_danger_level);
    
    // كتابة الإجراءات
    g_key_file_set_string(keyfile, "Actions", "LidActionAC", config.lid_action_ac);
    g_key_file_set_string(keyfile, "Actions", "LidActionBattery", config.lid_action_battery);
    g_key_file_set_string(keyfile, "Actions", "PowerButtonAction", config.power_button_action);
    g_key_file_set_string(keyfile, "Actions", "CriticalAction", config.critical_action);
    
    // كتابة إعدادات السطوع
    g_key_file_set_integer(keyfile, "Brightness", "AC", config.brightness_ac);
    g_key_file_set_integer(keyfile, "Brightness", "Battery", config.brightness_battery);
    g_key_file_set_boolean(keyfile, "Brightness", "DimOnIdle", config.dim_on_idle);
    g_key_file_set_integer(keyfile, "Brightness", "DimLevelPercent", config.dim_level_percent);
    
    // كتابة إعدادات الكيبورد
    g_key_file_set_boolean(keyfile, "Keyboard", "AutoOff", config.kbd_backlight_auto_off);
    g_key_file_set_integer(keyfile, "Keyboard", "Timeout", config.kbd_backlight_timeout);
    
    // كتابة إعدادات القفل
    g_key_file_set_boolean(keyfile, "Lock", "OnSuspend", config.lock_on_suspend);
    g_key_file_set_boolean(keyfile, "Lock", "OnLid", config.lock_on_lid);
    g_key_file_set_string(keyfile, "Lock", "Command", config.lock_screen_cmd);
    
    GError *error = NULL;
    gboolean success = g_key_file_save_to_file(keyfile, file_path, &error);
    
    if (success) {
        printf("⚙️ Config saved to %s\n", file_path);
    } else {
        fprintf(stderr, "⚙️ Failed to save config: %s\n", error->message);
        g_error_free(error);
    }
    
    g_key_file_free(keyfile);
    g_free(dir_path);
    g_free(file_path);
    
    return success;
}

void config_free(void) {
    g_free(config.lid_action_ac);
    g_free(config.lid_action_battery);
    g_free(config.power_button_action);
    g_free(config.critical_action);
    g_free(config.lock_screen_cmd);
    memset(&config, 0, sizeof(config));
    config_loaded = FALSE;
}
