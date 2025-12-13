#ifndef IDLE_H
#define IDLE_H

#include <glib.h>

// ═══════════════════════════════════════════════════════════════════════════
// ⏳ إدارة الخمول
// ═══════════════════════════════════════════════════════════════════════════

/**
 * إعادة ضبط مؤقتات الخمول (عند نشاط المستخدم)
 */
void idle_reset_timers(void);

/**
 * إلغاء جميع مؤقتات الخمول
 */
void idle_cancel_timers(void);

/**
 * تعيين أوقات الخمول
 * @param dim_timeout وقت التعتيم بالثواني
 * @param blank_timeout وقت إطفاء الشاشة بالثواني
 * @param suspend_timeout وقت السكون التلقائي بالثواني
 */
void idle_set_timeouts(guint dim_timeout, guint blank_timeout, guint suspend_timeout);

#endif // IDLE_H
