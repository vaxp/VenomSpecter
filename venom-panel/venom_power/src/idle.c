#include "idle.h"
#include "venom_power.h"
#include "backlight.h"
#include "inhibitor.h"
#include "logind.h"
#include "keyboard.h"
#include "input_monitor.h"
#include "dbus_service.h"
#include <stdio.h>
#include <unistd.h>

// ═══════════════════════════════════════════════════════════════════════════
// ⏳ إدارة الخمول المبنية على نشاط المستخدم الفعلي
// ═══════════════════════════════════════════════════════════════════════════

static guint idle_check_timer_id = 0;
static gboolean dim_triggered = FALSE;
static gboolean blank_triggered = FALSE;
static gboolean suspend_triggered = FALSE;
static gulong last_idle_ms = 0;
static gboolean was_idle = FALSE;

// ═══════════════════════════════════════════════════════════════════════════
// استعادة الشاشة عند نشاط المستخدم
// ═══════════════════════════════════════════════════════════════════════════

static void restore_screen_on_activity(void) {
    // استعادة الشاشة فقط إذا كانت معتمة/مطفأة
    if (power_state.screen_blanked) {
        printf("☀️ User active - unblanking screen\n");
        backlight_blank_screen(FALSE);
        dbus_emit_signal("ScreenBlanked", g_variant_new("(b)", FALSE));
    }
    
    if (power_state.screen_dimmed) {
        printf("☀️ User active - restoring brightness\n");
        backlight_dim_screen(FALSE);
        dbus_emit_signal("ScreenDimmed", g_variant_new("(b)", FALSE));
    }
    
    // استعادة إضاءة الكيبورد
    if (keyboard_is_supported()) {
        keyboard_auto_off(FALSE);
    }
    
    // إعادة ضبط جميع المحفزات
    dim_triggered = FALSE;
    blank_triggered = FALSE;
    suspend_triggered = FALSE;
    power_state.is_idle = FALSE;
    was_idle = FALSE;
}

// ═══════════════════════════════════════════════════════════════════════════
// فحص دوري لوقت الخمول الفعلي
// ═══════════════════════════════════════════════════════════════════════════

static gboolean check_idle_status(gpointer data) {
    (void)data;
    
    // الحصول على وقت الخمول الفعلي من X11
    gulong idle_ms = input_get_idle_time_ms();
    guint idle_seconds = idle_ms / 1000;
    
    // ═══════════════════════════════════════════════════════════════════════
    // كشف نشاط المستخدم: إذا انخفض وقت الخمول = المستخدم تحرك/كتب
    // ═══════════════════════════════════════════════════════════════════════
    if (idle_ms < last_idle_ms || idle_seconds < 3) {
        // المستخدم نشط الآن
        if (was_idle || power_state.screen_dimmed || power_state.screen_blanked) {
            restore_screen_on_activity();
        }
        last_idle_ms = idle_ms;
        return TRUE; // استمر في الفحص
    }
    
    last_idle_ms = idle_ms;
    
    // ═══════════════════════════════════════════════════════════════════════
    // تحقق من المانعات قبل أي إجراء
    // ═══════════════════════════════════════════════════════════════════════
    if (inhibitor_has_type(INHIBIT_IDLE)) {
        return TRUE; // مانع نشط
    }
    
    was_idle = TRUE;
    power_state.is_idle = TRUE;
    
    // ═══════════════════════════════════════════════════════════════════════
    // مرحلة 1: تعتيم الشاشة
    // ═══════════════════════════════════════════════════════════════════════
    if (!dim_triggered && power_state.dim_timeout > 0 && 
        idle_seconds >= power_state.dim_timeout) {
        
        printf("🔅 Idle %us >= %us, dimming screen\n", idle_seconds, power_state.dim_timeout);
        backlight_dim_screen(TRUE);
        dbus_emit_signal("ScreenDimmed", g_variant_new("(b)", TRUE));
        
        if (keyboard_is_supported()) {
            keyboard_auto_off(TRUE);
        }
        
        dim_triggered = TRUE;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // مرحلة 2: إطفاء الشاشة
    // ═══════════════════════════════════════════════════════════════════════
    if (!blank_triggered && power_state.blank_timeout > 0 && 
        idle_seconds >= power_state.blank_timeout) {
        
        printf("🖥️ Idle %us >= %us, blanking screen\n", idle_seconds, power_state.blank_timeout);
        backlight_blank_screen(TRUE);
        dbus_emit_signal("ScreenBlanked", g_variant_new("(b)", TRUE));
        
        blank_triggered = TRUE;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // مرحلة 3: السكون التلقائي (على البطارية فقط)
    // ═══════════════════════════════════════════════════════════════════════
    if (!suspend_triggered && power_state.on_battery && 
        power_state.suspend_timeout > 0 && 
        idle_seconds >= power_state.suspend_timeout) {
        
        if (inhibitor_has_type(INHIBIT_SUSPEND)) {
            printf("⏸️ Auto-suspend blocked by inhibitor\n");
            return TRUE;
        }
        
        printf("💤 Idle %us on battery, auto-suspending\n", idle_seconds);
        // لا نستدعي lock_screen هنا لأن PrepareForSleep سيفعل ذلك
        logind_call("Suspend", g_variant_new("(b)", TRUE));
        
        suspend_triggered = TRUE;
    }
    
    return TRUE;
}

// ═══════════════════════════════════════════════════════════════════════════
// واجهة عامة
// ═══════════════════════════════════════════════════════════════════════════

void idle_cancel_timers(void) {
    if (idle_check_timer_id) {
        g_source_remove(idle_check_timer_id);
        idle_check_timer_id = 0;
    }
    
    // استعادة الشاشة عند الإلغاء
    restore_screen_on_activity();
}

void idle_reset_timers(void) {
    // إيقاف المؤقت القديم دون استعادة الشاشة
    if (idle_check_timer_id) {
        g_source_remove(idle_check_timer_id);
        idle_check_timer_id = 0;
    }
    
    // استعادة الشاشة
    restore_screen_on_activity();
    
    // إعادة تعيين المتغيرات
    last_idle_ms = 0;
    
    // تهيئة مراقبة الإدخال
    if (!input_monitor_init()) {
        fprintf(stderr, "⚠️ Input monitoring unavailable\n");
    }
    
    // بدء الفحص الدوري كل ثانية
    idle_check_timer_id = g_timeout_add_seconds(1, check_idle_status, NULL);
    
    printf("⏰ Idle monitor: dim=%us, blank=%us, suspend=%us\n",
           power_state.dim_timeout, power_state.blank_timeout, power_state.suspend_timeout);
}

void idle_set_timeouts(guint dim_timeout, guint blank_timeout, guint suspend_timeout) {
    power_state.dim_timeout = dim_timeout;
    power_state.blank_timeout = blank_timeout;
    power_state.suspend_timeout = suspend_timeout;
    
    // إعادة ضبط المحفزات
    dim_triggered = FALSE;
    blank_triggered = FALSE;
    suspend_triggered = FALSE;
    
    printf("⏰ Timeouts updated: dim=%us, blank=%us, suspend=%us\n",
           dim_timeout, blank_timeout, suspend_timeout);
}
