#include "input_monitor.h"
#include "venom_power.h"
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🖱️ مراقبة نشاط المستخدم عبر XScreenSaver Extension
// ═══════════════════════════════════════════════════════════════════════════

static Display *display = NULL;
static gulong last_idle_time = 0;
static gboolean initialized = FALSE;

gboolean input_monitor_init(void) {
    if (initialized) return TRUE;
    
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "⚠️ Cannot open X display for input monitoring\n");
        return FALSE;
    }
    
    // التحقق من دعم XScreenSaver extension
    int event_base, error_base;
    if (!XScreenSaverQueryExtension(display, &event_base, &error_base)) {
        fprintf(stderr, "⚠️ XScreenSaver extension not available\n");
        XCloseDisplay(display);
        display = NULL;
        return FALSE;
    }
    
    initialized = TRUE;
    printf("🖱️ Input monitoring initialized (XScreenSaver)\n");
    return TRUE;
}

void input_monitor_stop(void) {
    if (display) {
        XCloseDisplay(display);
        display = NULL;
    }
    initialized = FALSE;
}

gulong input_get_idle_time_ms(void) {
    if (!display) return 0;
    
    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if (!info) return 0;
    
    XScreenSaverQueryInfo(display, DefaultRootWindow(display), info);
    gulong idle_ms = info->idle;
    
    XFree(info);
    return idle_ms;
}

gboolean input_has_activity(void) {
    gulong current_idle = input_get_idle_time_ms();
    
    // إذا كان وقت الخمول الحالي أقل من السابق، فهناك نشاط
    gboolean activity = (current_idle < last_idle_time);
    last_idle_time = current_idle;
    
    return activity;
}

void input_reset_idle(void) {
    last_idle_time = 0;
}
