#include "pager.h"
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Global Pager State */
static Display *x_display = NULL;
static Window root_window;
static GtkWidget *pager_box = NULL;
static int current_desktop = 0;
static int num_desktops = 0;
static Atom _net_number_of_desktops;
static Atom _net_current_desktop;
static Atom _net_client_list;
static Atom _net_client_list_stacking;
static Atom _net_wm_desktop;

/* Callback Handling */
static PagerClickCallback click_callback = NULL;
static gpointer click_callback_data = NULL;

void pager_set_click_callback(PagerClickCallback callback, gpointer user_data) {
    click_callback = callback;
    click_callback_data = user_data;
}

/* Helper to get X11 property value (Cardinal/Int) */
static long get_x11_property(Window win, Atom atom) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    long value = -1;

    if (XGetWindowProperty(x_display, win, atom, 0, 1, False,
                           XA_CARDINAL, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            value = *(long *)prop;
        }
        if (prop) XFree(prop);
    }
    return value;
}

/* Get thumbnail data for a window using nearest neighbor scaling */
static GdkPixbuf *get_window_thumbnail(Window win, int dest_w, int dest_h) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(x_display, win, &attrs)) return NULL;
    if (attrs.map_state != IsViewable) return NULL;

    /* Skip small windows or weird geometry */
    if (attrs.width < 10 || attrs.height < 10) return NULL;

    /* Capture window content */
    /* Note: XGetImage can be slow, but for thumbnails it's acceptable if specific opt is not avail */
    /* Use Error Trap in case window vanishes */
    gdk_x11_display_error_trap_push(gdk_display_get_default());
    XImage *ximage = XGetImage(x_display, win, 0, 0, attrs.width, attrs.height, AllPlanes, ZPixmap);
    gdk_x11_display_error_trap_pop_ignored(gdk_display_get_default());

    if (!ximage) return NULL;

    /* Create source pixbuf from XImage data */
    /* Assuming standard RGB layout (simplicity for X11) - robust conversions are complex */
    /* For "Visual Excellence", we assume a common TrueColor visual for now */
    
    /* Manual Downscaling (Nearest Neighbor) */
    /* We create the dest pixbuf directly to avoid full alloc */
    GdkPixbuf *dest = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, dest_w, dest_h);
    if (!dest) {
        XDestroyImage(ximage);
        return NULL;
    }

    guchar *pixels = gdk_pixbuf_get_pixels(dest);
    int rowstride = gdk_pixbuf_get_rowstride(dest);
    int n_channels = gdk_pixbuf_get_n_channels(dest);

    float scale_x = (float)attrs.width / dest_w;
    float scale_y = (float)attrs.height / dest_h;

    for (int y = 0; y < dest_h; y++) {
        for (int x = 0; x < dest_w; x++) {
            int src_x = (int)(x * scale_x);
            int src_y = (int)(y * scale_y);

            /* Safety clamp */
            if (src_x >= attrs.width) src_x = attrs.width - 1;
            if (src_y >= attrs.height) src_y = attrs.height - 1;

            unsigned long pixel = XGetPixel(ximage, src_x, src_y);
            
            /* Extract RGB from pixel - assumes TrueColor/standard shifting */
            /* In production code, check visual masks! */
            int r = (pixel >> 16) & 0xFF;
            int g = (pixel >> 8) & 0xFF;
            int b = pixel & 0xFF;

            guchar *p = pixels + y * rowstride + x * n_channels;
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = 255; /* Full opacity */
        }
    }

    XDestroyImage(ximage);
    return dest;
}

/* Draw function for each workspace preview */
static gboolean on_workspace_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    int desktop_idx = GPOINTER_TO_INT(data);
    
    /* Style Context */
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    int widget_w = gtk_widget_get_allocated_width(widget);
    int widget_h = gtk_widget_get_allocated_height(widget);

    /* Get Screen Geometry for Scaling */
    GdkScreen *screen = gtk_widget_get_screen(widget);
    int screen_w = gdk_screen_get_width(screen);
    int screen_h = gdk_screen_get_height(screen);
    
    if (screen_w <= 0) screen_w = 1920; /* Fallback */
    if (screen_h <= 0) screen_h = 1080;

    float scale_x = (float)widget_w / screen_w;
    float scale_y = (float)widget_h / screen_h;

    /* Draw Background */
    gtk_render_background(context, cr, 0, 0, widget_w, widget_h);

    /* Active Desktop Highlight */
    if (desktop_idx == current_desktop) {
        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3); /* Blue accent */
        cairo_rectangle(cr, 0, 0, widget_w, widget_h);
        cairo_fill(cr);
    }

    /* Iterate windows using STACKING list (Bottom-to-Top) */
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    /* Try Stacking List first, fallback to normal Client List */
    Atom list_atom = _net_client_list_stacking;
    if (XGetWindowProperty(x_display, root_window, list_atom, 0, 1024, False,
                           XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after, &prop) != Success) {
        list_atom = _net_client_list;
        XGetWindowProperty(x_display, root_window, list_atom, 0, 1024, False,
                           XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    }

    if (prop) {
        Window *list = (Window *)prop;
        for (unsigned long i = 0; i < nitems; i++) {
            Window win = list[i];
            
            /* Check desktop */
            long win_desktop = get_x11_property(win, _net_wm_desktop);
            
            /* Show on all (-1) or specific desktop */
            if (win_desktop == desktop_idx || win_desktop == 0xFFFFFFFF) {
                
                /* Get Real Geometry */
                XWindowAttributes attrs;
                if (XGetWindowAttributes(x_display, win, &attrs) && attrs.map_state == IsViewable) {
                    /* Skip very small windows (utility/hidden) */
                    if (attrs.width < 50 || attrs.height < 50) continue;

                    /* Calculate Pager Geometry */
                    int px = (int)(attrs.x * scale_x);
                    int py = (int)(attrs.y * scale_y);
                    int pw = (int)(attrs.width * scale_x);
                    int ph = (int)(attrs.height * scale_y);
                    
                    /* Clamp minimum visibility */
                    if (pw < 4) pw = 4;
                    if (ph < 4) ph = 4;

                    /* Get Thumbnail scaled to exact proportional size */
                    GdkPixbuf *thumb = get_window_thumbnail(win, pw, ph);
                    if (thumb) {
                        gdk_cairo_set_source_pixbuf(cr, thumb, px, py);
                        cairo_paint(cr);
                        g_object_unref(thumb);
                        
                        /* Draw tiny Border around window for definition */
                        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.2);
                        cairo_set_line_width(cr, 1);
                        cairo_rectangle(cr, px + 0.5, py + 0.5, pw - 1, ph - 1);
                        cairo_stroke(cr);
                    }
                }
            }
        }
        XFree(prop);
    }
    
    /* Draw Pager Border */
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, 0.5, 0.5, widget_w - 1, widget_h - 1);
    cairo_stroke(cr);

    return FALSE;
}

/* Click handler to switch desktop */
static gboolean on_workspace_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
        int desktop_idx = GPOINTER_TO_INT(data);
        
        XEvent xev;
        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.xclient.window = root_window;
        xev.xclient.message_type = _net_current_desktop;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = desktop_idx;
        xev.xclient.data.l[1] = CurrentTime;
        
        XSendEvent(x_display, root_window, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(x_display);
        
        /* Trigger Callback */
        if (click_callback) {
            click_callback(desktop_idx, click_callback_data);
        }
        
        return TRUE;
    }
    return FALSE;
}

void pager_init(Display *display, Window root) {
    x_display = display;
    root_window = root;
    
    _net_number_of_desktops = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
    _net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    _net_client_list = XInternAtom(display, "_NET_CLIENT_LIST", False);
    _net_client_list_stacking = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False);
    _net_wm_desktop = XInternAtom(display, "_NET_WM_DESKTOP", False);
}

void pager_update(void) {
    if (!pager_box) return;

    /* Update States */
    long d = get_x11_property(root_window, _net_current_desktop);
    if (d >= 0) current_desktop = d;
    
    long n = get_x11_property(root_window, _net_number_of_desktops);
    if (n > 0 && n != num_desktops) {
        /* Re-create widgets if number changes */
        num_desktops = n;
        
        /* Clear existing */
        GList *children = gtk_container_get_children(GTK_CONTAINER(pager_box));
        g_list_free_full(children, (GDestroyNotify)gtk_widget_destroy);
        
        for (int i = 0; i < num_desktops; i++) {
            GtkWidget *da = gtk_drawing_area_new();
            /* Double the size: 60x40 -> 120x80 */
            gtk_widget_set_size_request(da, 240, 160);
            gtk_widget_set_margin_start(da, 4);
            gtk_widget_set_margin_end(da, 4);
            
            /* Enable events */
            gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK);
            
            g_signal_connect(da, "draw", G_CALLBACK(on_workspace_draw), GINT_TO_POINTER(i));
            g_signal_connect(da, "button-press-event", G_CALLBACK(on_workspace_clicked), GINT_TO_POINTER(i));
            
            gtk_box_pack_start(GTK_BOX(pager_box), da, FALSE, FALSE, 0);
        }
        gtk_widget_show_all(pager_box);
    }
    
    /* Queue redraw for all previews */
    gtk_widget_queue_draw(pager_box);
}

static void on_pager_destroy(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    pager_box = NULL;
    num_desktops = 0; /* Force re-creation of widgets next time */
}

GtkWidget *pager_create_widget(void) {
    if (pager_box) return pager_box; /* Singleton-ish, or just return existing if active */
    
    pager_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_name(pager_box, "workspace-pager");
    
    g_signal_connect(pager_box, "destroy", G_CALLBACK(on_pager_destroy), NULL);
    
    /* Initial Populate */
    pager_update();
    
    return pager_box;
}
