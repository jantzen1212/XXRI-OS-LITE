/* xxri-dialog - the dialog every XXRI shell tool uses.
 *
 *   xxri-dialog ask   "Title" "Body"  "Primary" "Secondary"   -> exit 0 / 1
 *   xxri-dialog info  "Title" "Body"  ["Primary"]             -> exit 0 / 1
 *   xxri-dialog error "Title" "Body"  ["Primary"]             -> exit 0 / 1
 *
 * Enter = primary (exit 0), Escape = secondary/close (exit 1).
 * A GtkMessageDialog styled by xxri-dialog.h, same as in the apps.
 */
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include "xxri-dialog.h"

/* the dark appearance: the same card, in charcoal */
static const char DARK_CSS[] =
    "@define-color xxri_dlg_bg    #2A2B33;"
    "@define-color xxri_dlg_ink   #F2F2F4;"
    "@define-color xxri_dlg_muted #A0A1AA;"
    "@define-color xxri_dlg_line  alpha(#FFFFFF, 0.10);"
    "@define-color xxri_dlg_btn   #3C3D47;"
    "@define-color xxri_dlg_btn_h #454650;";

/* flwm is focus-follows-mouse: grab the keyboard so Enter/Escape always work */
static gboolean on_map(GtkWidget* w, GdkEvent* e, gpointer d) {
    (void)e; (void)d;
    GdkSeat* seat = gdk_display_get_default_seat(gtk_widget_get_display(w));
    gdk_seat_grab(seat, gtk_widget_get_window(w), GDK_SEAT_CAPABILITY_KEYBOARD,
                  TRUE, NULL, NULL, NULL, NULL);
    return FALSE;
}

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);
    if (argc < 4) {
        fprintf(stderr, "usage: xxri-dialog ask|info|error TITLE BODY [PRIMARY [SECONDARY]]\n");
        return 2;
    }
    const char* mode  = argv[1];
    const char* title = argv[2];
    const char* body  = argv[3];
    gboolean    ask   = !strcmp(mode, "ask");
    const char* prim  = argc > 4 ? argv[4] : (ask ? "Yes" : "OK");
    const char* sec   = argc > 5 ? argv[5] : "Not now";

    xxri_dialogs_init();
    gboolean dark = FALSE;
    g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL);
    if (dark) {
        GtkCssProvider* css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(css, DARK_CSS, -1, NULL);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    GtkWidget* d = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
        !strcmp(mode, "error") ? GTK_MESSAGE_ERROR : ask ? GTK_MESSAGE_QUESTION : GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE, "%s", title);
    if (*body)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", body);
    if (ask) gtk_dialog_add_button(GTK_DIALOG(d), sec, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(d), prim, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    gtk_window_set_title(GTK_WINDOW(d), "");
    gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);
    gtk_window_set_keep_above(GTK_WINDOW(d), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(d), TRUE);
    g_signal_connect(d, "map-event", G_CALLBACK(on_map), NULL);

    int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    return r == GTK_RESPONSE_OK ? 0 : 1;
}
