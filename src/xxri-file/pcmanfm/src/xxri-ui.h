/*
 *      xxri-ui.h
 *
 *      XXRI File - XXRI OS Lite shell for the PCManFM main window.
 *
 *      This is the XXRI-specific user interface layer of the fork.  Stock
 *      PCManFM builds a menubar + toolbar + statusbar around a notebook of
 *      side-pane/folder-view panes.  The XXRI design (assets/mockup) instead
 *      puts every navigation control into a translucent left rail and gives
 *      the content column a wordmark, a location pill and filter chips.
 *      Keeping that here, rather than editing main-win.c line by line, keeps
 *      the upstream window code recognisable and easy to rebase.
 */

#ifndef __XXRI_UI_H__
#define __XXRI_UI_H__

#include <gtk/gtk.h>
#include <libfm/fm-gtk.h>

G_BEGIN_DECLS

typedef struct _FmMainWin FmMainWin;

/* Give the window an ARGB visual so the rail can be translucent. */
void xxri_ui_enable_alpha(GtkWidget* win);

/* Load /usr/local/share/xxri-file/xxri-file.css once per process. */
void xxri_ui_init_style(void);

/* Build the translucent left rail: window controls, menu, search and the
 * icon-only Places list (Home, Desktop, Documents, Downloads, Pictures,
 * Music, Videos, Computer + mounted volumes). */
GtkWidget* xxri_ui_build_rail(FmMainWin* win);

/* Wrap the window root so the undecorated window gains invisible resize
 * edges (no border, no padding, appearance unchanged). */
GtkWidget* xxri_ui_wrap_root(FmMainWin* win, GtkWidget* root);

/* Build the content header: "XXRI File" wordmark, location pill and chips. */
GtkWidget* xxri_ui_build_header(FmMainWin* win);

/* Give libfm's folder view the XXRI skin (no column headers, transparent). */
void xxri_ui_style_folder_view(GtkWidget* view);

/* Refresh the location pill (folder name + total size) and rail selection. */
void xxri_ui_update_location(FmMainWin* win, FmPath* path);
void xxri_ui_update_size(FmMainWin* win, const char* size_text);

/* TRUE for locations XXRI hides from ordinary users (/proc, /sys, /dev...).
 * Deliberate navigation (typing the path) is still honoured. */
gboolean xxri_ui_path_is_system(FmPath* path);

/* Folder-model filter built on the above; hides system trees from listings. */
gboolean xxri_ui_model_filter(FmFileInfo* file, gpointer user_data);

G_END_DECLS

#endif /* __XXRI_UI_H__ */
