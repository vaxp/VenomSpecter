// #include <gio/gio.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <dirent.h>

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔧 الإعدادات والثوابت
// // ═══════════════════════════════════════════════════════════════════════════

// #define LOCK_SCREEN_CMD "/home/x/Desktop/venomlocker/build/linux/x64/release/bundle/venomlocker &"
// #define BACKLIGHT_PATH "/sys/class/backlight"

// // أوقات الخمول (بالثواني)
// #define DEFAULT_DIM_TIMEOUT      120   // 2 دقيقة
// #define DEFAULT_BLANK_TIMEOUT    300   // 5 دقائق
// #define DEFAULT_SUSPEND_TIMEOUT  900   // 15 دقيقة (على البطارية فقط)

// // مستويات البطارية
// #define BATTERY_LOW      10
// #define BATTERY_CRITICAL  5
// #define BATTERY_DANGER    2

// // ═══════════════════════════════════════════════════════════════════════════
// // 📦 هياكل البيانات
// // ═══════════════════════════════════════════════════════════════════════════

// typedef enum {
//     INHIBIT_IDLE     = 1 << 0,
//     INHIBIT_SUSPEND  = 1 << 1,
//     INHIBIT_SHUTDOWN = 1 << 2
// } InhibitType;

// typedef struct {
//     guint id;
//     gchar *app_name;
//     gchar *reason;
//     InhibitType type;
// } Inhibitor;

// typedef struct {
//     // إعدادات الخمول
//     guint dim_timeout;
//     guint blank_timeout;
//     guint suspend_timeout;
    
//     // الحالة الحالية
//     gboolean is_idle;
//     gboolean screen_dimmed;
//     gboolean screen_blanked;
//     gint original_brightness;
    
//     // معرفات المؤقتات
//     guint dim_timer_id;
//     guint blank_timer_id;
//     guint suspend_timer_id;
    
//     // حالة البطارية
//     gdouble battery_percentage;
//     gboolean on_battery;
//     gboolean lid_closed;
    
//     // المانعات
//     GList *inhibitors;
//     guint next_inhibitor_id;
    
//     // الاتصالات
//     GDBusConnection *session_conn;
//     GDBusConnection *system_conn;
// } PowerState;

// static GMainLoop *loop;
// static PowerState state = {0};

// // ═══════════════════════════════════════════════════════════════════════════
// // 📋 تعريف واجهة D-Bus
// // ═══════════════════════════════════════════════════════════════════════════

// static const gchar introspection_xml[] =
// "<node>"
// "  <interface name='org.venom.Power'>"
// // أوامر الطاقة الأساسية
// "    <method name='Shutdown'><arg type='b' name='success' direction='out'/></method>"
// "    <method name='Reboot'><arg type='b' name='success' direction='out'/></method>"
// "    <method name='Suspend'><arg type='b' name='success' direction='out'/></method>"
// "    <method name='Hibernate'><arg type='b' name='success' direction='out'/></method>"
// "    <method name='Logout'><arg type='b' name='success' direction='out'/></method>"
// "    <method name='LockScreen'><arg type='b' name='success' direction='out'/></method>"
// // التحكم بالسطوع
// "    <method name='GetBrightness'><arg type='i' name='level' direction='out'/></method>"
// "    <method name='SetBrightness'><arg type='i' name='level' direction='in'/><arg type='b' name='success' direction='out'/></method>"
// // معلومات البطارية
// "    <method name='GetBatteryInfo'>"
// "      <arg type='d' name='percentage' direction='out'/>"
// "      <arg type='b' name='charging' direction='out'/>"
// "      <arg type='x' name='time_to_empty' direction='out'/>"
// "    </method>"
// // حالة الغطاء
// "    <method name='GetLidState'><arg type='b' name='closed' direction='out'/></method>"
// // نظام المنع
// "    <method name='Inhibit'>"
// "      <arg type='s' name='what' direction='in'/>"
// "      <arg type='s' name='who' direction='in'/>"
// "      <arg type='s' name='why' direction='in'/>"
// "      <arg type='u' name='cookie' direction='out'/>"
// "    </method>"
// "    <method name='UnInhibit'><arg type='u' name='cookie' direction='in'/></method>"
// "    <method name='ListInhibitors'><arg type='a(uss)' name='inhibitors' direction='out'/></method>"
// // إعدادات الخمول
// "    <method name='SetIdleTimeouts'>"
// "      <arg type='u' name='dim' direction='in'/>"
// "      <arg type='u' name='blank' direction='in'/>"
// "      <arg type='u' name='suspend' direction='in'/>"
// "    </method>"
// "    <method name='SimulateActivity'/>"
// // الإشارات
// "    <signal name='BatteryWarning'><arg type='d' name='percentage'/></signal>"
// "    <signal name='BatteryCritical'><arg type='d' name='percentage'/></signal>"
// "    <signal name='LidStateChanged'><arg type='b' name='closed'/></signal>"
// "    <signal name='PowerSourceChanged'><arg type='b' name='on_battery'/></signal>"
// "    <signal name='ScreenDimmed'><arg type='b' name='dimmed'/></signal>"
// "    <signal name='ScreenBlanked'><arg type='b' name='blanked'/></signal>"
// "  </interface>"
// "</node>";

// // ═══════════════════════════════════════════════════════════════════════════
// // 💡 التحكم بالسطوع
// // ═══════════════════════════════════════════════════════════════════════════

// static gchar* find_backlight_device() {
//     DIR *dir = opendir(BACKLIGHT_PATH);
//     if (!dir) return NULL;
    
//     struct dirent *entry;
//     gchar *device = NULL;
    
//     while ((entry = readdir(dir)) != NULL) {
//         if (entry->d_name[0] != '.') {
//             device = g_strdup(entry->d_name);
//             break;
//         }
//     }
//     closedir(dir);
//     return device;
// }

// static gint read_backlight_value(const gchar *file) {
//     gchar *device = find_backlight_device();
//     if (!device) return -1;
    
//     gchar *path = g_strdup_printf("%s/%s/%s", BACKLIGHT_PATH, device, file);
//     g_free(device);
    
//     gchar *contents = NULL;
//     gint value = -1;
    
//     if (g_file_get_contents(path, &contents, NULL, NULL)) {
//         value = atoi(contents);
//         g_free(contents);
//     }
//     g_free(path);
//     return value;
// }

// static gint get_brightness() {
//     return read_backlight_value("brightness");
// }

// static gint get_max_brightness() {
//     return read_backlight_value("max_brightness");
// }

// static gboolean set_brightness(gint level) {
//     gchar *device = find_backlight_device();
//     if (!device) return FALSE;
    
//     gchar *path = g_strdup_printf("%s/%s/brightness", BACKLIGHT_PATH, device);
//     g_free(device);
    
//     gint max = get_max_brightness();
//     if (max <= 0) {
//         g_free(path);
//         return FALSE;
//     }
    
//     // تحديد المستوى بين 0 والحد الأقصى
//     if (level < 0) level = 0;
//     if (level > max) level = max;
    
//     gchar *value_str = g_strdup_printf("%d", level);
//     gboolean success = g_file_set_contents(path, value_str, -1, NULL);
    
//     g_free(value_str);
//     g_free(path);
//     return success;
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔌 التحكم بـ DPMS (إطفاء الشاشة)
// // ═══════════════════════════════════════════════════════════════════════════

// static void screen_blank(gboolean blank) {
//     if (blank) {
//         system("xset dpms force off 2>/dev/null");
//         state.screen_blanked = TRUE;
//         printf("🖥️ Screen blanked\n");
//     } else {
//         system("xset dpms force on 2>/dev/null");
//         state.screen_blanked = FALSE;
//         printf("🖥️ Screen unblanked\n");
//     }
// }

// static void screen_dim(gboolean dim) {
//     if (dim && !state.screen_dimmed) {
//         state.original_brightness = get_brightness();
//         gint dimmed = state.original_brightness * 30 / 100; // تعتيم إلى 30%
//         if (dimmed < 1) dimmed = 1;
//         set_brightness(dimmed);
//         state.screen_dimmed = TRUE;
//         printf("🔅 Screen dimmed to %d%%\n", 30);
//     } else if (!dim && state.screen_dimmed) {
//         set_brightness(state.original_brightness);
//         state.screen_dimmed = FALSE;
//         printf("🔆 Screen brightness restored\n");
//     }
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🚫 نظام المنع (Inhibitors)
// // ═══════════════════════════════════════════════════════════════════════════

// static gboolean has_inhibitor(InhibitType type) {
//     for (GList *l = state.inhibitors; l != NULL; l = l->next) {
//         Inhibitor *inh = (Inhibitor*)l->data;
//         if (inh->type & type) return TRUE;
//     }
//     return FALSE;
// }

// static guint add_inhibitor(const gchar *what, const gchar *who, const gchar *why) {
//     Inhibitor *inh = g_new0(Inhibitor, 1);
//     inh->id = ++state.next_inhibitor_id;
//     inh->app_name = g_strdup(who);
//     inh->reason = g_strdup(why);
    
//     // تحويل "what" إلى نوع
//     if (g_str_has_prefix(what, "idle"))
//         inh->type = INHIBIT_IDLE;
//     else if (g_str_has_prefix(what, "suspend") || g_str_has_prefix(what, "sleep"))
//         inh->type = INHIBIT_SUSPEND;
//     else if (g_str_has_prefix(what, "shutdown"))
//         inh->type = INHIBIT_SHUTDOWN;
//     else
//         inh->type = INHIBIT_IDLE | INHIBIT_SUSPEND;
    
//     state.inhibitors = g_list_append(state.inhibitors, inh);
//     printf("🚫 Inhibitor added: [%u] %s - %s\n", inh->id, who, why);
//     return inh->id;
// }

// static void remove_inhibitor(guint cookie) {
//     for (GList *l = state.inhibitors; l != NULL; l = l->next) {
//         Inhibitor *inh = (Inhibitor*)l->data;
//         if (inh->id == cookie) {
//             printf("✅ Inhibitor removed: [%u] %s\n", inh->id, inh->app_name);
//             g_free(inh->app_name);
//             g_free(inh->reason);
//             state.inhibitors = g_list_delete_link(state.inhibitors, l);
//             g_free(inh);
//             return;
//         }
//     }
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // ⏳ إدارة الخمول
// // ═══════════════════════════════════════════════════════════════════════════

// static void cancel_idle_timers() {
//     if (state.dim_timer_id) {
//         g_source_remove(state.dim_timer_id);
//         state.dim_timer_id = 0;
//     }
//     if (state.blank_timer_id) {
//         g_source_remove(state.blank_timer_id);
//         state.blank_timer_id = 0;
//     }
//     if (state.suspend_timer_id) {
//         g_source_remove(state.suspend_timer_id);
//         state.suspend_timer_id = 0;
//     }
// }

// static gboolean call_logind(const gchar *method, GVariant *params);

// static gboolean on_suspend_timeout(gpointer data) {
//     state.suspend_timer_id = 0;
    
//     if (has_inhibitor(INHIBIT_SUSPEND)) {
//         printf("⏸️ Auto-suspend blocked by inhibitor\n");
//         return FALSE;
//     }
    
//     // السكون التلقائي فقط على البطارية
//     if (state.on_battery) {
//         printf("💤 Auto-suspending after idle timeout...\n");
//         system(LOCK_SCREEN_CMD);
//         g_usleep(500000); // انتظار نصف ثانية
//         call_logind("Suspend", g_variant_new("(b)", TRUE));
//     }
//     return FALSE;
// }

// static gboolean on_blank_timeout(gpointer data) {
//     state.blank_timer_id = 0;
    
//     if (has_inhibitor(INHIBIT_IDLE)) {
//         printf("⏸️ Screen blank blocked by inhibitor\n");
//         return FALSE;
//     }
    
//     screen_blank(TRUE);
    
//     // جدولة السكون التلقائي
//     if (state.on_battery && state.suspend_timeout > 0) {
//         guint delay = state.suspend_timeout - state.blank_timeout;
//         if (delay > 0) {
//             state.suspend_timer_id = g_timeout_add_seconds(delay, on_suspend_timeout, NULL);
//         }
//     }
//     return FALSE;
// }

// static gboolean on_dim_timeout(gpointer data) {
//     state.dim_timer_id = 0;
    
//     if (has_inhibitor(INHIBIT_IDLE)) {
//         printf("⏸️ Screen dim blocked by inhibitor\n");
//         return FALSE;
//     }
    
//     screen_dim(TRUE);
    
//     // جدولة إطفاء الشاشة
//     if (state.blank_timeout > 0) {
//         guint delay = state.blank_timeout - state.dim_timeout;
//         if (delay > 0) {
//             state.blank_timer_id = g_timeout_add_seconds(delay, on_blank_timeout, NULL);
//         }
//     }
//     return FALSE;
// }

// static void reset_idle_timers() {
//     cancel_idle_timers();
    
//     // استعادة الشاشة
//     if (state.screen_blanked) screen_blank(FALSE);
//     if (state.screen_dimmed) screen_dim(FALSE);
    
//     state.is_idle = FALSE;
    
//     // بدء مؤقت التعتيم
//     if (state.dim_timeout > 0) {
//         state.dim_timer_id = g_timeout_add_seconds(state.dim_timeout, on_dim_timeout, NULL);
//     }
    
//     printf("⏰ Idle timers reset (dim: %us, blank: %us, suspend: %us)\n",
//            state.dim_timeout, state.blank_timeout, state.suspend_timeout);
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔋 مراقبة البطارية
// // ═══════════════════════════════════════════════════════════════════════════

// static void emit_signal(const gchar *signal_name, GVariant *params) {
//     if (state.session_conn) {
//         g_dbus_connection_emit_signal(
//             state.session_conn,
//             NULL,
//             "/org/venom/Power",
//             "org.venom.Power",
//             signal_name,
//             params,
//             NULL
//         );
//     }
// }

// static void send_notification(const gchar *title, const gchar *body, const gchar *urgency) {
//     gchar *cmd = g_strdup_printf("notify-send -u %s '%s' '%s' 2>/dev/null", 
//                                   urgency, title, body);
//     system(cmd);
//     g_free(cmd);
// }

// static void check_battery_level(gdouble percentage, gboolean charging) {
//     static gboolean warned_low = FALSE;
//     static gboolean warned_critical = FALSE;
    
//     if (charging) {
//         warned_low = FALSE;
//         warned_critical = FALSE;
//         return;
//     }
    
//     if (percentage <= BATTERY_DANGER) {
//         printf("🚨 CRITICAL: Battery at %.0f%%! Hibernating to save data!\n", percentage);
//         send_notification("⚠️ بطارية حرجة!", "سيتم إيقاف الجهاز لحماية البيانات", "critical");
//         emit_signal("BatteryCritical", g_variant_new("(d)", percentage));
        
//         // محاولة السبات أولاً، ثم الإيقاف
//         if (!call_logind("Hibernate", g_variant_new("(b)", TRUE))) {
//             call_logind("PowerOff", g_variant_new("(b)", TRUE));
//         }
//     }
//     else if (percentage <= BATTERY_CRITICAL && !warned_critical) {
//         printf("⚠️ Battery critical: %.0f%%\n", percentage);
//         send_notification("⚠️ البطارية 5%", "يرجى شبك الشاحن فوراً!", "critical");
//         emit_signal("BatteryWarning", g_variant_new("(d)", percentage));
//         warned_critical = TRUE;
//     }
//     else if (percentage <= BATTERY_LOW && !warned_low) {
//         printf("🔋 Battery low: %.0f%%\n", percentage);
//         send_notification("🔋 البطارية منخفضة", "10% متبقية", "normal");
//         emit_signal("BatteryWarning", g_variant_new("(d)", percentage));
//         warned_low = TRUE;
//     }
// }

// static void on_upower_properties_changed(GDBusConnection *connection,
//                                          const gchar *sender_name,
//                                          const gchar *object_path,
//                                          const gchar *interface_name,
//                                          const gchar *signal_name,
//                                          GVariant *parameters,
//                                          gpointer user_data) {
//     GVariant *changed_props = NULL;
//     const gchar *iface = NULL;
    
//     g_variant_get(parameters, "(&s@a{sv}as)", &iface, &changed_props, NULL);
    
//     if (changed_props) {
//         GVariant *percentage_v = g_variant_lookup_value(changed_props, "Percentage", G_VARIANT_TYPE_DOUBLE);
//         GVariant *state_v = g_variant_lookup_value(changed_props, "State", G_VARIANT_TYPE_UINT32);
        
//         if (percentage_v) {
//             gdouble pct = g_variant_get_double(percentage_v);
//             guint32 bat_state = state_v ? g_variant_get_uint32(state_v) : 0;
//             gboolean charging = (bat_state == 1); // 1 = Charging
            
//             state.battery_percentage = pct;
//             check_battery_level(pct, charging);
            
//             g_variant_unref(percentage_v);
//         }
//         if (state_v) g_variant_unref(state_v);
//         g_variant_unref(changed_props);
//     }
// }

// static void on_upower_device_properties_changed(GDBusConnection *connection,
//                                                 const gchar *sender_name,
//                                                 const gchar *object_path,
//                                                 const gchar *interface_name,
//                                                 const gchar *signal_name,
//                                                 GVariant *parameters,
//                                                 gpointer user_data) {
//     GVariant *changed_props = NULL;
//     const gchar *iface = NULL;
    
//     g_variant_get(parameters, "(&s@a{sv}as)", &iface, &changed_props, NULL);
    
//     if (changed_props) {
//         GVariant *on_battery_v = g_variant_lookup_value(changed_props, "OnBattery", G_VARIANT_TYPE_BOOLEAN);
        
//         if (on_battery_v) {
//             gboolean on_battery = g_variant_get_boolean(on_battery_v);
            
//             if (on_battery != state.on_battery) {
//                 state.on_battery = on_battery;
//                 printf("🔌 Power source: %s\n", on_battery ? "Battery" : "AC");
//                 emit_signal("PowerSourceChanged", g_variant_new("(b)", on_battery));
                
//                 // إعادة ضبط المؤقتات حسب مصدر الطاقة
//                 reset_idle_timers();
//             }
//             g_variant_unref(on_battery_v);
//         }
//         g_variant_unref(changed_props);
//     }
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 💻 مراقبة أحداث Logind
// // ═══════════════════════════════════════════════════════════════════════════

// static void on_logind_properties_changed(GDBusConnection *connection,
//                                          const gchar *sender_name,
//                                          const gchar *object_path,
//                                          const gchar *interface_name,
//                                          const gchar *signal_name,
//                                          GVariant *parameters,
//                                          gpointer user_data) {
//     GVariant *changed_props = NULL;
//     const gchar *iface = NULL;
    
//     g_variant_get(parameters, "(&s@a{sv}as)", &iface, &changed_props, NULL);
    
//     if (changed_props) {
//         // مراقبة حالة الغطاء
//         GVariant *lid_v = g_variant_lookup_value(changed_props, "LidClosed", G_VARIANT_TYPE_BOOLEAN);
//         if (lid_v) {
//             gboolean closed = g_variant_get_boolean(lid_v);
            
//             if (closed != state.lid_closed) {
//                 state.lid_closed = closed;
//                 printf("💻 Lid %s\n", closed ? "closed" : "opened");
//                 emit_signal("LidStateChanged", g_variant_new("(b)", closed));
                
//                 if (closed) {
//                     // إذا أُغلق الغطاء: قفل الشاشة والسكون
//                     system(LOCK_SCREEN_CMD);
//                     if (state.on_battery) {
//                         g_usleep(500000);
//                         call_logind("Suspend", g_variant_new("(b)", TRUE));
//                     }
//                 }
//             }
//             g_variant_unref(lid_v);
//         }
//         g_variant_unref(changed_props);
//     }
// }

// static void on_prepare_for_sleep(GDBusConnection *connection,
//                                  const gchar *sender_name,
//                                  const gchar *object_path,
//                                  const gchar *interface_name,
//                                  const gchar *signal_name,
//                                  GVariant *parameters,
//                                  gpointer user_data) {
//     gboolean start_sleeping;
//     g_variant_get(parameters, "(b)", &start_sleeping);

//     if (start_sleeping) {
//         printf("💤 System is going to sleep! Launching Venom Locker...\n");
//         int ret = system(LOCK_SCREEN_CMD);
//         if (ret == -1) {
//             fprintf(stderr, "Failed to launch lock screen\n");
//         }
//     } else {
//         printf("☀️ System just woke up.\n");
//         reset_idle_timers();
//     }
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔌 دوال التحكم بالطاقة
// // ═══════════════════════════════════════════════════════════════════════════

// static gboolean call_logind(const gchar *method, GVariant *params) {
//     GError *error = NULL;
//     GVariant *result;

//     if (!state.system_conn) return FALSE;

//     result = g_dbus_connection_call_sync(
//         state.system_conn, "org.freedesktop.login1", "/org/freedesktop/login1",
//         "org.freedesktop.login1.Manager", method, params,
//         NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

//     if (error) {
//         fprintf(stderr, "Logind error: %s\n", error->message);
//         g_error_free(error);
//         return FALSE;
//     }
//     g_variant_unref(result);
//     return TRUE;
// }

// static gboolean do_logout() {
//     char *session_id = getenv("XDG_SESSION_ID");
//     if (!session_id) return FALSE;
    
//     char path[128];
//     snprintf(path, sizeof(path), "/org/freedesktop/login1/session/%s", session_id);
    
//     g_dbus_connection_call_sync(state.system_conn, "org.freedesktop.login1", path,
//         "org.freedesktop.login1.Session", "Terminate", NULL, NULL, 
//         G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
//     return TRUE;
// }

// static gboolean do_lock_screen() {
//     int ret = system(LOCK_SCREEN_CMD);
//     return (ret != -1);
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🔋 استعلام معلومات البطارية
// // ═══════════════════════════════════════════════════════════════════════════

// static void get_battery_info(gdouble *percentage, gboolean *charging, gint64 *time_to_empty) {
//     *percentage = 0;
//     *charging = FALSE;
//     *time_to_empty = 0;
    
//     if (!state.system_conn) return;
    
//     GError *error = NULL;
//     GVariant *result = g_dbus_connection_call_sync(
//         state.system_conn,
//         "org.freedesktop.UPower",
//         "/org/freedesktop/UPower/devices/DisplayDevice",
//         "org.freedesktop.DBus.Properties",
//         "GetAll",
//         g_variant_new("(s)", "org.freedesktop.UPower.Device"),
//         G_VARIANT_TYPE("(a{sv})"),
//         G_DBUS_CALL_FLAGS_NONE,
//         -1, NULL, &error
//     );
    
//     if (error) {
//         g_error_free(error);
//         return;
//     }
    
//     GVariant *props = NULL;
//     g_variant_get(result, "(@a{sv})", &props);
    
//     if (props) {
//         GVariant *v;
        
//         if ((v = g_variant_lookup_value(props, "Percentage", G_VARIANT_TYPE_DOUBLE))) {
//             *percentage = g_variant_get_double(v);
//             g_variant_unref(v);
//         }
//         if ((v = g_variant_lookup_value(props, "State", G_VARIANT_TYPE_UINT32))) {
//             *charging = (g_variant_get_uint32(v) == 1);
//             g_variant_unref(v);
//         }
//         if ((v = g_variant_lookup_value(props, "TimeToEmpty", G_VARIANT_TYPE_INT64))) {
//             *time_to_empty = g_variant_get_int64(v);
//             g_variant_unref(v);
//         }
//         g_variant_unref(props);
//     }
//     g_variant_unref(result);
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 📡 معالج طلبات D-Bus
// // ═══════════════════════════════════════════════════════════════════════════

// static void handle_method_call(GDBusConnection *connection, const gchar *sender,
//                                const gchar *object_path, const gchar *interface_name,
//                                const gchar *method_name, GVariant *parameters,
//                                GDBusMethodInvocation *invocation, gpointer user_data) {
    
//     // أوامر الطاقة الأساسية
//     if (g_strcmp0(method_name, "Shutdown") == 0) {
//         gboolean success = call_logind("PowerOff", g_variant_new("(b)", TRUE));
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     else if (g_strcmp0(method_name, "Reboot") == 0) {
//         gboolean success = call_logind("Reboot", g_variant_new("(b)", TRUE));
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     else if (g_strcmp0(method_name, "Suspend") == 0) {
//         gboolean success = call_logind("Suspend", g_variant_new("(b)", TRUE));
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     else if (g_strcmp0(method_name, "Hibernate") == 0) {
//         gboolean success = call_logind("Hibernate", g_variant_new("(b)", TRUE));
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     else if (g_strcmp0(method_name, "Logout") == 0) {
//         gboolean success = do_logout();
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     else if (g_strcmp0(method_name, "LockScreen") == 0) {
//         gboolean success = do_lock_screen();
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     // التحكم بالسطوع
//     else if (g_strcmp0(method_name, "GetBrightness") == 0) {
//         gint level = get_brightness();
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", level));
//     }
//     else if (g_strcmp0(method_name, "SetBrightness") == 0) {
//         gint level;
//         g_variant_get(parameters, "(i)", &level);
//         gboolean success = set_brightness(level);
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", success));
//     }
//     // معلومات البطارية
//     else if (g_strcmp0(method_name, "GetBatteryInfo") == 0) {
//         gdouble percentage;
//         gboolean charging;
//         gint64 time_to_empty;
//         get_battery_info(&percentage, &charging, &time_to_empty);
//         g_dbus_method_invocation_return_value(invocation, 
//             g_variant_new("(dbx)", percentage, charging, time_to_empty));
//     }
//     // حالة الغطاء
//     else if (g_strcmp0(method_name, "GetLidState") == 0) {
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", state.lid_closed));
//     }
//     // نظام المنع
//     else if (g_strcmp0(method_name, "Inhibit") == 0) {
//         const gchar *what, *who, *why;
//         g_variant_get(parameters, "(&s&s&s)", &what, &who, &why);
//         guint cookie = add_inhibitor(what, who, why);
//         g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", cookie));
//     }
//     else if (g_strcmp0(method_name, "UnInhibit") == 0) {
//         guint cookie;
//         g_variant_get(parameters, "(u)", &cookie);
//         remove_inhibitor(cookie);
//         g_dbus_method_invocation_return_value(invocation, NULL);
//     }
//     else if (g_strcmp0(method_name, "ListInhibitors") == 0) {
//         GVariantBuilder builder;
//         g_variant_builder_init(&builder, G_VARIANT_TYPE("a(uss)"));
        
//         for (GList *l = state.inhibitors; l != NULL; l = l->next) {
//             Inhibitor *inh = (Inhibitor*)l->data;
//             g_variant_builder_add(&builder, "(uss)", inh->id, inh->app_name, inh->reason);
//         }
        
//         g_dbus_method_invocation_return_value(invocation, 
//             g_variant_new("(a(uss))", &builder));
//     }
//     // إعدادات الخمول
//     else if (g_strcmp0(method_name, "SetIdleTimeouts") == 0) {
//         g_variant_get(parameters, "(uuu)", &state.dim_timeout, &state.blank_timeout, &state.suspend_timeout);
//         reset_idle_timers();
//         g_dbus_method_invocation_return_value(invocation, NULL);
//     }
//     else if (g_strcmp0(method_name, "SimulateActivity") == 0) {
//         reset_idle_timers();
//         g_dbus_method_invocation_return_value(invocation, NULL);
//     }
//     else {
//         g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, 
//             G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method: %s", method_name);
//     }
// }

// static const GDBusInterfaceVTable interface_vtable = { handle_method_call, NULL, NULL };

// // ═══════════════════════════════════════════════════════════════════════════
// // 🚀 تهيئة الخدمة
// // ═══════════════════════════════════════════════════════════════════════════

// static void setup_upower_monitoring() {
//     if (!state.system_conn) return;
    
//     // مراقبة بطارية العرض
//     g_dbus_connection_signal_subscribe(
//         state.system_conn,
//         "org.freedesktop.UPower",
//         "org.freedesktop.DBus.Properties",
//         "PropertiesChanged",
//         "/org/freedesktop/UPower/devices/DisplayDevice",
//         NULL,
//         G_DBUS_SIGNAL_FLAGS_NONE,
//         on_upower_properties_changed,
//         NULL, NULL
//     );
    
//     // مراقبة تغيير مصدر الطاقة
//     g_dbus_connection_signal_subscribe(
//         state.system_conn,
//         "org.freedesktop.UPower",
//         "org.freedesktop.DBus.Properties",
//         "PropertiesChanged",
//         "/org/freedesktop/UPower",
//         NULL,
//         G_DBUS_SIGNAL_FLAGS_NONE,
//         on_upower_device_properties_changed,
//         NULL, NULL
//     );
    
//     printf("🔋 UPower monitoring active\n");
// }

// static void setup_logind_monitoring() {
//     if (!state.system_conn) return;
    
//     // مراقبة السكون
//     g_dbus_connection_signal_subscribe(
//         state.system_conn,
//         "org.freedesktop.login1",
//         "org.freedesktop.login1.Manager",
//         "PrepareForSleep",
//         "/org/freedesktop/login1",
//         NULL,
//         G_DBUS_SIGNAL_FLAGS_NONE,
//         on_prepare_for_sleep,
//         NULL, NULL
//     );
    
//     // مراقبة تغيير الخصائص (الغطاء)
//     g_dbus_connection_signal_subscribe(
//         state.system_conn,
//         "org.freedesktop.login1",
//         "org.freedesktop.DBus.Properties",
//         "PropertiesChanged",
//         "/org/freedesktop/login1",
//         "org.freedesktop.login1.Manager",
//         G_DBUS_SIGNAL_FLAGS_NONE,
//         on_logind_properties_changed,
//         NULL, NULL
//     );
    
//     printf("💻 Logind monitoring active\n");
// }

// static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
//     GError *error = NULL;
//     GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    
//     if (error) {
//         fprintf(stderr, "Failed to parse introspection: %s\n", error->message);
//         g_error_free(error);
//         return;
//     }
    
//     // تسجيل الخدمة
//     g_dbus_connection_register_object(connection, "/org/venom/Power",
//                                       node_info->interfaces[0],
//                                       &interface_vtable, NULL, NULL, NULL);
    
//     state.session_conn = connection;
    
//     printf("\n");
//     printf("══════════════════════════════════════════════════════════════\n");
//     printf("⚡ Venom Power Daemon v2.0 Running\n");
//     printf("══════════════════════════════════════════════════════════════\n");
    
//     // الاتصال بـ System Bus
//     state.system_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
//     if (!state.system_conn) {
//         fprintf(stderr, "⚠️ Could not connect to system bus: %s\n", error->message);
//         g_error_free(error);
//     } else {
//         setup_upower_monitoring();
//         setup_logind_monitoring();
//     }
    
//     // تهيئة حالة الخمول
//     state.dim_timeout = DEFAULT_DIM_TIMEOUT;
//     state.blank_timeout = DEFAULT_BLANK_TIMEOUT;
//     state.suspend_timeout = DEFAULT_SUSPEND_TIMEOUT;
    
//     // قراءة الحالة الأولية للبطارية
//     gdouble pct;
//     gboolean charging;
//     gint64 tte;
//     get_battery_info(&pct, &charging, &tte);
//     state.battery_percentage = pct;
//     state.on_battery = !charging;
    
//     printf("🔋 Battery: %.0f%% (%s)\n", pct, charging ? "Charging" : "Discharging");
//     printf("⏰ Idle timeouts: dim=%us, blank=%us, suspend=%us\n",
//            state.dim_timeout, state.blank_timeout, state.suspend_timeout);
//     printf("══════════════════════════════════════════════════════════════\n\n");
    
//     // بدء مؤقتات الخمول
//     reset_idle_timers();
    
//     g_dbus_node_info_unref(node_info);
// }

// static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
//     fprintf(stderr, "❌ Lost D-Bus name: %s\n", name);
//     g_main_loop_quit(loop);
// }

// // ═══════════════════════════════════════════════════════════════════════════
// // 🎯 نقطة الدخول الرئيسية
// // ═══════════════════════════════════════════════════════════════════════════

// int main(int argc, char *argv[]) {
//     printf("🚀 Starting Venom Power Daemon...\n");
    
//     g_bus_own_name(G_BUS_TYPE_SESSION, "org.venom.Power", G_BUS_NAME_OWNER_FLAGS_NONE,
//                    on_bus_acquired, NULL, on_name_lost, NULL, NULL);
    
//     loop = g_main_loop_new(NULL, FALSE);
//     g_main_loop_run(loop);
    
//     // تنظيف
//     cancel_idle_timers();
//     if (state.system_conn) g_object_unref(state.system_conn);
    
//     return 0;
// }