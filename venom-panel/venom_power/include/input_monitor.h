#ifndef INPUT_MONITOR_H
#define INPUT_MONITOR_H

#include <glib.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🖱️ مراقبة نشاط المستخدم
// ═══════════════════════════════════════════════════════════════════════════

/**
 * تهيئة مراقبة الإدخال
 * @return TRUE في حالة النجاح
 */
gboolean input_monitor_init(void);

/**
 * إيقاف مراقبة الإدخال
 */
void input_monitor_stop(void);

/**
 * الحصول على وقت الخمول بالمللي ثانية
 * @return وقت الخمول منذ آخر نشاط
 */
gulong input_get_idle_time_ms(void);

/**
 * التحقق من وجود نشاط منذ آخر فحص
 * @return TRUE إذا كان هناك نشاط
 */
gboolean input_has_activity(void);

/**
 * إعادة تعيين عداد وقت الخمول
 */
void input_reset_idle(void);

#endif // INPUT_MONITOR_H
