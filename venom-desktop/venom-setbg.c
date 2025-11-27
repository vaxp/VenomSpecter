#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <Imlib2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <image_path>\n", argv[0]);
        return 1;
    }

    char *image_path = argv[1];
    Display *disp = XOpenDisplay(NULL);
    
    if (!disp) {
        fprintf(stderr, "Cannot open X display!\n");
        return 1;
    }

    int scr = DefaultScreen(disp);
    Window root = RootWindow(disp, scr);
    
    /* إعداد Imlib2 للعمل مع شاشتنا */
    imlib_context_set_display(disp);
    imlib_context_set_visual(DefaultVisual(disp, scr));
    imlib_context_set_colormap(DefaultColormap(disp, scr));

    /* تحميل الصورة */
    Imlib_Image image = imlib_load_image(image_path);
    if (!image) {
        fprintf(stderr, "Failed to load image: %s\n", image_path);
        XCloseDisplay(disp);
        return 1;
    }

    imlib_context_set_image(image);

    /* الحصول على أبعاد الشاشة */
    int width = DisplayWidth(disp, scr);
    int height = DisplayHeight(disp, scr);

    /* إنشاء Pixmap (مساحة في ذاكرة X Server) */
    Pixmap pixmap = XCreatePixmap(disp, root, width, height, DefaultDepth(disp, scr));

    /* رسم الصورة على الـ Pixmap مع تغيير الحجم لملء الشاشة */
    imlib_context_set_drawable(pixmap);
    imlib_render_image_on_drawable_at_size(0, 0, width, height);

    /* تعيين الـ Pixmap كخلفية للـ Root Window */
    XSetWindowBackgroundPixmap(disp, root, pixmap);
    XClearWindow(disp, root);

    /* --- الخطوات السحرية للتوافق مع الشفافية --- */
    /* هذه الخصائص تخبر البرامج الأخرى (مثل التيرمينال الشفاف) أن هناك خلفية */
    
    Atom prop_root = XInternAtom(disp, "_XROOTPMAP_ID", False);
    Atom prop_eset = XInternAtom(disp, "ESETROOT_PMAP_ID", False);

    /* تعيين الخصائص */
    XChangeProperty(disp, root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pixmap, 1);
    XChangeProperty(disp, root, prop_eset, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pixmap, 1);

    /* تنظيف وإغلاق */
    /* ملاحظة: الـ Pixmap يبقى في X Server حتى نغيره، لكن برنامجنا ينتهي */
    imlib_free_image();
    XSetCloseDownMode(disp, RetainPermanent); /* مهم: أخبر X أن يحتفظ بالمصادر بعد خروجنا */
    XCloseDisplay(disp);

    return 0;
}