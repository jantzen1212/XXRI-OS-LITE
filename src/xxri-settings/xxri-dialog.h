/* xxri-dialog.h - every GtkDialog of the process (incl. libfm's in Files)
 * becomes an undecorated window with a rounded card inside; menus get real
 * rounded corners too. Compositor: ARGB visual, real shadow. Without one the
 * card is cut out with the Shape extension (aliased, no shadow).
 *
 *   xxri_dialogs_init();      once, before the first dialog is created
 *
 * Colours are the named xxri_dlg_* colours, an app's dark sheet redefines them.
 * Identical copies live in settings, store, media, installer and Files -
 * keep them in sync. */
#ifndef XXRI_DIALOG_H
#define XXRI_DIALOG_H
#include <gtk/gtk.h>

/* shadow room: GTK's blur reaches ~1.4x the CSS radius, 20px blur + 5px offset */
#define XD_MARGIN 36
#define XD_RADIUS 14

/* fallback priority, so app sheets can redefine them */
static const char XD_CSS_COLORS[] =
    "@define-color xxri_dlg_bg    #FFFFFF;"
    "@define-color xxri_dlg_ink   #1C1C1E;"
    "@define-color xxri_dlg_muted #6B6B73;"
    "@define-color xxri_dlg_line  alpha(#000000, 0.10);"
    "@define-color xxri_dlg_btn   #ECECEF;"
    "@define-color xxri_dlg_btn_h #E2E2E6;";

static const char XD_CSS[] =
    /* the window itself is only the room the shadow falls into */
    ".xxri-dialog, .xxri-dialog.background {"
    "  background-color: transparent; background-image: none;"
    "  border: none; box-shadow: none; }"
    ".xxri-dialog > box.dialog-vbox {"
    "  background-color: @xxri_dlg_bg; background-image: none; color: @xxri_dlg_ink;"
    "  border: 1px solid @xxri_dlg_line; border-radius: 14px; padding: 8px; }"
    ".xxri-dialog.composited > box.dialog-vbox {"
    "  margin: 36px;"
    "  box-shadow: 0 5px 20px alpha(#000000, 0.22), 0 1px 3px alpha(#000000, 0.10); }"
    /* no compositor: the card is the whole (shaped) window */
    ".xxri-dialog.solid { background-color: @xxri_dlg_bg; }"
    ".xxri-dialog.solid > box.dialog-vbox { border-radius: 0; }"
    ".xxri-dialog label { color: @xxri_dlg_ink; }"
    ".xxri-dialog .xxri-dlg-heading {"
    "  font-weight: 700; font-size: 10.5pt; padding: 8px 10px 6px 10px; }"
    /* alerts: compact, centred, title over a quieter explanation */
    ".xxri-dialog.message > box.dialog-vbox { padding: 22px 16px 16px 16px; }"
    ".xxri-dialog .xxri-dlg-title { font-weight: 700; font-size: 10.5pt; }"
    ".xxri-dialog .xxri-dlg-text  { color: @xxri_dlg_muted; font-size: 9.5pt; }"
    ".xxri-dialog .dialog-action-box, .xxri-dialog .dialog-action-area {"
    "  background-color: transparent; background-image: none; border: none; }"
    ".xxri-dialog .dialog-action-area { padding: 8px 6px 4px 6px; }"
    ".xxri-dialog.message .dialog-action-area { padding: 0; }"
    /* the file chooser inside a card: quiet column headers, a places list
     * with room between icon and name */
    ".xxri-dialog treeview.view header button {"
    "  border: none; border-bottom: 1px solid @xxri_dlg_line; border-radius: 0;"
    "  box-shadow: none; background-image: none; background-color: transparent;"
    "  color: @xxri_dlg_muted; font-weight: 600; padding: 4px 8px; }"
    ".xxri-dialog placessidebar, .xxri-dialog placessidebar list {"
    "  background-color: transparent; border: none; }"
    ".xxri-dialog placessidebar row { padding: 3px 8px; min-height: 24px; border-radius: 8px; }"
    ".xxri-dialog placessidebar row .sidebar-icon { margin: 0 8px 0 2px; }"
    ".xxri-dialog paned > separator { background-color: @xxri_dlg_line; }"
    /* secondary actions are neutral, the one primary action is XXRI blue */
    ".xxri-dialog .dialog-action-area button {"
    "  min-height: 22px; min-width: 64px; padding: 5px 16px;"
    "  border: none; border-radius: 10px; box-shadow: none;"
    "  background-image: none; background-color: @xxri_dlg_btn;"
    "  color: @xxri_dlg_ink; font-weight: 600; font-size: 9.75pt; }"
    ".xxri-dialog .dialog-action-area button label { color: inherit; }"
    ".xxri-dialog .dialog-action-area button:hover  { background-color: @xxri_dlg_btn_h; }"
    ".xxri-dialog .dialog-action-area button:active { background-color: shade(@xxri_dlg_btn_h, 0.94); }"
    ".xxri-dialog .dialog-action-area button:disabled { opacity: 0.5; }"
    ".xxri-dialog .dialog-action-area button.suggested-action,"
    ".xxri-dialog .dialog-action-area button.xxri-accent {"
    "  background-image: linear-gradient(to right, #1E4FC2, #4A9AF5); color: #FFFFFF; }"
    ".xxri-dialog .dialog-action-area button.suggested-action:hover,"
    ".xxri-dialog .dialog-action-area button.xxri-accent:hover {"
    "  background-image: linear-gradient(to right, #2459D0, #5AA5F8); }"
    ".xxri-dialog .dialog-action-area button.suggested-action:active,"
    ".xxri-dialog .dialog-action-area button.xxri-accent:active {"
    "  background-image: linear-gradient(to right, #1A45AB, #3F8BE2); }"
    ".xxri-dialog .dialog-action-area button.destructive-action {"
    "  background-image: none; background-color: #E5484D; color: #FFFFFF; }"
    ".xxri-dialog .dialog-action-area button.destructive-action:hover {"
    "  background-color: #EC5A5E; }";

/* A menu's own window: nothing but the room its rounded corners need. */
static const char XD_CSS_MENU[] =
    ".xxri-popup, .xxri-popup.background {"
    "  background-color: transparent; background-image: none; border: none; box-shadow: none; }";

static void xd_class(GtkWidget* w, const char* c) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), c);
}
static gboolean xd_has(GtkWidget* w, const char* c) {
    return gtk_style_context_has_class(gtk_widget_get_style_context(w), c);
}

/* composited: only the card takes input; otherwise the window is cut to it */
static void xd_allocated(GtkWidget* w, GdkRectangle* a, gpointer d) {
    (void)d;
    if (!gtk_widget_get_realized(w)) return;
    if (xd_has(w, "composited")) {
        cairo_rectangle_int_t r = { XD_MARGIN, XD_MARGIN,
                                    a->width - 2 * XD_MARGIN, a->height - 2 * XD_MARGIN };
        if (r.width < 1 || r.height < 1) return;
        cairo_region_t* reg = cairo_region_create_rectangle(&r);
        gtk_widget_input_shape_combine_region(w, reg);
        cairo_region_destroy(reg);
        return;
    }
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_A1, a->width, a->height);
    cairo_t* cr = cairo_create(s);
    const double R = XD_RADIUS, W = a->width, H = a->height, Q = G_PI / 2;
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_new_sub_path(cr);
    cairo_arc(cr, W - R, R,     R, -Q, 0);
    cairo_arc(cr, W - R, H - R, R, 0, Q);
    cairo_arc(cr, R,     H - R, R, Q, 2 * Q);
    cairo_arc(cr, R,     R,     R, 2 * Q, 3 * Q);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_region_t* reg = gdk_cairo_region_create_from_surface(s);
    cairo_surface_destroy(s);
    gtk_widget_shape_combine_region(w, reg);
    cairo_region_destroy(reg);
}

/* no title bar, so dragging the card moves the dialog (only presses on the
 * window's own surface, widgets with their own window keep theirs) */
static gboolean xd_press(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)d;
    if (e->type != GDK_BUTTON_PRESS || e->button != 1) return FALSE;
    if (e->window != gtk_widget_get_window(w)) return FALSE;
    gtk_window_begin_move_drag(GTK_WINDOW(w), e->button,
                               (int)e->x_root, (int)e->y_root, e->time);
    return TRUE;
}

/* before realize: the visual can't change afterwards */
static void xxri_dialog_style(GtkWidget* w) {
    if (!GTK_IS_DIALOG(w) || g_object_get_data(G_OBJECT(w), "xxri-dialog")) return;
    g_object_set_data(G_OBJECT(w), "xxri-dialog", GINT_TO_POINTER(1));

    GdkScreen* scr  = gtk_widget_get_screen(w);
    GdkVisual* rgba = gdk_screen_get_rgba_visual(scr);
    gtk_window_set_decorated(GTK_WINDOW(w), FALSE);
    xd_class(w, "xxri-dialog");
    if (rgba && gdk_screen_is_composited(scr)) {
        gtk_widget_set_visual(w, rgba);
        xd_class(w, "composited");
    } else
        xd_class(w, "solid");
    gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(w, "size-allocate", G_CALLBACK(xd_allocated), NULL);
    g_signal_connect(w, "button-press-event", G_CALLBACK(xd_press), NULL);

    GtkWidget* box = gtk_dialog_get_content_area(GTK_DIALOG(w));
    if (GTK_IS_MESSAGE_DIALOG(w)) {
        /* the first label is the message, anything under it explains it */
        GtkWidget* area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(w));
        GList* kids = gtk_container_get_children(GTK_CONTAINER(area));
        for (GList* l = kids; l; l = l->next) {
            GtkWidget* c = GTK_WIDGET(l->data);
            xd_class(c, l == kids ? "xxri-dlg-title" : "xxri-dlg-text");
            if (GTK_IS_LABEL(c)) {
                gtk_label_set_max_width_chars(GTK_LABEL(c), 40);
                gtk_label_set_justify(GTK_LABEL(c), GTK_JUSTIFY_CENTER);
                gtk_label_set_xalign(GTK_LABEL(c), 0.5);
                gtk_widget_set_halign(c, GTK_ALIGN_CENTER);
            }
        }
        g_list_free(kids);
    } else {
        /* the window title becomes the card's heading */
        const char* t = gtk_window_get_title(GTK_WINDOW(w));
        GList* kids = gtk_container_get_children(GTK_CONTAINER(box));
        gboolean titled = FALSE;
        for (GList* l = kids; l; l = l->next)
            if (!g_strcmp0(gtk_widget_get_name(GTK_WIDGET(l->data)), "xxri-dlg-title"))
                titled = TRUE;
        g_list_free(kids);
        if (t && *t && !titled) {
            GtkWidget* h = gtk_label_new(t);
            xd_class(h, "xxri-dlg-heading");
            gtk_widget_set_halign(h, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(box), h, FALSE, FALSE, 0);
            gtk_box_reorder_child(GTK_BOX(box), h, 0);
            gtk_widget_show(h);
        }
    }

    /* one primary button: the app's own choice, else the affirmative response */
    static const int yes[] = { GTK_RESPONSE_OK, GTK_RESPONSE_YES, GTK_RESPONSE_ACCEPT,
                               GTK_RESPONSE_APPLY };
    GtkWidget* first = NULL; gboolean marked = FALSE; int buttons = 0;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkWidget* act = gtk_dialog_get_action_area(GTK_DIALOG(w));
    G_GNUC_END_IGNORE_DEPRECATIONS
    GList* kids = act ? gtk_container_get_children(GTK_CONTAINER(act)) : NULL;
    for (GList* l = kids; l; l = l->next) {
        GtkWidget* b = GTK_WIDGET(l->data);
        if (!GTK_IS_BUTTON(b)) continue;
        buttons++;
        if (xd_has(b, "suggested-action") || xd_has(b, "xxri-accent") ||
            xd_has(b, "destructive-action")) marked = TRUE;
    }
    g_list_free(kids);
    for (guint i = 0; !first && i < G_N_ELEMENTS(yes); i++)
        first = gtk_dialog_get_widget_for_response(GTK_DIALOG(w), yes[i]);
    if (!marked && first) xd_class(first, "suggested-action");
    if (GTK_IS_MESSAGE_DIALOG(w) && act && buttons > 1)
        gtk_box_set_spacing(GTK_BOX(act), 8);
}

/* a rounded menu in an opaque window shows square corners: ARGB visual for
 * the menu's toplevel before it is realized */
static void (*xd_menu_show)(GtkWidget*);
static void xd_menu_shown(GtkWidget* m) {
    GtkWidget* top = gtk_widget_get_toplevel(m);
    if (GTK_IS_WINDOW(top) && top != m && !gtk_widget_get_realized(top) &&
        !xd_has(top, "xxri-popup")) {
        GdkScreen* scr  = gtk_widget_get_screen(top);
        GdkVisual* rgba = gdk_screen_get_rgba_visual(scr);
        if (rgba && gdk_screen_is_composited(scr)) {
            gtk_widget_set_visual(top, rgba);
            xd_class(top, "xxri-popup");
        }
    }
    xd_menu_show(m);
}

static void (*xd_window_realize)(GtkWidget*);
static void xd_realize(GtkWidget* w) {
    xxri_dialog_style(w);
    xd_window_realize(w);
}

/* Hook GtkDialogClass->realize so library dialogs are caught too. Subclasses
 * copy the slot when first used, hence "before the first dialog". */
static void xxri_dialogs_init(void) {
    static gboolean done;
    if (done) return;
    done = TRUE;

    GdkScreen* scr = gdk_screen_get_default();
    GtkCssProvider* colors = gtk_css_provider_new();
    gtk_css_provider_load_from_data(colors, XD_CSS_COLORS, -1, NULL);
    gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(colors),
                                              GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, XD_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 50);

    GtkCssProvider* menu_css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(menu_css, XD_CSS_MENU, -1, NULL);
    gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(menu_css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 50);

    GtkWidgetClass* cls = GTK_WIDGET_CLASS(g_type_class_ref(GTK_TYPE_DIALOG));
    xd_window_realize = cls->realize;
    cls->realize = xd_realize;
    cls = GTK_WIDGET_CLASS(g_type_class_ref(GTK_TYPE_MENU));
    xd_menu_show = cls->show;
    cls->show = xd_menu_shown;
}

#endif
