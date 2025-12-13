#include "pager.h"
#include "logic/pager_service.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#include <stdlib.h>
#include <string.h>

/* UI State */
static GtkWidget *pager_drawing_area = NULL;
static int active_desktop = 0;
static int num_desktops = 1;
static PagerClickCallback click_callback = NULL;
static gpointer click_callback_data = NULL;

static gboolean on_pager_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    if (!widget) return FALSE;
    
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    /* Refresh data from service */
    num_desktops = pager_svc_get_num_desktops();
    active_desktop = pager_svc_get_current_desktop();
    
    /* Calculate Desktop Dimensions */
    int desk_width = width / (num_desktops > 0 ? num_desktops : 1);
    int desk_height = height;
    
    /* Background */
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.8);
    cairo_paint(cr);
    
    /* Get Screen Geometry for Scaling */
    GdkScreen *screen = gtk_widget_get_screen(widget);
    int screen_w = gdk_screen_get_width(screen);
    int screen_h = gdk_screen_get_height(screen);
    
    /* Fallback if screen size is invalid */
    if (screen_w <= 0) screen_w = 1920;
    if (screen_h <= 0) screen_h = 1080;
    
    float scale_x = (float)desk_width / screen_w;
    float scale_y = (float)desk_height / screen_h;
    
    for (int i = 0; i < num_desktops; i++) {
        int x_offset = i * desk_width;
        int y_offset = 0;
        
        /* Draw Desktop Box */
        if (i == active_desktop) {
            cairo_set_source_rgba(cr, 0.3, 0.3, 0.4, 0.6);
            cairo_rectangle(cr, x_offset + 1, y_offset + 1, desk_width - 2, desk_height - 2);
            cairo_fill(cr);
            
            cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
            cairo_rectangle(cr, x_offset + 1, y_offset + 1, desk_width - 2, desk_height - 2);
            cairo_stroke(cr);
        } else {
            cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.4);
            cairo_rectangle(cr, x_offset + 1, y_offset + 1, desk_width - 2, desk_height - 2);
            cairo_fill(cr);
            
            cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5);
            cairo_rectangle(cr, x_offset + 1, y_offset + 1, desk_width - 2, desk_height - 2);
            cairo_stroke(cr);
        }
        
        /* Draw Window Previews */
        GList *windows = pager_svc_get_windows(i);
        
        cairo_save(cr);
        /* Clip to desktop area */
        cairo_rectangle(cr, x_offset + 1, y_offset + 1, desk_width - 2, desk_height - 2);
        cairo_clip(cr);
        
        for (GList *l = windows; l != NULL; l = l->next) {
             Window win = GPOINTER_TO_INT(l->data);
             int wx, wy, ww, wh;
             
             if (pager_svc_get_window_geometry(win, &wx, &wy, &ww, &wh)) {
                 /* Scale to Pager Coordinates */
                 int px = x_offset + (int)(wx * scale_x);
                 int py = y_offset + (int)(wy * scale_y);
                 int pw = (int)(ww * scale_x);
                 int ph = (int)(wh * scale_y);
                 
                 /* Min visible size */
                 if (pw < 4) pw = 4;
                 if (ph < 4) ph = 4;
                 
                 /* Live GPU Render: Get XComposite Pixmap */
                 Pixmap pm = pager_svc_get_pixmap(win);
                 gboolean drawn = FALSE;
                 
                 if (pm != None) {
                     Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
                     
                     /* Need Visual for Surface */
                     XWindowAttributes win_attrs;
                     if (XGetWindowAttributes(dpy, win, &win_attrs)) {
                         cairo_surface_t *surf = cairo_xlib_surface_create(dpy, pm, win_attrs.visual, win_attrs.width, win_attrs.height);
                         if (surf) {
                             cairo_save(cr);
                             
                             /* Translate to position */
                             cairo_translate(cr, px, py);
                             
                             /* Scale down */
                             double sx = (double)pw / win_attrs.width;
                             double sy = (double)ph / win_attrs.height;
                             cairo_scale(cr, sx, sy);
                             
                             cairo_set_source_surface(cr, surf, 0, 0);
                             cairo_paint(cr);
                             
                             cairo_surface_destroy(surf);
                             cairo_restore(cr);
                             drawn = TRUE;
                         }
                     }
                     XFreePixmap(dpy, pm);
                 }
                 
                 if (!drawn) {
                     /* Fallback: Elegant Placeholder */
                     cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.8);
                     cairo_rectangle(cr, px, py, pw, ph);
                     cairo_fill(cr);
                     
                     /* Titlebar hint */
                     cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 1.0);
                     cairo_rectangle(cr, px, py, pw, MAX(ph * 0.15, 4));
                     cairo_fill(cr);
                 }
                 
                 /* Border */
                 cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 0.8);
                 cairo_set_line_width(cr, 1);
                 cairo_rectangle(cr, px, py, pw, ph);
                 cairo_stroke(cr);
             }
        }
        g_list_free(windows);
        
        cairo_restore(cr);
    }
    
    return FALSE;
}

static gboolean on_pager_click(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)data;
    int width = gtk_widget_get_allocated_width(widget);
    if (!num_desktops) num_desktops = 1;
    int desk_width = width / num_desktops;
    
    int index = event->x / desk_width;
    if (index >= 0 && index < num_desktops) {
        pager_svc_set_desktop(index);
        /* User callback */
        if (click_callback) {
            click_callback(index, click_callback_data);
        }
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

void pager_init(Display *dpy, Window root) {
    pager_svc_init(dpy, root);
}

static void on_pager_destroy(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    pager_drawing_area = NULL;
}

GtkWidget *pager_create_widget(void) {
    if (pager_drawing_area) return pager_drawing_area;
    
    pager_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(pager_drawing_area, 240, 160);
    g_signal_connect(pager_drawing_area, "draw", G_CALLBACK(on_pager_draw), NULL);
    g_signal_connect(pager_drawing_area, "destroy", G_CALLBACK(on_pager_destroy), NULL);
    
    gtk_widget_add_events(pager_drawing_area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(pager_drawing_area, "button-press-event", G_CALLBACK(on_pager_click), NULL);
    
    return pager_drawing_area;
}

void pager_update(void) {
    /* Invalidates draw */
    if (pager_drawing_area) {
        gtk_widget_queue_draw(pager_drawing_area);
    }
}

void pager_set_click_callback(PagerClickCallback cb, gpointer data) {
    click_callback = cb;
    click_callback_data = data;
}
