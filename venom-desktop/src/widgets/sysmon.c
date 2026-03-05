#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../include/venom-widget-api.h"

/* Sysmon State */
static GtkWidget *lbl_cpu = NULL;
static GtkWidget *lbl_ram = NULL;
static GtkWidget *prog_cpu = NULL;
static GtkWidget *prog_ram = NULL;

static unsigned long long last_total_user, last_total_user_low, last_total_sys, last_total_idle;

/* Drag tracking state */
static gboolean is_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int widget_start_x = 0;
static int widget_start_y = 0;
static VenomDesktopAPI *api_handle = NULL;

/* CPU Calculation */
static double get_cpu_usage() {
    FILE *file;
    double percent = 0.0;
    unsigned long long total_user, total_user_low, total_sys, total_idle, total;
    
    file = fopen("/proc/stat", "r");
    if (!file) return 0.0;
    
    if (fscanf(file, "cpu %llu %llu %llu %llu", &total_user, &total_user_low, &total_sys, &total_idle) < 4) {
        fclose(file);
        return 0.0;
    }
    fclose(file);
    
    if (total_user < last_total_user || total_user_low < last_total_user_low ||
        total_sys < last_total_sys || total_idle < last_total_idle) {
        percent = -1.0; /* overflow */
    } else {
        total = (total_user - last_total_user) + (total_user_low - last_total_user_low) + (total_sys - last_total_sys);
        percent = total;
        total += (total_idle - last_total_idle);
        percent /= total;
        percent *= 100;
    }
    
    last_total_user = total_user;
    last_total_user_low = total_user_low;
    last_total_sys = total_sys;
    last_total_idle = total_idle;
    
    return percent >= 0 ? percent : 0.0;
}

/* Memory Calculation */
static double get_ram_usage() {
    FILE *file;
    char buffer[256];
    unsigned long long mem_total = 0, mem_free = 0, mem_avail = 0, buffers = 0, cached = 0;
    
    file = fopen("/proc/meminfo", "r");
    if (!file) return 0.0;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        if (sscanf(buffer, "MemTotal: %llu kB", &mem_total) == 1) continue;
        if (sscanf(buffer, "MemFree: %llu kB", &mem_free) == 1) continue;
        if (sscanf(buffer, "MemAvailable: %llu kB", &mem_avail) == 1) continue;
        if (sscanf(buffer, "Buffers: %llu kB", &buffers) == 1) continue;
        if (sscanf(buffer, "Cached: %llu kB", &cached) == 1) continue;
    }
    fclose(file);
    
    if (mem_total == 0) return 0.0;
    
    /* Calculating Used memory exactly like htop does */
    unsigned long long mem_used = mem_total - mem_free - buffers - cached;
    return ((double)mem_used / mem_total) * 100.0;
}

static gboolean update_sysmon(gpointer data) {
    if (!lbl_cpu || !lbl_ram) return FALSE;
    
    double cpu = get_cpu_usage();
    double ram = get_ram_usage();
    
    char cpu_str[32];
    char ram_str[32];
    snprintf(cpu_str, sizeof(cpu_str), "CPU: %.1f%%", cpu);
    snprintf(ram_str, sizeof(ram_str), "RAM: %.1f%%", ram);
    
    gtk_label_set_text(GTK_LABEL(lbl_cpu), cpu_str);
    gtk_label_set_text(GTK_LABEL(lbl_ram), ram_str);
    
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog_cpu), cpu / 100.0);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog_ram), ram / 100.0);
    
    return TRUE; /* Continue timer */
}

/* Mouse Interaction Handlers */
static gboolean on_widget_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) { // Left click
        is_dragging = TRUE;
        drag_start_x = event->x_root;
        drag_start_y = event->y_root;
        
        gint wx, wy;
        gtk_widget_translate_coordinates(widget, gtk_widget_get_toplevel(widget), 0, 0, &wx, &wy);
        widget_start_x = wx;
        widget_start_y = wy;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_widget_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_dragging && api_handle && api_handle->layout_container) {
        int dx = event->x_root - drag_start_x;
        int dy = event->y_root - drag_start_y;
        
        int new_x = widget_start_x + dx;
        int new_y = widget_start_y + dy;
        
        gtk_layout_move(GTK_LAYOUT(api_handle->layout_container), widget, new_x, new_y);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_widget_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1 && is_dragging) {
        is_dragging = FALSE;
        
        if (api_handle && api_handle->save_position && api_handle->layout_container) {
            gint x, y;
            gtk_widget_translate_coordinates(widget, gtk_widget_get_toplevel(widget), 0, 0, &x, &y);
            api_handle->save_position("sysmon.so", x, y);
        }
        return TRUE;
    }
    return FALSE;
}

/* Initialization GUI construction */
static GtkWidget* create_sysmon_ui(VenomDesktopAPI *desktop_api) {
    /* Main container (EventBox to catch mouse interactions) */
    api_handle = desktop_api;
    GtkWidget *event_box = gtk_event_box_new();
    gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
    
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_widget_button_press), NULL);
    g_signal_connect(event_box, "motion-notify-event", G_CALLBACK(on_widget_motion), NULL);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_widget_button_release), NULL);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(event_box), frame);
    gtk_widget_set_size_request(frame, 220, 100);
    
    /* Create a CSS provider exactly for the widget boundary */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "frame { "
        "   background-color: rgba(0, 0, 0, 0.23); "
        "   border: 1px solid rgba(0, 0, 0, 0.1); "
        "   border-radius: 8px; "
        "   padding: 10px; "
        "}"
        "label { color: #ecf0f1; font-weight: bold; font-family: monospace; }"
        "progressbar trough { background-color: rgba(0,0,0,0.6); border-radius: 4px; min-height: 8px; }"
        "progressbar progress { background-color: #3498db; border-radius: 4px; }", -1, NULL);
        
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), 800);
    g_object_unref(css);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* Headline */
    GtkWidget *title = gtk_label_new("System Monitor");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    /* CPU Box */
    GtkWidget *cpu_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    lbl_cpu = gtk_label_new("CPU: 0.0%");
    gtk_widget_set_size_request(lbl_cpu, 90, -1);
    gtk_widget_set_halign(lbl_cpu, GTK_ALIGN_START);
    prog_cpu = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(cpu_box), lbl_cpu, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cpu_box), prog_cpu, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), cpu_box, FALSE, FALSE, 0);

    /* RAM Box */
    GtkWidget *ram_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    lbl_ram = gtk_label_new("RAM: 0.0%");
    gtk_widget_set_size_request(lbl_ram, 90, -1);
    gtk_widget_set_halign(lbl_ram, GTK_ALIGN_START);
    prog_ram = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(ram_box), lbl_ram, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ram_box), prog_ram, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), ram_box, FALSE, FALSE, 0);

    /* Start the updater loop */
    g_timeout_add(1000, update_sysmon, NULL);
    
    gtk_widget_show_all(event_box);
    return event_box;
}

/* Export API Hook */
VenomWidgetAPI* venom_widget_init(void) {
    static VenomWidgetAPI api;
    api.name = "System Monitor";
    api.description = "Displays live CPU and RAM usage percentages.";
    api.author = "Venom Core";
    api.create_widget = create_sysmon_ui;
    return &api;
}
