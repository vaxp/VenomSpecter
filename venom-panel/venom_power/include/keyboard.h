#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <glib.h>

// ═══════════════════════════════════════════════════════════════════════════
// ⌨️ إضاءة الكيبورد
// ═══════════════════════════════════════════════════════════════════════════

/**
 * الحصول على مستوى إضاءة الكيبورد
 * @return مستوى الإضاءة أو -1 في حالة الفشل
 */
gint keyboard_get_brightness(void);

/**
 * الحصول على أقصى مستوى لإضاءة الكيبورد
 * @return أقصى مستوى أو -1 في حالة الفشل
 */
gint keyboard_get_max_brightness(void);

/**
 * تعيين إضاءة الكيبورد
 * @param level المستوى المطلوب
 * @return TRUE في حالة النجاح
 */
gboolean keyboard_set_brightness(gint level);

/**
 * تشغيل/إيقاف إضاءة الكيبورد تلقائياً عند الخمول
 * @param off TRUE لإيقاف الإضاءة، FALSE لاستعادتها
 */
void keyboard_auto_off(gboolean off);

/**
 * التحقق من دعم إضاءة الكيبورد
 * @return TRUE إذا كان الكيبورد يدعم الإضاءة
 */
gboolean keyboard_is_supported(void);

#endif // KEYBOARD_H
