/*
 *      xxri-ui.c
 *
 *      XXRI File - XXRI OS Lite shell for the PCManFM main window.
 *      See xxri-ui.h for why this lives outside main-win.c.
 *
 *      Layout target (assets/mockup/mockup1.jpg):
 *
 *        +------+--------------------------------------------+
 *        | rail |  XXRI File                                 |
 *        | /\[] |  ( Downloads                    3.2 MiB )  |
 *        |  X   |  [All] [Browser]                           |
 *        |  =   |  This week .............................   |
 *        |  Q   |  [thumb] name / source / date              |
 *        |  v   |                                            |
 *        +------+--------------------------------------------+
 */

#include "xxri-ui.h"
#include "main-win.h"
#include "pcmanfm.h"

#include <glib/gi18n.h>
#include <string.h>

/* same window controls as Settings and Store */
#include "xxri-chrome.h"
#include "xxri-dialog.h"
#include "xxri-theme.h"

static void xxri_theme_changed(gboolean dark, gpointer data);

/* One chrome per window, owned by the window. */
static XxriChrome* xxri_get_chrome(FmMainWin* win)
{
    XxriChrome* c = g_object_get_data(G_OBJECT(win), "xxri-chrome");

    if(!c)
    {
        c = xxri_chrome_new(GTK_WIDGET(win));
        g_object_set_data_full(G_OBJECT(win), "xxri-chrome", c, g_free);
    }
    return c;
}

GtkWidget* xxri_ui_wrap_root(FmMainWin* win, GtkWidget* root)
{
    return xxri_chrome_resizable(xxri_get_chrome(win), root);
}

/* active filter chip, applied by the folder-model filter */
typedef enum { XXRI_FILTER_ALL, XXRI_FILTER_FOLDERS } XxriFilter;
static XxriFilter xxri_filter_mode = XXRI_FILTER_ALL;

/* ------------------------------------------------------------------ style */

void xxri_ui_init_style(void)
{
    static gboolean done = FALSE;
    GtkCssProvider* prov;
    GdkScreen* screen;
    GError* err = NULL;
    const char* css = "/usr/local/share/xxri-file/xxri-file.css";

    if(done)
        return;
    done = TRUE;

    screen = gdk_screen_get_default();
    if(!screen)
        return;
    prov = gtk_css_provider_new();
    if(gtk_css_provider_load_from_path(prov, css, &err))
    {
        gtk_style_context_add_provider_for_screen(screen,
                GTK_STYLE_PROVIDER(prov),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    else
    {
        /* not fatal, it just looks stock */
        g_warning("XXRI File: cannot load %s: %s", css,
                  err ? err->message : "unknown error");
        g_clear_error(&err);
    }
    g_object_unref(prov);
    /* Light or Dark as in Settings, switching while open */
    xxri_theme_init("/usr/local/share/xxri-file/xxri-file-dark.css", xxri_theme_changed, NULL);
    /* every dialog - libfm's included - as an XXRI card */
    xxri_dialogs_init();
}

/* --------------------------------------------------- hidden system places */

/* kept out of listings and shortcuts; typing the path still works */
static const char* const xxri_system_paths[] = {
    "/proc", "/sys", "/dev", "/run", "/tmp/.X11-unix", "/lost+found", NULL
};

gboolean xxri_ui_path_is_system(FmPath* path)
{
    char* str;
    guint i;
    gboolean hit = FALSE;

    if(!path)
        return FALSE;
    str = fm_path_to_str(path);
    if(!str)
        return FALSE;
    for(i = 0; xxri_system_paths[i]; ++i)
    {
        gsize len = strlen(xxri_system_paths[i]);
        if(strncmp(str, xxri_system_paths[i], len) == 0
           && (str[len] == '\0' || str[len] == '/'))
        {
            hit = TRUE;
            break;
        }
    }
    g_free(str);
    return hit;
}

/* -------------------------------------------------------------- the rail */

typedef struct {
    const char* label;      /* tooltip - the rail is icon-only, as drawn */
    const char* asset;      /* file in XXRI_ICON_DIR; NULL marks a rule */
    gint        dir;        /* GUserDirectory, or one of the codes below */
} XxriPlace;

#define XXRI_PLACE_HOME     (-1)
#define XXRI_PLACE_COMPUTER (-2)
/* not a location at all - opens Settings' existing Storage Usage page */
#define XXRI_PLACE_STORAGE_UI (-3)

/* the mockup's rail, in its order. No Home/Desktop entry (no asset, not in the
 * mockup); Home is where the window opens and is in the rail menu. */
static const XxriPlace xxri_places_tbl[] = {
    { NULL, NULL, 0 },                                   /* dotted rule */
    { N_("Downloads"), "slice30.png",        G_USER_DIRECTORY_DOWNLOAD },
    { N_("Pictures"),  "Photos.png",         G_USER_DIRECTORY_PICTURES },
    { N_("Music"),     "music.png",          G_USER_DIRECTORY_MUSIC },
    { N_("Videos"),    "Videos.png",         G_USER_DIRECTORY_VIDEOS },
    { N_("Documents"), "Documents.png",      G_USER_DIRECTORY_DOCUMENTS },
    { NULL, NULL, 0 },                                   /* dotted rule */
    { N_("Internal Storage"), "Internal-storage.png", XXRI_PLACE_HOME },
    { NULL, NULL, 0 },                                   /* dotted rule */
    { N_("Storage Usage"), "Storage-usage.png", XXRI_PLACE_STORAGE_UI },
};

/* NULL when the XDG dir isn't set up (fresh accounts often lack ~/Videos) */
static char* xxri_place_path(const XxriPlace* p)
{
    const char* d;

    if(p->dir == XXRI_PLACE_HOME)
        return g_strdup(g_get_home_dir());
    if(p->dir == XXRI_PLACE_COMPUTER)
        return g_strdup("/");
    d = g_get_user_special_dir((GUserDirectory)p->dir);
    if(!d || !*d)
        return NULL;
    /* unset falls back to $HOME; better no entry than a fake one */
    if(g_strcmp0(d, g_get_home_dir()) == 0)
        return NULL;
    return g_strdup(d);
}

static void on_rail_place_clicked(GtkButton* btn, FmMainWin* win)
{
    const char* path = g_object_get_data(G_OBJECT(btn), "xxri-path");

    if(path)
        fm_main_win_chdir_by_name(win, path);
}

/* Storage Usage is the Settings page, opened like the dock does */
static void on_rail_storage_clicked(GtkButton* btn, FmMainWin* win)
{
    const char* argv[] = { "/usr/local/bin/xxri-dock-launch", "xxri-settings",
                           "/usr/local/bin/xxri-settings", "storage", NULL };
    GError* err = NULL;

    if(!g_spawn_async(NULL, (char**)argv, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, &err))
    {
        g_warning("XXRI File: cannot open Storage Usage: %s",
                  err ? err->message : "?");
        g_clear_error(&err);
    }
}

static void on_rail_menu_clicked(GtkButton* btn, FmMainWin* win)
{
    GtkWidget* menu = gtk_ui_manager_get_widget(win->ui, "/XxriMenu");

    /* The mockup has no menubar; the hamburger carries the same actions. */
    if(menu)
        gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(btn),
                                 GDK_GRAVITY_NORTH_EAST, GDK_GRAVITY_NORTH_WEST,
                                 NULL);
}

#define XXRI_ICON_DIR "/usr/local/share/xxri-file/icons"

/* XXRI artwork rasterised by stage-icons.sh, never an icon theme lookup
 * (that's how Adwaita folders sneak in). Missing = warning + blank. */
/* an asset's file: NAME-dark.png while Mode is Dark, where one exists (the
 * near-black glyphs, tools/icons/files-dark.sh) */
static char* xxri_asset_path(const char* asset)
{
    if(xxri_theme_dark() && g_str_has_suffix(asset, ".png"))
    {
        char* base = g_strndup(asset, strlen(asset) - 4);
        char* dark = g_strdup_printf("%s/%s-dark.png", XXRI_ICON_DIR, base);
        g_free(base);
        if(g_file_test(dark, G_FILE_TEST_EXISTS))
            return dark;
        g_free(dark);
    }
    return g_build_filename(XXRI_ICON_DIR, asset, NULL);
}

static GtkWidget* xxri_asset_image(const char* asset)
{
    char* path = xxri_asset_path(asset);
    GtkWidget* img;

    if(g_file_test(path, G_FILE_TEST_EXISTS))
        img = gtk_image_new_from_file(path);
    else
    {
        g_warning("XXRI File: missing asset %s", path);
        img = gtk_image_new();
    }
    g_free(path);
    /* remembered, so a Light/Dark switch can swap the file */
    g_object_set_data_full(G_OBJECT(img), "xxri-asset", g_strdup(asset), g_free);
    return img;
}

/* Light/Dark switched in Settings: reload the artwork that has a twin */
static void xxri_reload_assets(GtkWidget* w, gpointer data)
{
    const char* asset = g_object_get_data(G_OBJECT(w), "xxri-asset");
    if(asset && GTK_IS_IMAGE(w))
    {
        char* path = xxri_asset_path(asset);
        gtk_image_set_from_file(GTK_IMAGE(w), path);
        g_free(path);
    }
    if(GTK_IS_CONTAINER(w))
        gtk_container_forall(GTK_CONTAINER(w), xxri_reload_assets, data);
}
static void xxri_theme_changed(gboolean dark, gpointer data)
{
    GList* tops = gtk_window_list_toplevels();
    GList* l;
    for(l = tops; l; l = l->next)
    {
        xxri_reload_assets(GTK_WIDGET(l->data), NULL);
        gtk_widget_queue_draw(GTK_WIDGET(l->data));
    }
    g_list_free(tops);
}

static void on_rail_refresh_clicked(GtkButton* btn, FmMainWin* win)
{
    GtkAction* act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Reload");

    if(act)
        gtk_action_activate(act);
}

/* Window dragging. Rail and header are plain boxes without a GdkWindow, which
 * xxri_chrome_drag_area() skips, so wrap them in an input-only event box. */
static gboolean on_drag_press(GtkWidget* w, GdkEventButton* e, gpointer data)
{
    FmMainWin* win = data;

    if(e->type == GDK_BUTTON_PRESS && e->button == 1)
    {
        gtk_window_begin_move_drag(GTK_WINDOW(win), e->button,
                                   (gint)e->x_root, (gint)e->y_root, e->time);
        return TRUE;
    }
    if(e->type == GDK_2BUTTON_PRESS && e->button == 1)
        return TRUE;
    return FALSE;
}

static GtkWidget* xxri_drag_handle(FmMainWin* win, GtkWidget* child)
{
    GtkWidget* eb = gtk_event_box_new();

    gtk_event_box_set_visible_window(GTK_EVENT_BOX(eb), FALSE);
    gtk_widget_add_events(eb, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(eb), child);
    g_signal_connect(eb, "button-press-event", G_CALLBACK(on_drag_press), win);
    return eb;
}

static GtkWidget* xxri_rail_button(const char* asset, const char* tip)
{
    GtkWidget* btn = gtk_button_new();

    gtk_button_set_image(GTK_BUTTON(btn), xxri_asset_image(asset));
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(btn, FALSE);
    gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
    if(tip)
        gtk_widget_set_tooltip_text(btn, tip);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn),
                                "xxri-rail-btn");
    return btn;
}

/* short dotted rule between rail groups */
static gboolean on_rail_sep_draw(GtkWidget* w, cairo_t* cr, gpointer data)
{
    static const double dots[] = { 1.7, 4.4 };
    int width = gtk_widget_get_allocated_width(w);
    int height = gtk_widget_get_allocated_height(w);

    if(xxri_theme_dark())
        cairo_set_source_rgba(cr, 0.949, 0.949, 0.957, 0.50);
    else
        cairo_set_source_rgba(cr, 0.11, 0.11, 0.14, 0.60);
    cairo_set_line_width(cr, 1.7);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_dash(cr, dots, 2, 0);
    cairo_move_to(cr, width * 0.30, height / 2.0);
    cairo_line_to(cr, width * 0.70, height / 2.0);
    cairo_stroke(cr);
    return FALSE;
}

static GtkWidget* xxri_rail_separator(void)
{
    GtkWidget* sep = gtk_drawing_area_new();

    gtk_widget_set_size_request(sep, -1, 13);
    gtk_style_context_add_class(gtk_widget_get_style_context(sep),
                                "xxri-rail-sep");
    g_signal_connect(sep, "draw", G_CALLBACK(on_rail_sep_draw), NULL);
    return sep;
}

/* top to bottom: window controls, menu, refresh, then xxri_places_tbl */
GtkWidget* xxri_ui_build_rail(FmMainWin* win)
{
    GtkWidget* rail = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* btn;
    guint i;

    /* no .xxri-rail here, the scroller below paints it (twice = darker).
     * 78px = the mockup's 10.8% of the 720px default width */
    gtk_widget_set_size_request(rail, 78, -1);

    {
        XxriChrome* chrome = xxri_get_chrome(win);
        GtkWidget* ctl = xxri_chrome_controls(chrome);
        GtkWidget* row = gtk_event_box_new();

        gtk_event_box_set_visible_window(GTK_EVENT_BOX(row), FALSE);
        gtk_container_add(GTK_CONTAINER(row), ctl);
        gtk_widget_set_margin_top(row, 9);
        gtk_widget_set_margin_bottom(row, 10);
        gtk_widget_set_margin_start(row, 12);
        gtk_box_pack_start(GTK_BOX(rail), row, FALSE, FALSE, 0);
    }

    btn = xxri_rail_button("menu.png", _("Menu"));
    g_signal_connect(btn, "clicked", G_CALLBACK(on_rail_menu_clicked), win);
    gtk_box_pack_start(GTK_BOX(rail), btn, FALSE, FALSE, 0);

    btn = xxri_rail_button("refresh.png", _("Refresh"));
    g_signal_connect(btn, "clicked", G_CALLBACK(on_rail_refresh_clicked), win);
    gtk_box_pack_start(GTK_BOX(rail), btn, FALSE, FALSE, 0);

    for(i = 0; i < G_N_ELEMENTS(xxri_places_tbl); ++i)
    {
        const XxriPlace* p = &xxri_places_tbl[i];
        char* path;

        if(!p->label)
        {
            gtk_box_pack_start(GTK_BOX(rail), xxri_rail_separator(),
                               FALSE, FALSE, 0);
            continue;
        }
        if(p->dir == XXRI_PLACE_STORAGE_UI)
        {
            btn = xxri_rail_button(p->asset, _(p->label));
            g_signal_connect(btn, "clicked",
                             G_CALLBACK(on_rail_storage_clicked), win);
            gtk_box_pack_start(GTK_BOX(rail), btn, FALSE, FALSE, 0);
            continue;   /* not a location, never "selected" */
        }
        path = xxri_place_path(p);
        if(!path)
            continue;
        btn = xxri_rail_button(p->asset, _(p->label));
        g_object_set_data_full(G_OBJECT(btn), "xxri-path", path, g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_rail_place_clicked), win);
        gtk_box_pack_start(GTK_BOX(rail), btn, FALSE, FALSE, 0);
        win->xxri_places = g_list_append(win->xxri_places, btn);
    }
    /* in a scroller, or the glyph column forces a ~500px minimum window height */
    {
        GtkWidget* sw = gtk_scrolled_window_new(NULL, NULL);

        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                       GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
        gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(sw),
                                                        TRUE);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                                            GTK_SHADOW_NONE);
        gtk_style_context_add_class(gtk_widget_get_style_context(sw),
                                    "xxri-rail");
        gtk_container_add(GTK_CONTAINER(sw), rail);
        /* empty rail space drags the window too */
        return xxri_drag_handle(win, sw);
    }
}

/* ------------------------------------------------------------- the header */

/* folder name in gradient text: dynamic, so drawn, not an asset. Colours from
 * the slice33 artwork. */
static gboolean on_locname_draw(GtkWidget* w, cairo_t* cr, gpointer data)
{
    const char* text = g_object_get_data(G_OBJECT(w), "xxri-text");
    PangoLayout* lay;
    PangoFontDescription* fd;
    cairo_pattern_t* grad;
    int tw = 0, th = 0;
    int height = gtk_widget_get_allocated_height(w);

    if(!text || !*text)
        return FALSE;
    fd = pango_font_description_from_string("Avenir Next Bold 11");
    lay = gtk_widget_create_pango_layout(w, text);
    pango_layout_set_font_description(lay, fd);
    pango_layout_get_pixel_size(lay, &tw, &th);

    grad = cairo_pattern_create_linear(0, 0, tw > 0 ? tw : 1, 0);
    cairo_pattern_add_color_stop_rgb(grad, 0.0, 0.859, 0.0, 1.0);   /* #DB00FF */
    cairo_pattern_add_color_stop_rgb(grad, 1.0, 0.114, 0.0, 1.0);   /* #1D00FF */
    cairo_move_to(cr, 0, (height - th) / 2.0);
    cairo_set_source(cr, grad);
    pango_cairo_show_layout(cr, lay);
    cairo_pattern_destroy(grad);
    g_object_unref(lay);
    pango_font_description_free(fd);
    gtk_widget_set_size_request(w, tw, th);
    return FALSE;
}

/* active "All" uses the chip artwork (the mockup's state); otherwise a label
 * chip, gradient when active, outlined when not */
static void xxri_chip_set_active(GtkWidget* btn, gboolean active)
{
    gboolean is_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),
                                                        "xxri-chip-all"));
    GtkStyleContext* sc = gtk_widget_get_style_context(btn);
    GtkWidget* child = gtk_bin_get_child(GTK_BIN(btn));

    if(child)
        gtk_widget_destroy(child);
    if(is_all && active)
    {
        gtk_container_add(GTK_CONTAINER(btn), xxri_asset_image("chip-all.png"));
        gtk_style_context_remove_class(sc, "xxri-chip");
        gtk_style_context_remove_class(sc, "active");
        gtk_style_context_add_class(sc, "xxri-chip-img");
    }
    else
    {
        gtk_container_add(GTK_CONTAINER(btn),
                          gtk_label_new(is_all ? _("All") : _("Folders")));
        gtk_style_context_remove_class(sc, "xxri-chip-img");
        gtk_style_context_add_class(sc, "xxri-chip");
        if(active)
            gtk_style_context_add_class(sc, "active");
        else
            gtk_style_context_remove_class(sc, "active");
    }
    gtk_widget_show_all(btn);
}

static void on_chip_clicked(GtkButton* btn, FmMainWin* win)
{
    XxriFilter want = (XxriFilter)GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(btn), "xxri-filter"));
    FmFolderModel* model;
    GList* kids, *k;

    if(want == xxri_filter_mode)
        return;
    xxri_filter_mode = want;

    kids = gtk_container_get_children(GTK_CONTAINER(win->xxri_chips));
    for(k = kids; k; k = k->next)
    {
        GtkWidget* c = GTK_WIDGET(k->data);
        XxriFilter f = (XxriFilter)GPOINTER_TO_INT(
                g_object_get_data(G_OBJECT(c), "xxri-filter"));

        xxri_chip_set_active(c, f == want);
    }
    g_list_free(kids);

    model = win->folder_view ? fm_folder_view_get_model(win->folder_view) : NULL;
    if(model)
        fm_folder_model_apply_filters(model);
}

static GtkWidget* xxri_chip_new(FmMainWin* win, gboolean is_all)
{
    GtkWidget* chip = gtk_button_new();

    gtk_button_set_relief(GTK_BUTTON(chip), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(chip, FALSE);
    g_object_set_data(G_OBJECT(chip), "xxri-chip-all",
                      GINT_TO_POINTER(is_all ? 1 : 0));
    g_object_set_data(G_OBJECT(chip), "xxri-filter",
                      GINT_TO_POINTER(is_all ? XXRI_FILTER_ALL
                                             : XXRI_FILTER_FOLDERS));
    g_signal_connect(chip, "clicked", G_CALLBACK(on_chip_clicked), win);
    xxri_chip_set_active(chip, is_all);   /* All is the default filter */
    return chip;
}

GtkWidget* xxri_ui_build_header(FmMainWin* win)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* title;
    GtkWidget* pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* chips = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget* chip;

    gtk_style_context_add_class(gtk_widget_get_style_context(box),
                                "xxri-file-header");

    title = xxri_asset_image("title.png");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_margin_start(title, 10);
    gtk_widget_set_margin_bottom(title, 16);
    /* the wordmark strip is the window's drag handle */
    gtk_box_pack_start(GTK_BOX(box), xxri_drag_handle(win, title),
                       FALSE, FALSE, 0);

    /* location pill: folder name left, total size right */
    gtk_style_context_add_class(gtk_widget_get_style_context(pill),
                                "xxri-locpill");
    gtk_widget_set_size_request(pill, -1, 40);

    win->xxri_loc_name = gtk_drawing_area_new();
    gtk_widget_set_halign(win->xxri_loc_name, GTK_ALIGN_START);
    gtk_widget_set_valign(win->xxri_loc_name, GTK_ALIGN_FILL);
    gtk_widget_set_size_request(win->xxri_loc_name, 10, -1);
    g_signal_connect(win->xxri_loc_name, "draw",
                     G_CALLBACK(on_locname_draw), NULL);

    win->xxri_loc_size = gtk_label_new(NULL);
    gtk_widget_set_halign(win->xxri_loc_size, GTK_ALIGN_END);
    gtk_style_context_add_class(
            gtk_widget_get_style_context(win->xxri_loc_size),
            "xxri-locpill-size");

    gtk_box_pack_start(GTK_BOX(pill), win->xxri_loc_name, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(pill), win->xxri_loc_size, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), pill, FALSE, FALSE, 0);

    win->xxri_chips = chips;
    chip = xxri_chip_new(win, TRUE);
    gtk_box_pack_start(GTK_BOX(chips), chip, FALSE, FALSE, 0);
    chip = xxri_chip_new(win, FALSE);
    gtk_box_pack_start(GTK_BOX(chips), chip, FALSE, FALSE, 0);
    gtk_widget_set_halign(chips, GTK_ALIGN_START);
    gtk_widget_set_margin_start(chips, 8);
    gtk_widget_set_margin_top(chips, 14);
    gtk_widget_set_margin_bottom(chips, 12);
    gtk_box_pack_start(GTK_BOX(box), chips, FALSE, FALSE, 0);

    return box;
}

static gboolean xxri_is_hidden_place(FmPath* path);

/* on the folder model, so it covers the view, keyboard navigation and
 * Computer alike */
gboolean xxri_ui_model_filter(FmFileInfo* file, gpointer user_data)
{
    FmPath* p;

    if(!file)
        return TRUE;
    p = fm_file_info_get_path(file);
    if(xxri_ui_path_is_system(p) || xxri_is_hidden_place(p))
        return FALSE;
    if(xxri_filter_mode == XXRI_FILTER_FOLDERS && !fm_file_info_is_dir(file))
        return FALSE;
    return TRUE;
}

void xxri_ui_update_location(FmMainWin* win, FmPath* path)
{
    char* name;
    char* cur;
    GList* l;

    if(!win->xxri_loc_name || !path)
        return;

    /* light the rail entry for the folder we are actually in */
    cur = fm_path_to_str(path);
    for(l = win->xxri_places; l; l = l->next)
    {
        GtkWidget* btn = GTK_WIDGET(l->data);
        const char* bp = g_object_get_data(G_OBJECT(btn), "xxri-path");
        GtkStyleContext* sc = gtk_widget_get_style_context(btn);

        if(bp && cur && g_strcmp0(bp, cur) == 0)
            gtk_style_context_add_class(sc, "selected");
        else
            gtk_style_context_remove_class(sc, "selected");
    }
    g_free(cur);
    /* just the folder name, not the path (mockup) */
    name = fm_path_display_basename(path);
    g_object_set_data_full(G_OBJECT(win->xxri_loc_name), "xxri-text",
                           name ? name : g_strdup(""), g_free);
    gtk_widget_queue_draw(win->xxri_loc_name);
}

void xxri_ui_update_size(FmMainWin* win, const char* size_text)
{
    if(win->xxri_loc_size)
        gtk_label_set_text(GTK_LABEL(win->xxri_loc_size),
                           size_text ? size_text : "");
}

/* ------------------------------------------------------- folder view skin */

/* item row height (24px glyph + text), see xxri_band_cell_data() */
static int xxri_row_h = 30;

static void xxri_band_cell_data(GtkTreeViewColumn* col, GtkCellRenderer* cell,
                                GtkTreeModel* model, GtkTreeIter* iter,
                                gpointer user_data);

/* ------------------------------------------------ XXRI icons in the list */

/* the user folders with XXRI artwork wear it in the list too, matching the
 * rail; everything else keeps libfm's icon */
static GHashTable* xxri_list_assets;   /* absolute path -> GdkPixbuf* */
static GdkPixbuf*  xxri_folder_pb;     /* folder@2.png - every other folder */

/* ~/Desktop: XXRI has no desktop icons, so it's just noise */
static gboolean xxri_is_hidden_place(FmPath* path)
{
    char* str;
    char* desktop;
    gboolean hit;

    if(!path)
        return FALSE;
    str = fm_path_to_str(path);
    desktop = g_build_filename(g_get_home_dir(), "Desktop", NULL);
    hit = (g_strcmp0(str, desktop) == 0);
    g_free(str);
    g_free(desktop);
    return hit;
}

static void xxri_list_assets_add(const char* dir, const char* asset, int px)
{
    char* path;
    GdkPixbuf* pb;
    GError* err = NULL;

    if(!dir || !*dir || g_strcmp0(dir, g_get_home_dir()) == 0)
        return;
    path = g_build_filename(XXRI_ICON_DIR, asset, NULL);
    pb = gdk_pixbuf_new_from_file_at_scale(path, px, px, TRUE, &err);
    if(pb)
        g_hash_table_insert(xxri_list_assets, g_strdup(dir), pb);
    else
    {
        g_warning("XXRI File: missing asset %s (%s)", path,
                  err ? err->message : "?");
        g_clear_error(&err);
    }
    g_free(path);
}

static void xxri_list_assets_init(int px)
{
    if(xxri_list_assets)
        return;
    xxri_list_assets = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, g_object_unref);
    xxri_list_assets_add(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD),
                         "slice30@2.png", px);
    xxri_list_assets_add(g_get_user_special_dir(G_USER_DIRECTORY_PICTURES),
                         "Photos@2.png", px);
    xxri_list_assets_add(g_get_user_special_dir(G_USER_DIRECTORY_MUSIC),
                         "music@2.png", px);
    xxri_list_assets_add(g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS),
                         "Videos@2.png", px);
    xxri_list_assets_add(g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS),
                         "Documents@2.png", px);
    {
        char* apps = g_build_filename(g_get_home_dir(), "Applications", NULL);
        xxri_list_assets_add(apps, "all-apps@2.png", px);
        g_free(apps);
    }
    {
        char* f = g_build_filename(XXRI_ICON_DIR, "folder@2.png", NULL);
        GError* err = NULL;

        xxri_folder_pb = gdk_pixbuf_new_from_file_at_scale(f, px, px, TRUE, &err);
        if(!xxri_folder_pb)
        {
            g_warning("XXRI File: missing asset %s (%s)", f,
                      err ? err->message : "?");
            g_clear_error(&err);
        }
        g_free(f);
    }
}

static void xxri_icon_cell_data(GtkTreeViewColumn* col, GtkCellRenderer* cell,
                                GtkTreeModel* model, GtkTreeIter* iter,
                                gpointer user_data)
{
    FmFileInfo* fi = NULL;
    GdkPixbuf* pb;
    char* path;

    /* COL_INFO is a plain G_TYPE_POINTER in libfm - no reference is taken. */
    gtk_tree_model_get(model, iter, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
    if(!fi)
        return;
    path = fm_path_to_str(fm_file_info_get_path(fi));
    if(!path)
        return;
    pb = g_hash_table_lookup(xxri_list_assets, path);
    if(!pb && xxri_folder_pb && fm_file_info_is_dir(fi))
        pb = xxri_folder_pb;
    if(pb)
        g_object_set(cell, "pixbuf", pb, NULL);
    g_free(path);
    xxri_band_cell_data(col, cell, model, iter, NULL);
}

static void xxri_hook_icon_column(GtkTreeView* tv);

static void on_tv_columns_changed(GtkTreeView* tv, gpointer d)
{
    xxri_hook_icon_column(tv);
}

/* ------------------------------------------------------- time grouping */

/* groups by mtime; the list is sorted by date, so the heading goes on the
 * first row of each run */
typedef enum {
    XXRI_G_TODAY, XXRI_G_YESTERDAY, XXRI_G_WEEK, XXRI_G_EARLIER
} XxriGroup;

static XxriGroup xxri_group_of(time_t when)
{
    GDateTime* now = g_date_time_new_now_local();
    GDateTime* then = g_date_time_new_from_unix_local((gint64)when);
    XxriGroup g = XXRI_G_EARLIER;
    gint dn, dt;

    if(!then)
    {
        g_date_time_unref(now);
        return XXRI_G_EARLIER;
    }
    dn = g_date_time_get_day_of_year(now) + g_date_time_get_year(now) * 366;
    dt = g_date_time_get_day_of_year(then) + g_date_time_get_year(then) * 366;
    if(dn == dt)            g = XXRI_G_TODAY;
    else if(dn - dt == 1)   g = XXRI_G_YESTERDAY;
    else if(dn - dt < 7)    g = XXRI_G_WEEK;
    g_date_time_unref(now);
    g_date_time_unref(then);
    return g;
}

static const char* xxri_group_label(XxriGroup g)
{
    switch(g)
    {
    case XXRI_G_TODAY:     return _("Today");
    case XXRI_G_YESTERDAY: return _("Yesterday");
    case XXRI_G_WEEK:      return _("This week");
    default:               return _("Earlier");
    }
}

static FmFileInfo* xxri_info_at(GtkTreeModel* model, GtkTreeIter* iter)
{
    FmFileInfo* fi = NULL;

    gtk_tree_model_get(model, iter, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
    return fi;
}

#define XXRI_BAND 24   /* the heading band above a group's first row */

static gboolean xxri_opens_group(GtkTreeModel* model, GtkTreeIter* iter,
                                 XxriGroup* out)
{
    FmFileInfo* fi = xxri_info_at(model, iter);
    GtkTreeIter prev;
    XxriGroup g;

    if(!fi)
        return FALSE;
    g = xxri_group_of(fm_file_info_get_mtime(fi));
    if(out)
        *out = g;
    prev = *iter;
    if(gtk_tree_model_iter_previous(model, &prev))
    {
        FmFileInfo* pfi = xxri_info_at(model, &prev);

        if(pfi && xxri_group_of(fm_file_info_get_mtime(pfi)) == g)
            return FALSE;
    }
    return TRUE;
}

/* A group's first row gets an extra band of height with its cells bottom-
 * aligned; on_tv_draw_headings() paints the heading into the band. No real
 * heading rows: libfm maps selection/activation onto its own FmFolderModel,
 * and a proxy model with extra rows would open the wrong file. */
static void xxri_band_cell_data(GtkTreeViewColumn* col, GtkCellRenderer* cell,
                                GtkTreeModel* model, GtkTreeIter* iter,
                                gpointer user_data)
{
    if(xxri_opens_group(model, iter, NULL))
        g_object_set(cell, "height", xxri_row_h + XXRI_BAND, "yalign", 1.0f, NULL);
    else
        g_object_set(cell, "height", xxri_row_h, "yalign", 0.5f, NULL);
}

static void xxri_name_cell_data(GtkTreeViewColumn* col, GtkCellRenderer* cell,
                                GtkTreeModel* model, GtkTreeIter* iter,
                                gpointer user_data)
{
    FmFileInfo* fi = xxri_info_at(model, iter);

    if(fi)
        g_object_set(cell, "text", fm_file_info_get_disp_name(fi), NULL);
    xxri_band_cell_data(col, cell, model, iter, user_data);
}

/* no date on folders, it means nothing to a user */
static void xxri_date_cell_data(GtkTreeViewColumn* col, GtkCellRenderer* cell,
                                GtkTreeModel* model, GtkTreeIter* iter,
                                gpointer user_data)
{
    FmFileInfo* fi = xxri_info_at(model, iter);

    if(fi && fm_file_info_is_dir(fi))
        g_object_set(cell, "text", "", NULL);
    xxri_band_cell_data(col, cell, model, iter, user_data);
}

/* group heading and dotted leader in the band above each group's first row */
static gboolean on_tv_draw_headings(GtkWidget* tv, cairo_t* cr, gpointer data)
{
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    GtkTreeIter it;
    PangoLayout* lay;
    PangoFontDescription* fd;
    int width = gtk_widget_get_allocated_width(tv);

    if(!model || !gtk_tree_model_get_iter_first(model, &it))
        return FALSE;

    fd = pango_font_description_from_string("Avenir Next Bold 8");
    lay = gtk_widget_create_pango_layout(tv, NULL);
    pango_layout_set_font_description(lay, fd);

    do {
        XxriGroup g;
        GtkTreePath* path;
        GdkRectangle rect;
        int tw = 0, th = 0;
        double base;
        static const double dots[] = { 1.0, 3.0 };

        if(!xxri_opens_group(model, &it, &g))
            continue;
        path = gtk_tree_model_get_path(model, &it);
        gtk_tree_view_get_background_area(GTK_TREE_VIEW(tv), path, NULL, &rect);
        gtk_tree_path_free(path);
        if(rect.height <= 0)
            continue;

        pango_layout_set_text(lay, xxri_group_label(g), -1);
        pango_layout_get_pixel_size(lay, &tw, &th);
        /* sit the label in the band, clear of the item row below it */
        base = rect.y + (XXRI_BAND - th) / 2.0 + 1;

        cairo_save(cr);
        if(xxri_theme_dark())
            cairo_set_source_rgba(cr, 0.949, 0.949, 0.957, 0.62);
        else
            cairo_set_source_rgba(cr, 0.11, 0.11, 0.14, 0.58);
        cairo_move_to(cr, rect.x + 2, base);
        pango_cairo_show_layout(cr, lay);

        /* dotted leader, on the label's centre line, starting clear of it */
        cairo_set_line_width(cr, 1.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_dash(cr, dots, 2, 0);
        cairo_move_to(cr, rect.x + 2 + tw + 8, base + th / 2.0);
        cairo_line_to(cr, width - 4, base + th / 2.0);
        cairo_stroke(cr);
        cairo_restore(cr);
    } while(gtk_tree_model_iter_next(model, &it));

    g_object_unref(lay);
    pango_font_description_free(fd);
    return FALSE;
}

/* ----------------------------------------------------------- list columns */

/* Cell data funcs on libfm's renderers; they run after the column attributes,
 * so they win where they set something and leave the rest alone. */
static void xxri_hook_icon_column(GtkTreeView* tv)
{
    GList* cols = gtk_tree_view_get_columns(tv);
    GList* c;
    int idx;

    /* columns come from our own "name;size;mtime" config, so index = column */
    for(c = cols, idx = 0; c; c = c->next, ++idx)
    {
        GtkTreeViewColumn* col = GTK_TREE_VIEW_COLUMN(c->data);
        GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(col));
        GList* r;

        for(r = cells; r; r = r->next)
        {
            GtkCellRenderer* cell = GTK_CELL_RENDERER(r->data);

            if(GTK_IS_CELL_RENDERER_PIXBUF(cell))
                gtk_tree_view_column_set_cell_data_func(col, cell,
                        xxri_icon_cell_data, NULL, NULL);
            else if(GTK_IS_CELL_RENDERER_TEXT(cell))
            {
                if(idx == 0)
                    gtk_tree_view_column_set_cell_data_func(col, cell,
                            xxri_name_cell_data, NULL, NULL);
                else if(idx == 2)
                    gtk_tree_view_column_set_cell_data_func(col, cell,
                            xxri_date_cell_data, NULL, NULL);
            }
        }
        g_list_free(cells);
    }
    g_list_free(cols);
}

static void xxri_style_view_cb(GtkWidget* w, gpointer data)
{
    GtkStyleContext* sc = gtk_widget_get_style_context(w);

    if(GTK_IS_TREE_VIEW(w))
    {
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(w), FALSE);
        xxri_list_assets_init(24);
        /* libfm (re)builds its columns later, so hook columns-changed too */
        xxri_hook_icon_column(GTK_TREE_VIEW(w));
        g_signal_connect(w, "columns-changed",
                         G_CALLBACK(on_tv_columns_changed), NULL);
        g_signal_connect_after(w, "draw",
                               G_CALLBACK(on_tv_draw_headings), NULL);
        gtk_tree_view_set_enable_search(GTK_TREE_VIEW(w), TRUE);
        gtk_style_context_add_class(sc, "xxri-file-view");
    }
    else if(GTK_IS_ICON_VIEW(w))
    {
        gtk_style_context_add_class(sc, "xxri-file-view");
    }
    if(GTK_IS_SCROLLED_WINDOW(w))
    {
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(w),
                                            GTK_SHADOW_NONE);
        gtk_style_context_add_class(sc, "xxri-file-view");
    }
    if(GTK_IS_CONTAINER(w))
        gtk_container_forall(GTK_CONTAINER(w), xxri_style_view_cb, data);
}

/* libfm's view sits in its own scrolled window with no styling API: walk
 * down, add the XXRI class, drop the column headers (the mockup has none) */
void xxri_ui_style_folder_view(GtkWidget* view)
{
    if(!view)
        return;
    gtk_style_context_add_class(gtk_widget_get_style_context(view),
                                "xxri-file-view");
    if(GTK_IS_CONTAINER(view))
        gtk_container_forall(GTK_CONTAINER(view), xxri_style_view_cb, NULL);
}

/* ----------------------------------------------------------- translucency */

/* translucent rail needs ARGB + compositor; otherwise .composited is off and
 * the window is opaque */
static void xxri_apply_composited(GtkWidget* win)
{
    GdkScreen* screen = gtk_widget_get_screen(win);
    GtkStyleContext* sc = gtk_widget_get_style_context(win);

    if(screen && gdk_screen_is_composited(screen))
        gtk_style_context_add_class(sc, "composited");
    else
        gtk_style_context_remove_class(sc, "composited");
}

static void on_composited_changed(GdkScreen* screen, GtkWidget* win)
{
    xxri_apply_composited(win);
    gtk_widget_queue_draw(win);
}

void xxri_ui_enable_alpha(GtkWidget* win)
{
    GdkScreen* screen = gtk_widget_get_screen(win);
    GdkVisual* rgba;

    if(!screen)
        return;
    rgba = gdk_screen_get_rgba_visual(screen);
    if(rgba)
        gtk_widget_set_visual(win, rgba);
    xxri_apply_composited(win);
    /* the compositor may start or stop after we map */
    g_signal_connect(screen, "composited-changed",
                     G_CALLBACK(on_composited_changed), win);
}
