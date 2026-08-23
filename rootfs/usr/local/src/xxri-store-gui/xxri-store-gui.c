/* xxri-store-gui - the XXRI Store (Phase 9).  GTK3 frontend ONLY.
 *
 * Every catalog / network / install action goes through the `xxri-store`
 * engine, which in turn hands AppImages to `xxri-app` (Phase 6) - the single
 * AppImage runtime.  This program never downloads, never mounts, never writes
 * a .desktop file: it renders the repository and drives those backends.
 *
 * Design language: Phase 4 design system, identical to xxri Settings (Phase 8)
 * - Avenir Next, the xxri gradient (pink -> purple -> blue), soft cards, pill
 * buttons.  Layout follows the Phase 9 mockup: a narrow icon rail, a search
 * header, hero banners, section rails of app tiles.
 */
#include <gtk/gtk.h>
#include <cairo.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <math.h>
#include <glib/gstdio.h>

#define STORE_DIR "/usr/local/share/xxri-store"
#define ICON_DIR  STORE_DIR "/icons"
#define UI_ICONS  STORE_DIR "/ui"

/* ------------------------------------------------------------ image load -- */
/* This TC gdk-pixbuf build ships broken built-in PNG loaders, so PNGs come in
 * through cairo (libpng direct) and are converted to a GdkPixbuf. */
static GdkPixbuf* load_png(const char* path, int w, int h) {
    /* Repository icon_url values are not all PNG - Blender's, for one, is an
       SVG.  cairo only decodes PNG, so an SVG silently became "no icon" and the
       card fell back to a generated placeholder.  Try gdk-pixbuf first for
       anything cairo cannot handle: its loader set (including librsvg's SVG
       loader) is registered now that loaders.cache ships in the image. */
    cairo_surface_t* s = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(s);
        GError* e = NULL;
        GdkPixbuf* pb = (w > 0)
            ? gdk_pixbuf_new_from_file_at_scale(path, w, h, TRUE, &e)
            : gdk_pixbuf_new_from_file(path, &e);
        if (e) g_error_free(e);
        return pb;                 /* NULL only if no loader understands it */
    }
    int sw = cairo_image_surface_get_width(s), sh = cairo_image_surface_get_height(s);
    GdkPixbuf* full = gdk_pixbuf_get_from_surface(s, 0, 0, sw, sh);
    cairo_surface_destroy(s);
    if (!full) return NULL;
    if (w > 0 && (sw != w || sh != h)) {
        GdkPixbuf* sc = gdk_pixbuf_scale_simple(full, w, h, GDK_INTERP_BILINEAR);
        g_object_unref(full); return sc;
    }
    return full;
}

/* ------------------------------------------------------------- backend --- */
/* Temporary startup instrumentation (Phase: repository debugging).
   Writes to $HOME/xxri-store-debug.log whenever XXRI_STORE_DEBUG is set, so the
   Store can be traced when launched FROM THE DOCK, where there is no terminal
   to read stderr from. */
static void dbg(const char* fmt, ...) {
    static FILE* f = NULL;
    if (!g_getenv("XXRI_STORE_DEBUG")) return;
    if (!f) {
        const char* h = g_getenv("HOME");
        char p[512]; snprintf(p,sizeof p,"%s/xxri-store-debug.log", h?h:"/tmp");
        f = fopen(p,"a");
        if (!f) return;
        setvbuf(f, NULL, _IOLBF, 0);
    }
    va_list ap; va_start(ap,fmt);
    vfprintf(f,fmt,ap); va_end(ap);
    fputc('\n',f); fflush(f);
}

static char* run_cmd(const char* fmt, ...) {
    char cmd[2048]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    char full[2100]; snprintf(full, sizeof full, "%s 2>/dev/null", cmd);
    FILE* f = popen(full, "r"); if (!f) return g_strdup("");
    GString* s = g_string_new(""); char buf[4096]; size_t n;
    while ((n = fread(buf,1,sizeof buf,f)) > 0) g_string_append_len(s, buf, n);
    pclose(f);
    while (s->len && (s->str[s->len-1]=='\n' || s->str[s->len-1]=='\r')) g_string_truncate(s, s->len-1);
    return g_string_free(s, FALSE);
}
/* fire-and-forget: the UI must never block on the network */
/* Stage log for the install path.  Always on to stderr when XXRI_GUI_TRACE is
   set, and additionally appended to the shared install-trace.log so the GUI and
   backend stages interleave in one file. */
static void gtrace(const char* fmt, ...) {
    if (!g_getenv("XXRI_GUI_TRACE")) return;
    char msg[1024]; va_list ap; va_start(ap,fmt);
    vsnprintf(msg,sizeof msg,fmt,ap); va_end(ap);
    fprintf(stderr,"GUI-TRACE %s\n",msg); fflush(stderr);
    const char* h=g_getenv("HOME");
    if (h) { char pth[512]; snprintf(pth,sizeof pth,"%s/.cache/xxri-store/install-trace.log",h);
        FILE* f=fopen(pth,"a"); if(f){ fprintf(f,"%-8s %-20s %s\n","gui","stage",msg); fclose(f);} }
}

static void run_bg(const char* fmt, ...) {
    char cmd[2048]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    char* argv[4] = { (char*)"/bin/sh", (char*)"-c", cmd, NULL };
    GError* err=NULL;
    gboolean ok = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                  G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, &err);
    gtrace("run_bg spawn=%s cmd=[%s]%s%s", ok?"OK":"FAILED", cmd,
           err?" err=":"", err?err->message:"");
    if (err) g_error_free(err);
}

/* flat-JSON helpers (same approach as Settings; the backends emit flat schemas).
 * jfind tolerates whitespace around ':' so a pretty-printed remote repository
 * parses exactly like the compact bundled one. */
static const char* jfind(const char* doc, const char* key, char want) {
    char pat[128]; snprintf(pat,sizeof pat,"\"%s\"",key);
    size_t pl=strlen(pat);
    const char* p=doc;
    while ((p=strstr(p,pat))) {
        const char* q=p+pl;
        while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++;
        if (*q!=':') { p+=pl; continue; }
        q++;
        while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++;
        if (want && *q!=want) { p+=pl; continue; }
        return q;
    }
    return NULL;
}
static char* jget(const char* doc, const char* key) {
    const char* p = jfind(doc,key,0); if(!p) return g_strdup("");
    if(*p=='"'){ p++; const char* e=p; while(*e && !(*e=='"'&&e[-1]!='\\'))e++;
        GString* s=g_string_new_len(NULL,0);
        for(const char* c=p;c<e;c++){
            if(*c=='\\'&&c+1<e){ c++;
                g_string_append_c(s, *c=='n'?'\n' : *c=='t'?'\t' : *c=='r'?'\r' : *c);
            } else g_string_append_c(s,*c);
        }
        return g_string_free(s,FALSE);
    }
    const char* e=p; while(*e && *e!=','&&*e!='}'&&*e!=']')e++;
    return g_strndup(p,e-p);
}
/* Read a scalar key at the TOP level of one object.  Catalog entries nest
   objects (each app carries a "sources" array whose members repeat key names
   like "version" and "priority"), so a plain search would return a nested
   value.  Depth is tracked and only depth-1 keys are accepted. */
static char* jtop(const char* obj, const char* key) {
    size_t klen = strlen(key);
    int depth = 0, instr = 0;
    for (const char* p = obj; *p; p++) {
        if (instr) { if (*p=='\\') { p++; continue; } if (*p=='"') instr=0; continue; }
        if (*p=='"') {
            if (depth == 1 && !strncmp(p+1,key,klen) && p[1+klen]=='"') {
                const char* q = p+2+klen;
                while (*q==' '||*q=='\t') q++;
                if (*q==':') {
                    q++; while (*q==' '||*q=='\t') q++;
                    if (*q=='{'||*q=='[') return g_strdup("");   /* structured */
                    if (*q=='"') {
                        q++; GString* g = g_string_new(NULL);
                        for (; *q && *q!='"'; q++) {
                            if (*q=='\\' && q[1]) { q++;
                                g_string_append_c(g, *q=='n'?'\n' : *q=='t'?'\t' : *q=='r'?'\r' : *q);
                            } else g_string_append_c(g,*q);
                        }
                        return g_string_free(g,FALSE);
                    }
                    const char* e = q;
                    while (*e && *e!=','&&*e!='}'&&*e!=']'&&*e!='\n') e++;
                    while (e>q && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\r')) e--;
                    return g_strndup(q,e-q);
                }
            }
            instr = 1; continue;
        }
        if (*p=='{'||*p=='[') depth++;
        if (*p=='}'||*p==']') depth--;
    }
    return g_strdup("");
}

static GPtrArray* jarr(const char* doc, const char* key) {
    GPtrArray* v = g_ptr_array_new_with_free_func(g_free);
    const char* p=jfind(doc,key,'['); if(!p) return v; p++;
    int depth=0, instr=0; const char* start=NULL;
    for(const char* i=p;*i;i++){
        if(instr){ if(*i=='\\'){ i++; continue; } if(*i=='"') instr=0; continue; }
        if(*i=='"'){ instr=1; continue; }
        if(*i=='{'){ if(depth==0)start=i; depth++; }
        else if(*i=='}'){ depth--; if(depth==0&&start) g_ptr_array_add(v,g_strndup(start,i-start+1)); }
        else if(*i==']'&&depth==0) break;
    }
    return v;
}
/* string-array field: "key":["a","b"] -> newline-joined */
static char* jstrs(const char* doc, const char* key) {
    const char* p=jfind(doc,key,'['); if(!p) return g_strdup("");
    p++; const char* e=p; int d=1;
    while(*e && d){ if(*e=='[')d++; else if(*e==']')d--; if(d)e++; }
    GString* s=g_string_new(""); const char* c=p;
    while(c<e){ if(*c=='"'){ c++; const char* q=c; while(*q&&*q!='"')q++;
            if(s->len)g_string_append_c(s,'\n'); g_string_append_len(s,c,q-c); c=q+1; }
        else c++; }
    return g_string_free(s,FALSE);
}

/* --------------------------------------------------------------- model --- */
typedef struct {
    char *id,*name,*category,*kind,*summary,*description,*developer,*publisher,*license,
         *homepage,*website,*version,*release_date,*updated,*source_type,*source_ref,
         *changelog,*icon_url,*arch,*permissions,*deps,*tags,*sha256,*keywords;
    gint64 size;
    int rank, nshots;
    gboolean verified;      /* a real, downloadable asset was confirmed */
    char *unavailable;      /* non-empty: why this app cannot be installed  */
} App;

static GPtrArray*   g_apps;          /* all App*                            */
static GHashTable*  g_byid;          /* id -> App*                          */
static char*        g_arch = NULL;   /* this device's architecture          */
static char*        g_repo = NULL;   /* active repository directory         */
static gboolean     g_repo_offline = FALSE;   /* serving the cached catalog        */
static gboolean     g_repo_available = FALSE; /* a catalog (fresh or cached) exists  */
static char*        g_repo_schema  = NULL;    /* schema_version from the catalog     */
static char*        g_repo_updated = NULL;    /* last_updated from the catalog       */
static char*        g_repo_kind=NULL;/* "cached" | "bundled"                */
static char*        g_cache = NULL;  /* ~/.cache/xxri-store                 */
static gboolean     g_online = FALSE;
static gint64       g_free_kb = 0;   /* free space on / , refreshed with state */
static GHashTable*  g_installed;     /* name(lower) -> version              */
static GHashTable*  g_inst_id;       /* name(lower) -> xxri-app registry id */
static GHashTable*  g_updates;       /* catalog id -> "cur|latest"          */
static GHashTable*  g_dlstate;       /* catalog id -> state string          */
static GHashTable*  g_sections;      /* section -> GPtrArray of App*        */
static GHashTable*  g_iconcache;     /* "id@size" -> GdkPixbuf*  (real icon)  */
/* Generated letter tiles are cached separately from real icons.  Sharing one
   table made a cache hit indistinguishable from "this app has an icon", so the
   second card for an app that appears in two rails was handed the placeholder
   and never registered for the icon fetch - it kept the tile for the whole
   session even after the real icon had been downloaded. */
static GHashTable*  g_tilecache;     /* "id@size" -> GdkPixbuf*  (placeholder) */
static GtkWidget   *g_stack, *g_win, *g_search_entry, *g_status_chip, *g_rail;
static char*        g_page = NULL;   /* current page id                     */
static GPtrArray*   g_history;       /* page ids for the back button        */
static GPtrArray*   g_pending_icons; /* IconWant* for the visible page      */

typedef struct { char* id; GtkWidget* img; int size; } IconWant;

static void open_page(const char* id);
static gboolean app_installable(App* a);
static void rebuild(const char* id);
static void refresh_state(void);

static const char* CAT_TITLE(const char* id);

/* The repository labels categories for humans ("Remote Desktop", "IDE"); the
   Store's pages, rails and filters are keyed on its own slugs.  Translate at
   load time so the existing category UI keeps working untouched - without this
   no application matches any category and every page renders empty. */
static const char* cat_slug(const char* c) {
    struct { const char* from; const char* to; } m[] = {
        {"Browser","browser"},      {"Chat","communication"},   {"Office","office"},
        {"IDE","development"},      {"Development","development"},
        {"Graphics","graphics"},    {"Music","audio"},          {"Audio","audio"},
        {"Video","video"},          {"Multimedia","video"},     {"Emulator","virtualization"},
        {"Network","networking"},   {"Internet","networking"},  {"Remote Desktop","networking"},
        {"Utilities","utilities"},  {"Compression","utilities"},{"Science","science"},
        {"System","system"},        {"PDF","office"},           {"Security","security"},
        {"Games","games"},          {"Photography","photography"},
        {NULL,NULL}
    };
    if (!c || !*c) return "utilities";
    for (int i=0;m[i].from;i++) if (!g_ascii_strcasecmp(c,m[i].from)) return m[i].to;
    /* an unmapped label is lower-cased and used as-is, so a category the
       repository adds later still groups sensibly instead of vanishing */
    static char buf[64];
    g_snprintf(buf,sizeof buf,"%s",c);
    for (char* q=buf; *q; q++) *q = g_ascii_tolower(*q);
    return buf;
}

/* Order for the home-page rails: the repository's own priority_tier first
   (higher rank = lower tier number = better supported), then title. */
static gint cmp_rank_then_name(gconstpointer x, gconstpointer y) {
    const App* a = *(const App* const*)x;
    const App* b = *(const App* const*)y;
    if (a->rank != b->rank) return b->rank - a->rank;
    return g_ascii_strcasecmp(a->name?a->name:"", b->name?b->name:"");
}

/* ------------------------------------------------------------- catalog --- */
static void load_catalog(void) {
    if (g_apps) {   /* a refresh replaced the repository - drop the old model */
        g_ptr_array_free(g_apps, TRUE);
        g_hash_table_destroy(g_byid);
        if (g_sections) g_hash_table_destroy(g_sections);
        if (g_iconcache) g_hash_table_destroy(g_iconcache);
        if (g_tilecache) g_hash_table_destroy(g_tilecache);
    }
    g_apps = g_ptr_array_new();
    g_byid = g_hash_table_new(g_str_hash, g_str_equal);
    g_iconcache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    g_tilecache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

    g_free(g_repo);
    g_repo = run_cmd("xxri-store repo-path");
    if (!*g_repo) { g_free(g_repo); g_repo = g_strdup(STORE_DIR "/repository"); }

    /* The catalog is the repository's i686.json, fetched by the backend into
       the cache.  Nothing is bundled and nothing is generated locally: this is
       the single source of truth, and the schema's own field names are read
       directly.  Fields we do not know about are simply never asked for, so the
       repository can grow without needing a new Store. */
    char cpath[1024]; snprintf(cpath,sizeof cpath,"%s/i686.json",g_repo);
    char* doc=NULL; gsize len;
    g_repo_available = FALSE;
    dbg("LOAD_CATALOG repo=%s catalog=%s exists=%d", g_repo, cpath,
        g_file_test(cpath,G_FILE_TEST_EXISTS));
    if (g_file_get_contents(cpath,&doc,&len,NULL)) {
        g_repo_available = TRUE;
        g_free(g_repo_schema); g_repo_schema = jget(doc,"schema_version");
        g_free(g_repo_updated); g_repo_updated = jget(doc,"last_updated");
        GPtrArray* arr = jarr(doc,"applications");
        for (guint i=0;i<arr->len;i++) {
            char* o = arr->pdata[i];
            App* a = g_new0(App,1);
            a->id          = jtop(o,"id");
            a->name        = jtop(o,"title");
            { char* rawcat = jtop(o,"category");
              a->category = g_strdup(cat_slug(rawcat));
              g_free(rawcat); }
            a->description = jtop(o,"description");
            a->summary     = g_strdup(a->description);
            a->homepage    = jtop(o,"homepage");
            a->website     = g_strdup(a->homepage);
            a->icon_url    = jtop(o,"icon_url");
            a->arch        = jtop(o,"architecture");
            a->source_type = jtop(o,"package_type");
            a->sha256      = jtop(o,"checksum");
            a->version     = jtop(o,"version");
            if (!*a->version) { g_free(a->version); a->version = jtop(o,"current_upstream_version"); }
            a->tags     = jstrs(o,"tags");
            a->keywords = jstrs(o,"keywords");
            a->deps     = jstrs(o,"dependencies");
            a->developer = g_strdup(""); a->publisher = g_strdup("");
            a->license = g_strdup(""); a->changelog = g_strdup("");
            a->release_date = g_strdup(""); a->updated = g_strdup("");
            a->permissions = g_strdup(""); a->source_ref = g_strdup("");
            char* sz = jtop(o,"size"); a->size = g_ascii_strtoll(sz,NULL,10); g_free(sz);
            /* "null" is how the schema spells an absent value */
            if (!strcmp(a->icon_url,"null")) { g_free(a->icon_url); a->icon_url=g_strdup(""); }
            if (!strcmp(a->sha256,"null"))   { g_free(a->sha256);   a->sha256=g_strdup(""); }

            char* url = jtop(o,"download_url");
            gboolean has_url = *url && strcmp(url,"null") && g_str_has_prefix(url,"http");
            char* tier = jtop(o,"priority_tier");
            /* An absent tier is spelled "null"; atoi("null") is 0, which would
               otherwise score higher than tier 1 and push every unsupported
               entry to the top of the home page. */
            int tn = (*tier && strcmp(tier,"null")) ? atoi(tier) : 0;
            a->rank = tn ? (100 - tn*10) : 0;
            /* Packaging is decided from the URL's real extension, exactly as
               the backend does.  This used to shell out to `xxri-store field`
               once per application - 57 popen() calls that delayed the first
               paint by seconds - and the answer is a pure string test. */
            const char* ext = url;
            a->kind = g_strdup(
                (g_str_has_suffix(ext,".AppImage")||g_str_has_suffix(ext,".appimage")) ? "appimage" :
                 g_str_has_suffix(ext,".tcz")      ? "tcz" :
                (g_str_has_suffix(ext,".tar.gz") ||g_str_has_suffix(ext,".tgz")   ||
                 g_str_has_suffix(ext,".tar.xz") ||g_str_has_suffix(ext,".txz")   ||
                 g_str_has_suffix(ext,".tar.bz2")||g_str_has_suffix(ext,".tbz")   ||
                 g_str_has_suffix(ext,".zip")    ||g_str_has_suffix(ext,".7z"))   ? "archive" :
                (g_str_has_suffix(ext,".sh")||g_str_has_suffix(ext,".run")||
                 g_str_has_suffix(ext,".deb")||g_str_has_suffix(ext,".rpm"))      ? "installer" :
                 !strcmp(a->source_type,"official_appimage")                      ? "appimage" :
                 !strcmp(a->source_type,"tinycore_tcz")                           ? "tcz" :
                                                                                    "archive");
            gboolean runnable = !strcmp(a->kind,"appimage") || !strcmp(a->kind,"archive")
                             || !strcmp(a->kind,"tcz");
            a->verified = has_url && runnable;
            if (!a->verified) {
                char* why = jtop(o,"support_status");
                if (!*why) { g_free(why); why = jtop(o,"research_status"); }
                if (!has_url)
                    a->unavailable = g_strdup(*why ? why : "No download published for i686");
                else
                    a->unavailable = g_strdup_printf("Packaging not supported (%s)", a->kind);
                g_free(why);
            } else a->unavailable = g_strdup("");
            g_free(url); g_free(tier);

            a->nshots = 0;
            if (!*a->id) { g_free(a); continue; }
            g_ptr_array_add(g_apps,a);
            g_hash_table_replace(g_byid,a->id,a);
        }
        g_ptr_array_free(arr,TRUE); g_free(doc);
        dbg("PARSED size=%u apps=%u schema=%s updated=%s", (unsigned)len,
            g_apps->len, g_repo_schema?g_repo_schema:"-", g_repo_updated?g_repo_updated:"-");
    } else dbg("PARSE_FAILED could not read %s", cpath);
    dbg("REPO_AVAILABLE=%d MODEL_APPS=%u", g_repo_available, g_apps?g_apps->len:0);

    /* app_visible() below compares against this device's architecture, so it
       must be known before any visibility test runs - it used to be resolved
       further down, which left g_arch NULL here and crashed the Store on
       startup the moment the section builder called app_visible(). */
    if (!g_arch) {
        char* ar = run_cmd("xxri-store arch");
        g_arch = jget(ar,"arch"); g_free(ar);
        if (!g_arch || !*g_arch) { g_free(g_arch); g_arch = g_strdup("i686"); }
    }

    /* home-page sections come from the repository, not from the code */
    g_sections = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,(GDestroyNotify)g_ptr_array_unref);
    char sp[1024]; snprintf(sp,sizeof sp,"%s/sections.json",g_repo);
    char* sdoc=NULL; gsize slen;
    if (g_file_get_contents(sp,&sdoc,&slen,NULL)) {
        const char* keys[]={"featured","recommended","trending","editors","newest","updated",NULL};
        for (int k=0;keys[k];k++) {
            char* ids = jstrs(sdoc,keys[k]);
            GPtrArray* v = g_ptr_array_new();
            char** parts = g_strsplit(ids,"\n",-1);
            for (int i=0;parts[i];i++) {
                App* a = g_hash_table_lookup(g_byid,parts[i]);
                if (a) g_ptr_array_add(v,a);
            }
            g_strfreev(parts); g_free(ids);
            g_hash_table_replace(g_sections,g_strdup(keys[k]),v);
        }
        g_free(sdoc);
    } else {
        /* The online repository publishes no sections file, so the same
           section keys are filled from the repository's own ranking
           (priority_tier, already stored in App.rank).  The home page code
           below is untouched - only where these id lists come from changed,
           exactly as the catalog itself did.  Without this every rail and hero
           renders empty even though the catalog loaded fine. */
        GPtrArray* pool = g_ptr_array_new();
        for (guint i=0;i<g_apps->len;i++) {
            App* a = g_apps->pdata[i];
            /* app_installable() only tests the catalog (verified + arch);
               app_visible() would consult g_installed, which load_installed()
               has not built yet at this point in startup. */
            if (app_installable(a)) g_ptr_array_add(pool,a);
        }
        g_ptr_array_sort(pool, cmp_rank_then_name);
        struct { const char* key; int from, to, step; } S[] = {
            {"featured",    0,  8, 1},
            {"recommended", 0, 18, 1},
            {"trending",    4, 16, 1},
            {"editors",     0, 24, 3},
            {"newest",      8, 20, 1},
            {"updated",     0, 12, 1},
            {NULL,0,0,0}
        };
        for (int k=0;S[k].key;k++) {
            GPtrArray* v = g_ptr_array_new();
            for (int i=S[k].from; i<S[k].to && i<(int)pool->len; i+=S[k].step)
                g_ptr_array_add(v, pool->pdata[i]);
            g_hash_table_replace(g_sections,g_strdup(S[k].key),v);
        }
        g_ptr_array_free(pool,TRUE);
    }
    if (!g_arch) {
        char* ar = run_cmd("xxri-store arch");
        g_arch = jget(ar,"arch"); g_free(ar);
        if (!*g_arch) { g_free(g_arch); g_arch = g_strdup("i686"); }
    }
}

/* live state: what is installed, what has an update, what is downloading */
static void load_installed(void) {
    if (g_installed) g_hash_table_destroy(g_installed);
    if (g_inst_id) g_hash_table_destroy(g_inst_id);
    g_installed = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
    g_inst_id   = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
    char* js = run_cmd("xxri-store installed");
    GPtrArray* a = jarr(js,"apps");
    for (guint i=0;i<a->len;i++) {
        char* o=a->pdata[i];
        char* nm=jget(o,"name"); char* vr=jget(o,"version"); char* rid=jget(o,"id");
        char* low=g_ascii_strdown(nm,-1);
        g_hash_table_replace(g_installed,g_strdup(low),g_strdup(vr));
        g_hash_table_replace(g_inst_id,low,g_strdup(rid));
        g_free(nm);g_free(vr);g_free(rid);
    }
    g_ptr_array_free(a,TRUE); g_free(js);

    if (g_updates) g_hash_table_destroy(g_updates);
    g_updates = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
    char* uj = run_cmd("xxri-store updates");
    GPtrArray* u = jarr(uj,"updates");
    for (guint i=0;i<u->len;i++) {
        char* o=u->pdata[i];
        char* cid=jget(o,"catalog_id"); char* cur=jget(o,"current"); char* lat=jget(o,"latest");
        if (*cid) g_hash_table_replace(g_updates,g_strdup(cid),g_strdup_printf("%s|%s",cur,lat));
        g_free(cid);g_free(cur);g_free(lat);
    }
    g_ptr_array_free(u,TRUE); g_free(uj);

    if (g_dlstate) g_hash_table_destroy(g_dlstate);
    g_dlstate = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
    char* dj = run_cmd("xxri-store downloads");
    GPtrArray* dl = jarr(dj,"downloads");
    for (guint i=0;i<dl->len;i++) {
        char* o=dl->pdata[i];
        char* id=jget(o,"id"); char* st=jget(o,"state");
        g_hash_table_replace(g_dlstate,id,st);
    }
    g_ptr_array_free(dl,TRUE); g_free(dj);
}

static gboolean app_installed(App* a) {
    char* low=g_ascii_strdown(a->name,-1);
    gboolean r=g_hash_table_contains(g_installed,low); g_free(low); return r;
}
static const char* installed_id(App* a) {
    char* low=g_ascii_strdown(a->name,-1);
    const char* r=g_hash_table_lookup(g_inst_id,low); g_free(low); return r;
}
static gboolean app_compatible(App* a) {
    char** v=g_strsplit(a->arch,"\n",-1); gboolean ok=FALSE;
    for (int i=0;v[i];i++) if (!strcmp(v[i],g_arch)||!strcmp(v[i],"any")) ok=TRUE;
    g_strfreev(v); return ok;
}
/* On XXRI Lite most catalog apps are x86_64/aarch64 and cannot run.  Hide them
 * by default (a header toggle brings them back), so browsing shows only what
 * this device can actually install.  Installed apps always stay visible. */
static gboolean g_show_incompat = FALSE;
/* Parts 4 + 8: an app is shown only when this device can actually install it -
 * the architecture matches AND a concrete downloadable asset was confirmed at
 * catalog-build time (dead links / metadata-only entries are hidden).  Already
 * installed apps always stay visible, and the header toggle reveals the rest of
 * the catalog for browsing. */
static gboolean app_installable(App* a) { return a->verified && app_compatible(a); }
/* Only what this device can actually install is shown: the app must be verified
 * (a confirmed, downloadable asset) AND built for this CPU.  Apps for other
 * architectures are hidden, reachable only through the header toggle, so the
 * Store never advertises something the user cannot install. */
static gboolean app_unavailable(App* a) { return a->unavailable && *a->unavailable; }
static gboolean app_visible(App* a) {
    return g_show_incompat || app_installable(a) || app_installed(a) || app_unavailable(a);
}
static gboolean app_updatable(App* a) { return g_hash_table_contains(g_updates,a->id); }
static const char* app_dlstate(App* a) { return g_hash_table_lookup(g_dlstate,a->id); }

/* An app can publish several architectures.  Name the one a user would look
 * for (x86_64 unless the app is not built for it), and be able to list them
 * all - "published for aarch64 only" when it also ships x86_64 is a lie. */
static char* primary_arch(App* a) {
    char** v=g_strsplit(a->arch,"\n",-1);
    char* out=NULL;
    for (int i=0;v[i] && !out;i++) if (!strcmp(v[i],"x86_64")) out=g_strdup(v[i]);
    if (!out) out=g_strdup(v[0]?v[0]:"another CPU");
    g_strfreev(v); return out;
}
static char* arch_join(App* a,const char* sep,const char* last_sep) {
    char** v=g_strsplit(a->arch,"\n",-1);
    guint n=g_strv_length(v);
    GString* s=g_string_new("");
    for (guint i=0;i<n;i++) {
        if (i) g_string_append(s, (i==n-1 && last_sep) ? last_sep : sep);
        g_string_append(s,v[i]);
    }
    g_strfreev(v);
    return g_string_free(s,FALSE);
}

static char* human_size(gint64 b) {
    if (b <= 0) return g_strdup("");
    if (b < 1024*1024) return g_strdup_printf("%.0f KB", b/1024.0);
    if (b < 1024LL*1024*1024) return g_strdup_printf("%.1f MB", b/(1024.0*1024));
    return g_strdup_printf("%.2f GB", b/(1024.0*1024*1024));
}

/* ----------------------------------------------------------- ui helpers -- */
static void css(GtkWidget* w,const char* c){ gtk_style_context_add_class(gtk_widget_get_style_context(w),c); }
static GtkWidget* lbl(const char* t,const char* c,gfloat x){
    GtkWidget* l=gtk_label_new(t?t:"");
    gtk_label_set_xalign(GTK_LABEL(l),x);
    if(c)css(l,c); return l;
}
static GtkWidget* lbl_wrap(const char* t,const char* c,gfloat x,int width){
    GtkWidget* l=lbl(t,c,x);
    gtk_label_set_line_wrap(GTK_LABEL(l),TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(l),PANGO_WRAP_WORD_CHAR);
    if(width>0) gtk_label_set_max_width_chars(GTK_LABEL(l),width);
    return l;
}
static GtkWidget* lbl_ell(const char* t,const char* c,gfloat x,int chars){
    GtkWidget* l=lbl(t,c,x);
    gtk_label_set_ellipsize(GTK_LABEL(l),PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(l),chars);
    return l;
}

/* ---- generated app tile: a gradient rounded square with the app initial ---
 * 950+ apps cannot ship 950 icons, and an app without an icon must still look
 * like it belongs.  The tile is deterministic (same app -> same colours).  */
static void rr_path(cairo_t* cr,double x,double y,double w,double h,double r){
    cairo_new_sub_path(cr);
    cairo_arc(cr,x+w-r,y+r,r,-G_PI/2,0); cairo_arc(cr,x+w-r,y+h-r,r,0,G_PI/2);
    cairo_arc(cr,x+r,y+h-r,r,G_PI/2,G_PI); cairo_arc(cr,x+r,y+r,r,G_PI,3*G_PI/2);
    cairo_close_path(cr);
}
typedef struct { double r,g,b; } RGB;
static const RGB TILE_A[] = {
    {0.929,0.424,0.878},{0.514,0.443,0.969},{0.180,0.486,0.965},{0.941,0.416,0.557},
    {0.514,0.443,0.969},{0.180,0.486,0.965},{0.929,0.424,0.878},{0.110,0.106,0.141},
};
static const RGB TILE_B[] = {
    {0.514,0.443,0.969},{0.180,0.486,0.965},{0.514,0.443,0.969},{0.514,0.443,0.969},
    {0.929,0.424,0.878},{0.514,0.443,0.969},{0.180,0.486,0.965},{0.514,0.443,0.969},
};
static GdkPixbuf* gen_tile(const char* id,const char* name,int size){
    guint h=0; for(const char* p=id;*p;p++) h=h*31+(guint)*p;
    int k=h%G_N_ELEMENTS(TILE_A);
    cairo_surface_t* s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,size,size);
    cairo_t* cr=cairo_create(s);
    cairo_pattern_t* pat=cairo_pattern_create_linear(0,0,size,size);
    cairo_pattern_add_color_stop_rgb(pat,0,TILE_A[k].r,TILE_A[k].g,TILE_A[k].b);
    cairo_pattern_add_color_stop_rgb(pat,1,TILE_B[k].r,TILE_B[k].g,TILE_B[k].b);
    rr_path(cr,0,0,size,size,size*0.22);
    cairo_set_source(cr,pat); cairo_fill(cr); cairo_pattern_destroy(pat);
    /* the initial, in the family the rest of the OS uses */
    char init[8]={0}; const char* p=name&&*name?name:id;
    g_utf8_strncpy(init,p,1);
    char* up=g_utf8_strup(init,-1);
    cairo_select_font_face(cr,"Avenir Next",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr,size*0.5);
    cairo_text_extents_t te; cairo_text_extents(cr,up,&te);
    cairo_set_source_rgba(cr,1,1,1,0.95);
    cairo_move_to(cr,(size-te.width)/2-te.x_bearing,(size-te.height)/2-te.y_bearing);
    cairo_show_text(cr,up); g_free(up);
    cairo_destroy(cr);
    GdkPixbuf* pb=gdk_pixbuf_get_from_surface(s,0,0,size,size);
    cairo_surface_destroy(s);
    return pb;
}
/* icon resolution: runtime cache -> bundled -> generated tile */
static GdkPixbuf* app_icon(App* a,int size,gboolean* from_file){
    char key[256]; snprintf(key,sizeof key,"%s@%d",a->id,size);
    GdkPixbuf* pb=g_hash_table_lookup(g_iconcache,key);
    if (pb) { if(from_file)*from_file=TRUE; return pb; }
    /* a cached placeholder is NOT an icon: report it as such so the caller
       still queues this widget for the icon fetch */
    pb=g_hash_table_lookup(g_tilecache,key);
    if (pb) { if(from_file)*from_file=FALSE; return pb; }
    char p[1024]; GdkPixbuf* out=NULL; gboolean real=FALSE;
    /* An installed app's own extracted icon wins: xxri-app resolves it from the
       application's real metadata, so the Store then shows exactly what the
       dock and the menu show instead of a second, differently-sourced image. */
    snprintf(p,sizeof p,"/usr/local/share/pixmaps/xxri-app-%s.png",a->id);
    out=load_png(p,size,size);
    if (!out && g_cache) { snprintf(p,sizeof p,"%s/icons/%s.png",g_cache,a->id); out=load_png(p,size,size); }
    if (!out) { snprintf(p,sizeof p,"%s/%s.png",ICON_DIR,a->id); out=load_png(p,size,size); }
    if (out) real=TRUE; else out=gen_tile(a->id,a->name,size);
    if (out) g_hash_table_replace(real?g_iconcache:g_tilecache,g_strdup(key),out);
    if (from_file) *from_file=real;
    return out;
}
static GtkWidget* icon_img(App* a,int size){
    gboolean real=FALSE;
    GdkPixbuf* pb=app_icon(a,size,&real);
    GtkWidget* i=pb?gtk_image_new_from_pixbuf(pb):gtk_image_new();
    css(i,"xxri-appicon");
    gtk_widget_set_size_request(i,size,size);
    /* no real icon yet: remember it, the fetcher may bring one in a moment */
    if (!real && *a->icon_url && g_pending_icons) {
        IconWant* w=g_new0(IconWant,1);
        w->id=g_strdup(a->id); w->img=i; w->size=size;
        g_object_ref(i);
        g_ptr_array_add(g_pending_icons,w);
    }
    return i;
}

/* ------------------------------------------------- lazily cached icons ---- */
static void iconwant_free(gpointer p){ IconWant* w=p; g_free(w->id); if(w->img) g_object_unref(w->img); g_free(w); }
static gboolean poll_icons(gpointer u){
    static int tries=0;
    if (!g_pending_icons || !g_pending_icons->len) { tries=0; return G_SOURCE_REMOVE; }
    int landed=0;
    for (guint i=0;i<g_pending_icons->len;) {
        IconWant* w=g_pending_icons->pdata[i];
        if (!GTK_IS_IMAGE(w->img)) { g_ptr_array_remove_index_fast(g_pending_icons,i); continue; }
        char p[1024]; snprintf(p,sizeof p,"%s/icons/%s.png",g_cache,w->id);
        if (g_file_test(p,G_FILE_TEST_EXISTS)) {
            char key[256]; snprintf(key,sizeof key,"%s@%d",w->id,w->size);
            g_hash_table_remove(g_iconcache,key);
            g_hash_table_remove(g_tilecache,key);
            GdkPixbuf* pb=load_png(p,w->size,w->size);
            if (pb) {
                gtk_image_set_from_pixbuf(GTK_IMAGE(w->img),pb);
                g_hash_table_replace(g_iconcache,g_strdup(key),pb);
                landed++;
            }
            g_ptr_array_remove_index_fast(g_pending_icons,i); continue;
        }
        i++;
    }
    /* Patience must match the work: the repository publishes an icon for every
       application, so a cold cache means the engine is fetching dozens of files
       one HTTPS round trip at a time.  Counting polls from the start gave up
       after 18s and left the rest of the page as generated tiles, so the clock
       restarts every time an icon actually lands and only a genuinely idle
       stretch ends the poll. */
    if (landed) tries = 0;
    if (++tries > 40) { tries=0; return G_SOURCE_REMOVE; }
    return G_SOURCE_CONTINUE;
}
/* ask the engine to cache the icons this page wants (one batched call) */
static void kick_icon_fetch(void){
    if (!g_online || !g_pending_icons || !g_pending_icons->len) return;
    GString* cmd=g_string_new("xxri-store icons");
    GHashTable* seen=g_hash_table_new(g_str_hash,g_str_equal);
    int n=0;
    for (guint i=0;i<g_pending_icons->len && n<48;i++) {
        IconWant* w=g_pending_icons->pdata[i];
        if (g_hash_table_contains(seen,w->id)) continue;
        g_hash_table_add(seen,w->id);
        g_string_append_printf(cmd," %s",w->id); n++;
    }
    g_hash_table_destroy(seen);
    if (n) { run_bg("%s",cmd->str); g_timeout_add(1500,poll_icons,NULL); }
    g_string_free(cmd,TRUE);
}

/* --------------------------------------------------------- app actions --- */
static void act_install(GtkWidget* w,gpointer p){ App* a=p; (void)w;
    gtrace("act_install ENTER  ptr=%p", (void*)a);
    if (!a) { gtrace("act_install ABORT: NULL app pointer"); return; }
    gtrace("act_install app id='%s' name='%s' kind='%s' verified=%d",
           a->id?a->id:"(null)", a->name?a->name:"(null)",
           a->kind?a->kind:"(null)", a->verified);
    if (!a->id || !*a->id) { gtrace("act_install ABORT: empty id -> backend would get no argument"); return; }
    run_bg("xxri-store install %s",a->id);
    gtrace("act_install -> open_page(downloads)");
    open_page("downloads");
    gtrace("act_install DONE");
}
static void act_launch(GtkWidget* w,gpointer p){ App* a=p; (void)w;
    const char* rid=installed_id(a);
    if (rid) run_bg("xxri-app launch %s",rid);
}
static void act_remove(GtkWidget* w,gpointer p){ App* a=p; (void)w;
    const char* rid=installed_id(a);
    if (!rid) return;
    GtkWidget* d=gtk_message_dialog_new(GTK_WINDOW(g_win),GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,GTK_BUTTONS_NONE,
        "Remove %s?",a->name);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
        "The app will be removed from the dock and the menu, and its package file will be deleted.");
    gtk_dialog_add_button(GTK_DIALOG(d),"Cancel",GTK_RESPONSE_CANCEL);
    GtkWidget* rb=gtk_dialog_add_button(GTK_DIALOG(d),"Remove",GTK_RESPONSE_ACCEPT);
    css(rb,"destructive-action");
    int r=gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (r!=GTK_RESPONSE_ACCEPT) return;
    char* out=run_cmd("xxri-app remove %s --purge",rid); g_free(out);
    refresh_state();
    rebuild(g_page);
}
static void act_update(GtkWidget* w,gpointer p){ App* a=p; (void)w;
    run_bg("xxri-store install %s",a->id);
    open_page("downloads");
}
/* update by bare catalog id - used when the installed app's catalog entry is
 * not in the currently loaded catalog (e.g. a catalog that changed under us) */
static void act_update_id(GtkWidget* w,gpointer p){ (void)p;
    const char* cid=g_object_get_data(G_OBJECT(w),"cid");
    if (cid && *cid) { run_bg("xxri-store install %s",cid); open_page("downloads"); }
}
static void act_website(GtkWidget* w,gpointer p){ App* a=p; (void)w;
    if (*a->homepage) run_bg("xxri-open-url '%s'",a->homepage);
}

/* Disk this app will really need, in KB - the same estimate the backend uses
   (see space_needed() in xxri-store).  A compressed archive expands to several
   times its download size, and a native extension drags in a dependency chain
   much larger than itself, so the download size alone is not the answer. */
static gint64 app_space_kb(App* a){
    gint64 sz = a->size > 0 ? a->size : (gint64)32*1024*1024;
    int mult = 2;
    if (a->kind && !strcmp(a->kind,"tcz"))          mult = 5;
    else if (a->kind && !strcmp(a->kind,"archive")) mult = 4;
    return (sz/1024)*mult + 51200;          /* + 50 MB working headroom */
}
static gboolean app_has_room(App* a){
    return g_free_kb <= 0 || app_space_kb(a) <= g_free_kb;
}

/* the one button that says what this app needs right now */
static GtkWidget* action_button(App* a,gboolean big){
    const char* st=app_dlstate(a);
    if (st && (!strcmp(st,"downloading")||!strcmp(st,"queued")||!strcmp(st,"installing")
               ||!strcmp(st,"verifying")||!strcmp(st,"resolving"))) {
        GtkWidget* b=gtk_button_new_with_label(!strcmp(st,"queued")?"Queued":"Installing\xe2\x80\xa6");
        gtk_widget_set_sensitive(b,FALSE); css(b,"xxri-chip");
        if(big)css(b,"xxri-bigbtn");
        return b;
    }
    if (app_updatable(a)) {
        GtkWidget* b=gtk_button_new_with_label("Update");
        css(b,"xxri-accent"); if(big)css(b,"xxri-bigbtn");
        g_signal_connect(b,"clicked",G_CALLBACK(act_update),a);
        return b;
    }
    if (app_installed(a)) {
        GtkWidget* b=gtk_button_new_with_label("Open");
        css(b,"xxri-accent"); if(big)css(b,"xxri-bigbtn");
        g_signal_connect(b,"clicked",G_CALLBACK(act_launch),a);
        return b;
    }
    if (app_unavailable(a)) {
        GtkWidget* b=gtk_button_new_with_label(big ? "Currently unsupported" : "Unsupported");
        gtk_widget_set_sensitive(b,FALSE); css(b,"xxri-chip");
        gtk_widget_set_tooltip_text(b,a->unavailable);
        if(big)css(b,"xxri-bigbtn");
        return b;
    }
    if (app_compatible(a) && !a->verified) {
        /* metadata-only entry: no confirmed asset, so never offer a dead Install */
        GtkWidget* b=gtk_button_new_with_label("Unavailable");
        gtk_widget_set_sensitive(b,FALSE); css(b,"xxri-chip");
        gtk_widget_set_tooltip_text(b,"No downloadable release was found for this architecture");
        if(big)css(b,"xxri-bigbtn");
        return b;
    }
    if (!app_compatible(a)) {
        GtkWidget* b=gtk_button_new_with_label(big ? "Available for XXRI Regular / Pro"
                                                   : "Regular / Pro");
        gtk_widget_set_sensitive(b,FALSE); css(b,"xxri-chip");
        char* pa=primary_arch(a);
        char tip[128]; snprintf(tip,sizeof tip,
            "This app ships a %s build. XXRI Lite is 32-bit (i686); "
            "XXRI Regular and Pro can install it.", pa);
        g_free(pa);
        gtk_widget_set_tooltip_text(b,tip);
        if(big)css(b,"xxri-bigbtn");
        return b;
    }
    if (!app_has_room(a)) {
        /* The Store must not offer an action that cannot succeed: this install
           would run out of disk part-way through and fail deep inside the
           unpacker, which reads to the user as "Install does nothing". */
        GtkWidget* b=gtk_button_new_with_label(big ? "Not enough disk space" : "No space");
        gtk_widget_set_sensitive(b,FALSE); css(b,"xxri-chip");
        char tip[192]; snprintf(tip,sizeof tip,
            "%s needs about %lld MB but only %lld MB is free. "
            "Remove an application you no longer need, then try again.",
            a->name, (long long)(app_space_kb(a)/1024), (long long)(g_free_kb/1024));
        gtk_widget_set_tooltip_text(b,tip);
        if(big)css(b,"xxri-bigbtn");
        return b;
    }
    GtkWidget* b=gtk_button_new_with_label("Install");
    css(b,"xxri-accent"); if(big)css(b,"xxri-bigbtn");
    gulong h=g_signal_connect(b,"clicked",G_CALLBACK(act_install),a);
    gtrace("action_button INSTALL built for '%s' handler=%lu data=%p", a->id, h, (void*)a);
    return b;
}
/* the small round download glyph used on the mockup's tiles */
static gboolean draw_dl_glyph(GtkWidget* w,cairo_t* cr,gpointer u){
    App* a=u;
    GtkAllocation al; gtk_widget_get_allocation(w,&al);
    double S=al.width<al.height?al.width:al.height, cx=al.width/2.0, cy=al.height/2.0;
    gboolean inst=app_installed(a), upd=app_updatable(a), ok=app_compatible(a);
    const char* st=app_dlstate(a);
    gboolean busy = st && (!strcmp(st,"downloading")||!strcmp(st,"queued")||!strcmp(st,"installing"));
    if (upd || (!inst && ok && !busy)) {
        cairo_pattern_t* p=cairo_pattern_create_linear(0,0,al.width,al.height);
        cairo_pattern_add_color_stop_rgb(p,0,0.929,0.424,0.878);
        cairo_pattern_add_color_stop_rgb(p,1,0.514,0.443,0.969);
        cairo_set_source(cr,p); cairo_arc(cr,cx,cy,S/2-1,0,2*G_PI); cairo_fill(cr);
        cairo_pattern_destroy(p);
        cairo_set_source_rgb(cr,1,1,1); cairo_set_line_width(cr,1.8);
        cairo_move_to(cr,cx,cy-S*0.20); cairo_line_to(cr,cx,cy+S*0.16); cairo_stroke(cr);
        cairo_move_to(cr,cx-S*0.13,cy+0.02*S); cairo_line_to(cr,cx,cy+S*0.17);
        cairo_line_to(cr,cx+S*0.13,cy+0.02*S); cairo_stroke(cr);
    } else if (inst) {
        cairo_set_source_rgba(cr,0.204,0.780,0.349,0.16); cairo_arc(cr,cx,cy,S/2-1,0,2*G_PI); cairo_fill(cr);
        cairo_set_source_rgb(cr,0.118,0.620,0.275); cairo_set_line_width(cr,2);
        cairo_move_to(cr,cx-S*0.16,cy); cairo_line_to(cr,cx-S*0.04,cy+S*0.13);
        cairo_line_to(cr,cx+S*0.17,cy-S*0.15); cairo_stroke(cr);
    } else if (busy) {
        cairo_set_source_rgba(cr,0.514,0.443,0.969,0.18); cairo_arc(cr,cx,cy,S/2-1,0,2*G_PI); cairo_fill(cr);
        cairo_set_source_rgb(cr,0.514,0.443,0.969); cairo_set_line_width(cr,2);
        cairo_arc(cr,cx,cy,S*0.22,-G_PI/2,G_PI); cairo_stroke(cr);
    } else {
        cairo_set_source_rgba(cr,0.11,0.106,0.141,0.07); cairo_arc(cr,cx,cy,S/2-1,0,2*G_PI); cairo_fill(cr);
        cairo_set_source_rgba(cr,0.42,0.41,0.50,0.9); cairo_set_line_width(cr,1.6);
        cairo_move_to(cr,cx-S*0.12,cy); cairo_line_to(cr,cx+S*0.12,cy); cairo_stroke(cr);
    }
    return FALSE;
}
static gboolean on_dl_click(GtkWidget* w,GdkEventButton* e,gpointer u){ (void)w;(void)e;
    App* a=u;
    if (app_updatable(a)) { act_update(NULL,a); return TRUE; }
    if (app_installed(a)) { act_launch(NULL,a); return TRUE; }
    if (!app_compatible(a)) return TRUE;
    act_install(NULL,a); return TRUE;
}
static GtkWidget* dl_glyph(App* a,int size){
    GtkWidget* da=gtk_drawing_area_new();
    gtk_widget_set_size_request(da,size,size);
    gtk_widget_add_events(da,GDK_BUTTON_PRESS_MASK);
    g_signal_connect(da,"draw",G_CALLBACK(draw_dl_glyph),a);
    GtkWidget* ev=gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev),FALSE);
    gtk_container_add(GTK_CONTAINER(ev),da);
    g_signal_connect(ev,"button-press-event",G_CALLBACK(on_dl_click),a);
    gtk_widget_set_halign(ev,GTK_ALIGN_CENTER);
    return ev;
}

/* ------------------------------------------------------------- tiles ----- */
static void show_app(App* a);
static gboolean on_tile_click(GtkWidget* w,GdkEventButton* e,gpointer u){ (void)w;(void)e; show_app((App*)u); return FALSE; }

/* the vertical tile from the mockup: icon, two-line name, download glyph */
static GtkWidget* app_tile(App* a){
    GtkWidget* t=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    css(t,"xxri-tile");
    gtk_widget_set_size_request(t,104,-1);
    gtk_widget_set_valign(t,GTK_ALIGN_START);   /* natural height, not rail height */
    GtkWidget* ic=icon_img(a,60);
    gtk_widget_set_halign(ic,GTK_ALIGN_CENTER);
    GtkWidget* iev=gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(iev),FALSE);
    gtk_container_add(GTK_CONTAINER(iev),ic);
    g_signal_connect(iev,"button-press-event",G_CALLBACK(on_tile_click),a);
    gtk_box_pack_start(GTK_BOX(t),iev,FALSE,FALSE,0);
    GtkWidget* nm=lbl_wrap(a->name,"xxri-tile-name",0.5,12);
    gtk_label_set_justify(GTK_LABEL(nm),GTK_JUSTIFY_CENTER);
    gtk_label_set_lines(GTK_LABEL(nm),2);
    gtk_label_set_ellipsize(GTK_LABEL(nm),PANGO_ELLIPSIZE_END);
    GtkWidget* nev=gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(nev),FALSE);
    gtk_container_add(GTK_CONTAINER(nev),nm);
    g_signal_connect(nev,"button-press-event",G_CALLBACK(on_tile_click),a);
    gtk_box_pack_start(GTK_BOX(t),nev,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(t),dl_glyph(a,30),FALSE,FALSE,0);
    return t;
}

/* the wide card used by category / search / list pages */
static GtkWidget* app_card(App* a){
    GtkWidget* c=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
    css(c,"xxri-appcard");
    GtkWidget* ic=icon_img(a,52); gtk_widget_set_valign(ic,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(c),ic,FALSE,FALSE,0);
    GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_widget_set_valign(tv,GTK_ALIGN_CENTER);
    GtkWidget* nb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
    gtk_box_pack_start(GTK_BOX(nb),lbl_ell(a->name,"xxri-app-name",0,22),FALSE,FALSE,0);
    if (app_updatable(a)) {
        GtkWidget* b=lbl("Update","xxri-badge",0.5); gtk_widget_set_valign(b,GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(nb),b,FALSE,FALSE,0);
    } else if (app_installed(a)) {
        GtkWidget* b=lbl("Installed","xxri-badge ok",0.5); gtk_widget_set_valign(b,GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(nb),b,FALSE,FALSE,0);
    } else if (!app_compatible(a)) {
        GtkWidget* b=lbl("Regular / Pro","xxri-badge warn",0.5);
        gtk_widget_set_valign(b,GTK_ALIGN_CENTER);
        char* f=arch_join(a,"/",NULL);
        gtk_widget_set_tooltip_text(b,f); g_free(f);
        gtk_box_pack_start(GTK_BOX(nb),b,FALSE,FALSE,0);
    }
    gtk_box_pack_start(GTK_BOX(tv),nb,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(tv),lbl_ell(a->summary,"xxri-app-summary",0,52),FALSE,FALSE,0);
    char sub[192]; char* hs=human_size(a->size);
    { const char* parts[3]; int n=0;
      const char* cat=CAT_TITLE(a->category);
      if (a->developer && *a->developer) parts[n++]=a->developer;
      if (cat && *cat)                   parts[n++]=cat;
      if (hs && *hs)                     parts[n++]=hs;
      sub[0]=0;
      for (int i=0;i<n;i++) {
          if (i) g_strlcat(sub," \xc2\xb7 ",sizeof sub);
          g_strlcat(sub,parts[i],sizeof sub);
      }
    }
    g_free(hs);
    gtk_box_pack_start(GTK_BOX(tv),lbl_ell(sub,"xxri-app-dev",0,52),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(c),tv,TRUE,TRUE,0);
    GtkWidget* act=action_button(a,FALSE); gtk_widget_set_valign(act,GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(c),act,FALSE,FALSE,0);
    GtkWidget* ev=gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev),FALSE);
    gtk_container_add(GTK_CONTAINER(ev),c);
    g_signal_connect(ev,"button-press-event",G_CALLBACK(on_tile_click),a);
    return ev;
}

/* ------------------------------------------------------- lazy app grid --- */
/* Never build 950 cards.  Add a chunk, then more as the user reaches the end. */
typedef struct { GPtrArray* apps; guint shown; GtkWidget* box; GtkWidget* more; } LazyGrid;
static void lazy_free(gpointer p,GClosure* c){ (void)c; LazyGrid* g=p; g_ptr_array_free(g->apps,TRUE); g_free(g); }
static void lazy_add(LazyGrid* g,guint n){
    guint end=g->shown+n; if(end>g->apps->len) end=g->apps->len;
    for (guint i=g->shown;i<end;i++)
        gtk_box_pack_start(GTK_BOX(g->box),app_card(g->apps->pdata[i]),FALSE,FALSE,0);
    g->shown=end;
    gtk_widget_show_all(g->box);
    /* no_show_all was set on `more`, so this is the authority on its visibility */
    if (g->more) gtk_widget_set_visible(g->more,g->shown<g->apps->len);
    kick_icon_fetch();
}
static void on_more(GtkWidget* w,gpointer p){ (void)w; lazy_add((LazyGrid*)p,24); }
static void on_edge(GtkScrolledWindow* sw,GtkPositionType pos,gpointer p){ (void)sw;
    if (pos==GTK_POS_BOTTOM) lazy_add((LazyGrid*)p,24);
}
static GtkWidget* lazy_grid(GPtrArray* apps){
    LazyGrid* g=g_new0(LazyGrid,1);
    g->apps=apps; g->shown=0;
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    g->box=box;
    GtkWidget* wrap=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    gtk_box_pack_start(GTK_BOX(wrap),box,FALSE,FALSE,0);
    GtkWidget* more=gtk_button_new_with_label("Show more");
    css(more,"xxri-chip");
    gtk_widget_set_halign(more,GTK_ALIGN_CENTER);
    gtk_widget_set_no_show_all(more,TRUE);   /* lazy_add controls its visibility */
    g->more=more;
    g_signal_connect_data(more,"clicked",G_CALLBACK(on_more),g,lazy_free,0);
    gtk_box_pack_start(GTK_BOX(wrap),more,FALSE,FALSE,0);
    g_object_set_data(G_OBJECT(wrap),"lazy",g);
    lazy_add(g,24);
    return wrap;
}

/* -------------------------------------------------------- section rail --- */
/* a horizontal row of tiles, like the mockup's "Recomended apps" */
static void on_seeall(GtkWidget* w,gpointer p){ (void)w; open_page((char*)p); }
static void section_rail(GtkWidget* box,const char* title,GPtrArray* apps,const char* seeall){
    if (!apps || !apps->len) return;
    GtkWidget* hdr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_widget_set_margin_top(hdr,16);
    gtk_box_pack_start(GTK_BOX(hdr),lbl(title,"xxri-section",0),TRUE,TRUE,0);
    if (seeall) {
        GtkWidget* b=gtk_button_new_with_label("See all");
        css(b,"xxri-linkbtn");
        g_signal_connect_data(b,"clicked",G_CALLBACK(on_seeall),g_strdup(seeall),(GClosureNotify)g_free,0);
        gtk_box_pack_end(GTK_BOX(hdr),b,FALSE,FALSE,0);
    }
    gtk_box_pack_start(GTK_BOX(box),hdr,FALSE,FALSE,0);
    GtkWidget* sc=gtk_scrolled_window_new(NULL,NULL);
    /* EXTERNAL, not AUTOMATIC: a permanent grey scroll strip under every rail
       is the single biggest departure from the mockup.  The rail still scrolls
       with the wheel and by dragging, and "See all" opens the full list. */
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),GTK_POLICY_EXTERNAL,GTK_POLICY_NEVER);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sc),TRUE);
    css(sc,"xxri-rail");
    GtkWidget* row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
    int shown=0;
    for (guint i=0;i<apps->len && shown<12;i++){
        App* a=apps->pdata[i];
        if (!app_visible(a)) continue;            /* hide incompatible in rails too */
        gtk_box_pack_start(GTK_BOX(row),app_tile(a),FALSE,FALSE,0); shown++;
    }
    gtk_container_add(GTK_CONTAINER(sc),row);
    gtk_widget_set_size_request(sc,-1,158);
    if (shown) gtk_box_pack_start(GTK_BOX(box),sc,FALSE,FALSE,0);
    else { gtk_widget_destroy(sc); gtk_widget_destroy(hdr); }
}

/* ------------------------------------------------------------ queries ---- */
static GPtrArray* apps_in_cat(const char* cat){
    GPtrArray* r=g_ptr_array_new();
    for (guint i=0;i<g_apps->len;i++){ App* a=g_apps->pdata[i];
        if(!strcmp(a->category,cat) && app_visible(a)) g_ptr_array_add(r,a); }
    return r;
}
static GPtrArray* sect(const char* k){ return g_sections?g_hash_table_lookup(g_sections,k):NULL; }
static GPtrArray* apps_installed_list(void){
    GPtrArray* r=g_ptr_array_new();
    for (guint i=0;i<g_apps->len;i++){ App* a=g_apps->pdata[i]; if(app_installed(a)) g_ptr_array_add(r,a); }
    return r;
}
static GPtrArray* apps_updatable_list(void){
    GPtrArray* r=g_ptr_array_new();
    for (guint i=0;i<g_apps->len;i++){ App* a=g_apps->pdata[i]; if(app_updatable(a)) g_ptr_array_add(r,a); }
    return r;
}
/* downloads that stopped half way - the mockup's "Continue installing" */
static GPtrArray* apps_continue(void){
    GPtrArray* r=g_ptr_array_new();
    GHashTableIter it; gpointer k,v;
    if (!g_dlstate) return r;
    g_hash_table_iter_init(&it,g_dlstate);
    while (g_hash_table_iter_next(&it,&k,&v)) {
        if (strcmp(v,"paused") && strcmp(v,"failed") && strcmp(v,"queued")) continue;
        App* a=g_hash_table_lookup(g_byid,k);
        if (a) g_ptr_array_add(r,a);
    }
    return r;
}

/* --------------------------------------------------------------- hero ---- */
static gboolean draw_hero(GtkWidget* w,cairo_t* cr,gpointer u){
    guint h=GPOINTER_TO_UINT(u);
    GtkAllocation al; gtk_widget_get_allocation(w,&al);
    cairo_pattern_t* p=cairo_pattern_create_linear(0,0,al.width,al.height);
    /* three brand ramps, chosen per banner */
    if (h%3==0) { cairo_pattern_add_color_stop_rgb(p,0,0.110,0.106,0.141);
                  cairo_pattern_add_color_stop_rgb(p,1,0.325,0.278,0.671); }
    else if (h%3==1) { cairo_pattern_add_color_stop_rgb(p,0,0.929,0.424,0.878);
                       cairo_pattern_add_color_stop_rgb(p,1,0.180,0.486,0.965); }
    else { cairo_pattern_add_color_stop_rgb(p,0,0.180,0.486,0.965);
           cairo_pattern_add_color_stop_rgb(p,1,0.514,0.443,0.969); }
    rr_path(cr,0,0,al.width,al.height,18);
    cairo_set_source(cr,p); cairo_fill(cr);
    cairo_pattern_destroy(p);
    /* a soft light bloom, like the xxri wallpaper */
    cairo_pattern_t* g2=cairo_pattern_create_radial(al.width*0.82,al.height*0.15,4,
                                                    al.width*0.82,al.height*0.15,al.width*0.6);
    cairo_pattern_add_color_stop_rgba(g2,0,1,1,1,0.22);
    cairo_pattern_add_color_stop_rgba(g2,1,1,1,1,0);
    rr_path(cr,0,0,al.width,al.height,18);
    cairo_set_source(cr,g2); cairo_fill(cr); cairo_pattern_destroy(g2);
    return FALSE;
}
static GtkWidget* hero_banner(App* a,guint idx){
    GtkWidget* ov=gtk_overlay_new();
    GtkWidget* da=gtk_drawing_area_new();
    gtk_widget_set_size_request(da,376,150);
    g_signal_connect(da,"draw",G_CALLBACK(draw_hero),GUINT_TO_POINTER(idx));
    gtk_container_add(GTK_CONTAINER(ov),da);
    GtkWidget* in=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,14);
    css(in,"xxri-hero-in");
    gtk_box_pack_start(GTK_BOX(in),icon_img(a,64),FALSE,FALSE,0);
    GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);
    gtk_widget_set_valign(tv,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(tv),lbl(idx==0?"Editor's choice":"Featured","xxri-hero-kicker",0),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(tv),lbl_ell(a->name,"xxri-hero-title",0,18),FALSE,FALSE,0);
    GtkWidget* s=lbl_wrap(a->summary,"xxri-hero-sub",0,30);
    gtk_label_set_lines(GTK_LABEL(s),2);
    gtk_label_set_ellipsize(GTK_LABEL(s),PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(tv),s,FALSE,FALSE,0);
    GtkWidget* ab=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    GtkWidget* act=action_button(a,FALSE);
    css(act,"xxri-hero-btn");
    gtk_box_pack_start(GTK_BOX(ab),act,FALSE,FALSE,0);
    gtk_widget_set_margin_top(ab,4);
    gtk_box_pack_start(GTK_BOX(tv),ab,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(in),tv,TRUE,TRUE,0);
    gtk_overlay_add_overlay(GTK_OVERLAY(ov),in);
    GtkWidget* ev=gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev),FALSE);
    gtk_container_add(GTK_CONTAINER(ev),ov);
    return ev;
}

/* --------------------------------------------------------------- pages --- */
static void build_home(GtkWidget* box){
    /* hero rail - respects the hide-incompatible filter like every other rail */
    GPtrArray* feat=sect("featured");
    if (feat && feat->len) {
        GtkWidget* sc=gtk_scrolled_window_new(NULL,NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),GTK_POLICY_EXTERNAL,GTK_POLICY_NEVER);
        gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sc),TRUE);
        css(sc,"xxri-rail");
        GtkWidget* row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
        int shown=0;
        for (guint i=0;i<feat->len && shown<4;i++){
            App* a=feat->pdata[i];
            if (!app_visible(a)) continue;
            gtk_box_pack_start(GTK_BOX(row),hero_banner(a,shown),FALSE,FALSE,0); shown++;
        }
        gtk_container_add(GTK_CONTAINER(sc),row);
        gtk_widget_set_size_request(sc,-1,162);
        if (shown) gtk_box_pack_start(GTK_BOX(box),sc,FALSE,FALSE,0); else gtk_widget_destroy(sc);
    }
    /* state rails first: what the user is in the middle of */
    GPtrArray* cont=apps_continue();
    if (cont->len) section_rail(box,"Continue installing",cont,"downloads"); else g_ptr_array_free(cont,TRUE);
    GPtrArray* upd=apps_updatable_list();
    if (upd->len) section_rail(box,"Updates available",upd,"updates"); else g_ptr_array_free(upd,TRUE);

    section_rail(box,"Recommended for you",sect("recommended"),"cat:all");
    section_rail(box,"Trending",sect("trending"),"cat:all");
    section_rail(box,"Editor's choice",sect("editors"),"cat:all");
    section_rail(box,"Newest releases",sect("newest"),"cat:all");
    section_rail(box,"Recently updated",sect("updated"),"cat:all");

    GPtrArray* inst=apps_installed_list();
    if (inst->len) section_rail(box,"Your apps",inst,"installed"); else g_ptr_array_free(inst,TRUE);

    /* popular categories */
    GtkWidget* hdr=lbl("Popular categories","xxri-section",0);
    gtk_widget_set_margin_top(hdr,16);
    gtk_box_pack_start(GTK_BOX(box),hdr,FALSE,FALSE,0);
    GtkWidget* fb=gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb),GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb),4);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb),8);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb),8);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb),TRUE);
    const char* pop[]={"browser","office","development","graphics","video","audio","games","ai",NULL};
    for (int i=0;pop[i];i++) {
        GPtrArray* ap=apps_in_cat(pop[i]); int cnt=ap->len; g_ptr_array_free(ap,TRUE);
        if (!cnt) continue;
        GtkWidget* t=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); css(t,"xxri-cat-tile");
        gtk_box_pack_start(GTK_BOX(t),lbl(CAT_TITLE(pop[i]),"xxri-cat-name",0),FALSE,FALSE,0);
        char c[32]; snprintf(c,sizeof c,"%d app%s",cnt,cnt==1?"":"s");
        gtk_box_pack_start(GTK_BOX(t),lbl(c,"xxri-cat-count",0),FALSE,FALSE,0);
        GtkWidget* ev=gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev),FALSE);
        gtk_container_add(GTK_CONTAINER(ev),t);
        char pg[64]; snprintf(pg,sizeof pg,"cat:%s",pop[i]);
        g_signal_connect_data(ev,"button-press-event",G_CALLBACK(on_seeall),g_strdup(pg),(GClosureNotify)g_free,0);
        gtk_flow_box_insert(GTK_FLOW_BOX(fb),ev,-1);
    }
    gtk_box_pack_start(GTK_BOX(box),fb,FALSE,FALSE,0);
}

static void build_categories(GtkWidget* box){
    gtk_box_pack_start(GTK_BOX(box),lbl("Categories","xxri-page-title",0),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),lbl("Every app in the XXRI catalog, organised by what it does.","xxri-page-sub",0),FALSE,FALSE,0);
    GtkWidget* fb=gtk_flow_box_new();
    gtk_widget_set_margin_top(fb,12);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb),GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb),3);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb),8);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb),8);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb),TRUE);
    extern const char* ALL_CATS[];
    for (int i=0;ALL_CATS[i];i++) {
        GPtrArray* ap=apps_in_cat(ALL_CATS[i]); int cnt=ap->len; g_ptr_array_free(ap,TRUE);
        GtkWidget* t=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); css(t,"xxri-cat-tile");
        gtk_box_pack_start(GTK_BOX(t),lbl(CAT_TITLE(ALL_CATS[i]),"xxri-cat-name",0),FALSE,FALSE,0);
        char c[32]; snprintf(c,sizeof c,"%d app%s",cnt,cnt==1?"":"s");
        gtk_box_pack_start(GTK_BOX(t),lbl(c,"xxri-cat-count",0),FALSE,FALSE,0);
        GtkWidget* ev=gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev),FALSE);
        gtk_container_add(GTK_CONTAINER(ev),t);
        char pg[64]; snprintf(pg,sizeof pg,"cat:%s",ALL_CATS[i]);
        g_signal_connect_data(ev,"button-press-event",G_CALLBACK(on_seeall),g_strdup(pg),(GClosureNotify)g_free,0);
        gtk_flow_box_insert(GTK_FLOW_BOX(fb),ev,-1);
    }
    gtk_box_pack_start(GTK_BOX(box),fb,FALSE,FALSE,0);
}

static int cmp_rank(gconstpointer x,gconstpointer y){
    const App* a=*(App**)x; const App* b=*(App**)y;
    if (b->rank!=a->rank) return b->rank-a->rank;
    return g_ascii_strcasecmp(a->name,b->name);
}
static void build_category(GtkWidget* box,const char* cat){
    GPtrArray* ap;
    if (!strcmp(cat,"all")) { ap=g_ptr_array_new();
        for (guint i=0;i<g_apps->len;i++){ App* a=g_apps->pdata[i]; if(app_visible(a)) g_ptr_array_add(ap,a); } }
    else ap=apps_in_cat(cat);
    g_ptr_array_sort(ap,cmp_rank);
    gtk_box_pack_start(GTK_BOX(box),lbl(!strcmp(cat,"all")?"All apps":CAT_TITLE(cat),"xxri-page-title",0),FALSE,FALSE,0);
    int total=0; for(guint i=0;i<g_apps->len;i++){App*a=g_apps->pdata[i]; if((!strcmp(cat,"all")||!strcmp(a->category,cat))&&!app_installable(a)&&!app_installed(a)) total++; }
    char s[128];
    if (!g_show_incompat && total>0)
        snprintf(s,sizeof s,"%d app%s \xc2\xb7 %d need XXRI Regular / Pro",ap->len,ap->len==1?"":"s",total);
    else snprintf(s,sizeof s,"%d app%s",ap->len,ap->len==1?"":"s");
    gtk_box_pack_start(GTK_BOX(box),lbl(s,"xxri-page-sub",0),FALSE,FALSE,0);
    if (!ap->len) { g_ptr_array_free(ap,TRUE);
        gtk_box_pack_start(GTK_BOX(box),lbl("Nothing here yet.","xxri-hint",0),FALSE,FALSE,0);
        return; }
    gtk_box_pack_start(GTK_BOX(box),lazy_grid(ap),FALSE,FALSE,0);
}

/* ---------------------------------------------------------- app detail --- */
static void kv(GtkWidget* card,const char* k,const char* v){
    if (!v||!*v) return;
    GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(r,"xxri-rowitem");
    gtk_box_pack_start(GTK_BOX(r),lbl(k,"xxri-key",0),FALSE,FALSE,0);
    GtkWidget* val=lbl_ell(v,"xxri-val",1,40);
    gtk_box_pack_end(GTK_BOX(r),val,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(card),r,FALSE,FALSE,0);
}
static GtkWidget* g_shot_box; static char* g_shot_id;
static gboolean poll_shots(gpointer u){ (void)u;
    static int tries=0;
    if (!g_shot_box||!GTK_IS_WIDGET(g_shot_box)||!g_shot_id) { tries=0; return G_SOURCE_REMOVE; }
    int found=0;
    for (int i=0;i<3;i++) {
        char p[1024]; snprintf(p,sizeof p,"%s/shots/%s-%d.png",g_cache,g_shot_id,i);
        if (!g_file_test(p,G_FILE_TEST_EXISTS)) continue;
        found++;
    }
    GList* ch=gtk_container_get_children(GTK_CONTAINER(g_shot_box));
    int have=g_list_length(ch); g_list_free(ch);
    if (found>have) {
        ch=gtk_container_get_children(GTK_CONTAINER(g_shot_box));
        for (GList* l=ch;l;l=l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
        g_list_free(ch);
        for (int i=0;i<3;i++) {
            char p[1024]; snprintf(p,sizeof p,"%s/shots/%s-%d.png",g_cache,g_shot_id,i);
            if (!g_file_test(p,G_FILE_TEST_EXISTS)) continue;
            GdkPixbuf* full=load_png(p,0,0);
            if (!full) continue;
            int w=gdk_pixbuf_get_width(full),h=gdk_pixbuf_get_height(full);
            int th=170, tw=(int)((double)w*th/(h?h:1));
            if (tw>520){ tw=520; th=(int)((double)h*tw/(w?w:1)); }
            GdkPixbuf* sc=gdk_pixbuf_scale_simple(full,tw,th,GDK_INTERP_BILINEAR);
            g_object_unref(full);
            if (!sc) continue;
            GtkWidget* im=gtk_image_new_from_pixbuf(sc); g_object_unref(sc);
            css(im,"xxri-shot");
            gtk_box_pack_start(GTK_BOX(g_shot_box),im,FALSE,FALSE,0);
        }
        gtk_widget_show_all(g_shot_box);
    }
    if (++tries>10) { tries=0; return G_SOURCE_REMOVE; }
    return G_SOURCE_CONTINUE;
}
static void build_appdetail(GtkWidget* box,App* a){
    /* header card */
    GtkWidget* head=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,16); css(head,"xxri-card");
    GtkWidget* hr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,16); css(hr,"xxri-rowitem");
    GtkWidget* ic=icon_img(a,88); gtk_widget_set_valign(ic,GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(hr),ic,FALSE,FALSE,0);
    GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_box_pack_start(GTK_BOX(tv),lbl_ell(a->name,"xxri-page-title",0,26),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(tv),lbl_ell(a->developer,"xxri-app-dev",0,40),FALSE,FALSE,0);
    /* join only the fields that exist: the repository leaves license and size
       empty for most entries, and "Browsers \xc2\xb7 " with nothing after it
       looked like a rendering bug */
    char meta[256]; char* hs=human_size(a->size);
    { const char* parts[3]; int n=0;
      const char* cat=CAT_TITLE(a->category);
      if (cat && *cat)          parts[n++]=cat;
      if (a->license && *a->license) parts[n++]=a->license;
      if (hs && *hs)            parts[n++]=hs;
      meta[0]=0;
      for (int i=0;i<n;i++) {
          if (i) g_strlcat(meta," \xc2\xb7 ",sizeof meta);
          g_strlcat(meta,parts[i],sizeof meta);
      }
    }
    g_free(hs);
    gtk_box_pack_start(GTK_BOX(tv),lbl_ell(meta,"xxri-app-summary",0,44),FALSE,FALSE,0);
    GtkWidget* act=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_widget_set_margin_top(act,6);
    gtk_box_pack_start(GTK_BOX(act),action_button(a,TRUE),FALSE,FALSE,0);
    if (app_installed(a)) {
        if (app_updatable(a)) {
            GtkWidget* o=gtk_button_new_with_label("Open"); css(o,"xxri-chip");
            g_signal_connect(o,"clicked",G_CALLBACK(act_launch),a);
            gtk_box_pack_start(GTK_BOX(act),o,FALSE,FALSE,0);
        }
        GtkWidget* rm=gtk_button_new_with_label("Remove"); css(rm,"xxri-chip");
        g_signal_connect(rm,"clicked",G_CALLBACK(act_remove),a);
        gtk_box_pack_start(GTK_BOX(act),rm,FALSE,FALSE,0);
    }
    if (*a->homepage) {
        GtkWidget* wb=gtk_button_new_with_label("Website"); css(wb,"xxri-chip");
        g_signal_connect(wb,"clicked",G_CALLBACK(act_website),a);
        gtk_box_pack_start(GTK_BOX(act),wb,FALSE,FALSE,0);
    }
    gtk_box_pack_start(GTK_BOX(tv),act,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hr),tv,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(head),hr,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(box),head,FALSE,FALSE,0);

    /* an incompatible app must say so in plain words, not just grey out */
    if (!app_compatible(a)) {
        char* all=arch_join(a," and "," and ");
        char* pa=primary_arch(a);
        char msg[320];
        snprintf(msg,sizeof msg,
            "This app is published for %s. XXRI Lite runs %s, so it cannot be installed here \xe2\x80\x94 "
            "it is available on XXRI Regular and Pro, which are %s.",
            all,g_arch,pa);
        g_free(all); g_free(pa);
        GtkWidget* h=lbl_wrap(msg,"xxri-hint warn",0,80);
        gtk_widget_set_margin_top(h,10);
        gtk_box_pack_start(GTK_BOX(box),h,FALSE,FALSE,0);
    }

    /* screenshots (cached lazily; the section only appears if there are any) */
    if (a->nshots>0) {
        gtk_box_pack_start(GTK_BOX(box),lbl("Screenshots","xxri-section",0),FALSE,FALSE,0);
        GtkWidget* sc=gtk_scrolled_window_new(NULL,NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc),GTK_POLICY_EXTERNAL,GTK_POLICY_NEVER);
        gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sc),TRUE);
        css(sc,"xxri-rail");
        GtkWidget* row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
        gtk_container_add(GTK_CONTAINER(sc),row);
        gtk_widget_set_size_request(sc,-1,180);
        gtk_box_pack_start(GTK_BOX(box),sc,FALSE,FALSE,0);
        g_shot_box=row; g_free(g_shot_id); g_shot_id=g_strdup(a->id);
        int have=0;
        for (int i=0;i<3;i++){ char p[1024]; snprintf(p,sizeof p,"%s/shots/%s-%d.png",g_cache,a->id,i);
            if (g_file_test(p,G_FILE_TEST_EXISTS)) have++; }
        if (!have) {
            GtkWidget* ph=lbl(g_online?"Loading screenshots\xe2\x80\xa6":"Screenshots need an internet connection.","xxri-hint",0);
            gtk_box_pack_start(GTK_BOX(row),ph,FALSE,FALSE,0);
        }
        if (g_online) run_bg("xxri-store shots %s",a->id);
        g_timeout_add(1200,poll_shots,NULL);
        if (have) poll_shots(NULL);
    }

    gtk_box_pack_start(GTK_BOX(box),lbl("About","xxri-section",0),FALSE,FALSE,0);
    GtkWidget* ac=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(ac,"xxri-card");
    GtkWidget* de=lbl_wrap(*a->description?a->description:a->summary,"xxri-val",0,80);
    gtk_widget_set_margin_start(de,12); gtk_widget_set_margin_end(de,12);
    gtk_widget_set_margin_top(de,12); gtk_widget_set_margin_bottom(de,12);
    gtk_box_pack_start(GTK_BOX(ac),de,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),ac,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box),lbl("Information","xxri-section",0),FALSE,FALSE,0);
    GtkWidget* ic2=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(ic2,"xxri-card");
    kv(ic2,"Developer",a->developer);
    kv(ic2,"Publisher",*a->publisher?a->publisher:a->developer);
    kv(ic2,"Version",a->version);
    if (app_installed(a)) {
        char* low=g_ascii_strdown(a->name,-1);
        const char* iv=g_hash_table_lookup(g_installed,low); g_free(low);
        if (iv&&*iv) kv(ic2,"Installed version",iv);
    }
    kv(ic2,"Release date",a->release_date);
    kv(ic2,"Category",CAT_TITLE(a->category));
    char* archs=g_strdup(a->arch);
    for (char* p=archs;*p;p++) if(*p=='\n')*p=' ';
    kv(ic2,"Architecture",archs); g_free(archs);
    kv(ic2,"License",a->license);
    kv(ic2,"Homepage",a->homepage);
    char* hs2=human_size(a->size);
    if (*hs2) { kv(ic2,"Download size",hs2); kv(ic2,"File size",hs2); }
    g_free(hs2);
    if (*a->sha256 && strcmp(a->sha256,"-")) {
        char sh[32]; g_strlcpy(sh,a->sha256,25); g_strlcat(sh,"\xe2\x80\xa6",sizeof sh);
        kv(ic2,"SHA256",sh);
    } else kv(ic2,"SHA256","Published at download time");
    const char* srcname = !strcmp(a->source_type,"github") ? "GitHub Releases"
                        : !strcmp(a->source_type,"gitlab") ? "GitLab Releases"
                        : !strcmp(a->source_type,"direct") ? "Developer website"
                        : !strcmp(a->source_type,"local")  ? "Bundled with XXRI OS"
                        : !strcmp(a->source_type,"xxri")   ? "XXRI Repository" : a->source_type;
    kv(ic2,"Source",srcname);
    if (*a->source_ref) kv(ic2,"Source project",a->source_ref);
    kv(ic2,"Package type",!strcmp(a->kind,"appimage")?"AppImage":a->kind);
    gtk_box_pack_start(GTK_BOX(box),ic2,FALSE,FALSE,0);

    if (*a->permissions) {
        gtk_box_pack_start(GTK_BOX(box),lbl("Permissions","xxri-section",0),FALSE,FALSE,0);
        GtkWidget* pc=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(pc,"xxri-card");
        char** ps=g_strsplit(a->permissions,"\n",-1);
        for (int i=0;ps[i];i++) {
            if (!*ps[i]) continue;
            GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); css(r,"xxri-rowitem");
            gtk_box_pack_start(GTK_BOX(r),lbl(ps[i],"xxri-val",0),FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(pc),r,FALSE,FALSE,0);
        }
        g_strfreev(ps);
        gtk_box_pack_start(GTK_BOX(box),pc,FALSE,FALSE,0);
    }
    if (*a->deps) {
        gtk_box_pack_start(GTK_BOX(box),lbl("Dependencies","xxri-section",0),FALSE,FALSE,0);
        GtkWidget* dc=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(dc,"xxri-card");
        char** ds=g_strsplit(a->deps,"\n",-1);
        for (int i=0;ds[i];i++){ if(!*ds[i])continue;
            GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); css(r,"xxri-rowitem");
            gtk_box_pack_start(GTK_BOX(r),lbl(ds[i],"xxri-val",0),FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(dc),r,FALSE,FALSE,0); }
        g_strfreev(ds);
        gtk_box_pack_start(GTK_BOX(box),dc,FALSE,FALSE,0);
    }
    if (*a->changelog) {
        gtk_box_pack_start(GTK_BOX(box),lbl("What's new","xxri-section",0),FALSE,FALSE,0);
        GtkWidget* cc=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(cc,"xxri-card");
        GtkWidget* t=lbl_wrap(a->changelog,"xxri-val",0,80);
        gtk_label_set_lines(GTK_LABEL(t),14);
        gtk_label_set_ellipsize(GTK_LABEL(t),PANGO_ELLIPSIZE_END);
        gtk_widget_set_margin_start(t,12); gtk_widget_set_margin_end(t,12);
        gtk_widget_set_margin_top(t,12); gtk_widget_set_margin_bottom(t,12);
        gtk_box_pack_start(GTK_BOX(cc),t,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(box),cc,FALSE,FALSE,0);
    }
}

/* -------------------------------------------------------------- search --- */
static GtkWidget *g_res_box, *g_res_count;
static GtkWidget *g_f_cat, *g_f_arch, *g_f_pub, *g_f_inst, *g_f_upd;
static void do_search(void){
    if (!g_res_box) return;
    GList* ch=gtk_container_get_children(GTK_CONTAINER(g_res_box));
    for (GList* l=ch;l;l=l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);
    const char* q=g_search_entry?gtk_entry_get_text(GTK_ENTRY(g_search_entry)):"";
    char* ql=g_ascii_strdown(q,-1); g_strstrip(ql);
    char* cat = g_f_cat?gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(g_f_cat)):NULL;
    char* arch= g_f_arch?gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(g_f_arch)):NULL;
    char* pub = g_f_pub?gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(g_f_pub)):NULL;
    gboolean only_inst = g_f_inst && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_f_inst));
    gboolean only_upd  = g_f_upd  && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_f_upd));
    GPtrArray* r=g_ptr_array_new();
    for (guint i=0;i<g_apps->len;i++) {
        App* a=g_apps->pdata[i];
        if (*ql) {
            /* name, publisher, description, keywords, category and tags */
            char* nl=g_ascii_strdown(a->name,-1);
            char* sl=g_ascii_strdown(a->summary,-1);
            char* dl=g_ascii_strdown(a->developer,-1);
            char* pl=g_ascii_strdown(a->publisher,-1);
            char* de=g_ascii_strdown(a->description,-1);
            char* kw=g_ascii_strdown(a->keywords?a->keywords:"",-1);
            char* tg=g_ascii_strdown(a->tags,-1);
            const char* ct=CAT_TITLE(a->category);
            char* cl=g_ascii_strdown(ct,-1);
            gboolean hit = strstr(nl,ql)||strstr(sl,ql)||strstr(dl,ql)||strstr(pl,ql)
                         ||strstr(de,ql)||strstr(kw,ql)||strstr(tg,ql)||strstr(cl,ql);
            g_free(nl);g_free(sl);g_free(dl);g_free(pl);g_free(de);g_free(kw);g_free(tg);g_free(cl);
            if (!hit) continue;
        }
        if (cat && strcmp(cat,"All categories") && strcmp(CAT_TITLE(a->category),cat)) continue;
        /* hide incompatible unless the toggle is on OR the user picked an arch */
        gboolean arch_picked = arch && strcmp(arch,"Any architecture") && strcmp(arch,"Runs on this device");
        if (!g_show_incompat && !arch_picked && !app_visible(a)) continue;
        if (arch && strcmp(arch,"Any architecture")) {
            if (!strcmp(arch,"Runs on this device")) { if (!app_compatible(a)) continue; }
            else if (!strstr(a->arch,arch)) continue;
        }
        if (pub && strcmp(pub,"All publishers") && strcmp(a->developer,pub)) continue;
        if (only_inst && !app_installed(a)) continue;
        if (only_upd && !app_updatable(a)) continue;
        g_ptr_array_add(r,a);
    }
    g_ptr_array_sort(r,cmp_rank);
    if (g_res_count) {
        char c[96];
        snprintf(c,sizeof c,"%d result%s%s",r->len,r->len==1?"":"s",
                 *ql?"":" \xc2\xb7 type to narrow it down");
        gtk_label_set_text(GTK_LABEL(g_res_count),c);
    }
    if (!r->len) {
        g_ptr_array_free(r,TRUE);
        gtk_box_pack_start(GTK_BOX(g_res_box),
            lbl_wrap("No apps match that. Try a different word, or clear the filters.","xxri-hint",0,60),FALSE,FALSE,0);
    } else {
        gtk_box_pack_start(GTK_BOX(g_res_box),lazy_grid(r),FALSE,FALSE,0);
    }
    g_free(ql); g_free(cat); g_free(arch); g_free(pub);
    gtk_widget_show_all(g_res_box);
}
static void on_filter(GtkWidget* w,gpointer u){ (void)w;(void)u; do_search(); }
static void build_search(GtkWidget* box){
    gtk_box_pack_start(GTK_BOX(box),lbl("Search","xxri-page-title",0),FALSE,FALSE,0);
    /* filters: category, architecture, publisher, installed, updates */
    GtkWidget* fr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_widget_set_margin_top(fr,8);
    g_f_cat=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_cat),"All categories");
    extern const char* ALL_CATS[];
    for (int i=0;ALL_CATS[i];i++) {
        GPtrArray* ap=apps_in_cat(ALL_CATS[i]); int n=ap->len; g_ptr_array_free(ap,TRUE);
        if (n) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_cat),CAT_TITLE(ALL_CATS[i]));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_f_cat),0);
    g_signal_connect(g_f_cat,"changed",G_CALLBACK(on_filter),NULL);
    gtk_box_pack_start(GTK_BOX(fr),g_f_cat,FALSE,FALSE,0);

    g_f_arch=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_arch),"Any architecture");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_arch),"Runs on this device");
    const char* AR[]={"i686","x86_64","aarch64","arm",NULL};
    for (int i=0;AR[i];i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_arch),AR[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_f_arch),0);
    g_signal_connect(g_f_arch,"changed",G_CALLBACK(on_filter),NULL);
    gtk_box_pack_start(GTK_BOX(fr),g_f_arch,FALSE,FALSE,0);

    /* publishers: only those with more than a couple of apps, else the list
     * would be 700 entries long */
    g_f_pub=gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_pub),"All publishers");
    GHashTable* pc=g_hash_table_new(g_str_hash,g_str_equal);
    for (guint i=0;i<g_apps->len;i++) { App* a=g_apps->pdata[i];
        if (!*a->developer) continue;
        gpointer n=g_hash_table_lookup(pc,a->developer);
        g_hash_table_replace(pc,a->developer,GINT_TO_POINTER(GPOINTER_TO_INT(n)+1)); }
    GList* keys=g_hash_table_get_keys(pc);
    keys=g_list_sort(keys,(GCompareFunc)g_ascii_strcasecmp);
    for (GList* l=keys;l;l=l->next)
        if (GPOINTER_TO_INT(g_hash_table_lookup(pc,l->data))>=3)
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_f_pub),l->data);
    g_list_free(keys); g_hash_table_destroy(pc);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_f_pub),0);
    g_signal_connect(g_f_pub,"changed",G_CALLBACK(on_filter),NULL);
    gtk_box_pack_start(GTK_BOX(fr),g_f_pub,FALSE,FALSE,0);

    g_f_inst=gtk_toggle_button_new_with_label("Installed");
    css(g_f_inst,"xxri-chip");
    g_signal_connect(g_f_inst,"toggled",G_CALLBACK(on_filter),NULL);
    gtk_box_pack_start(GTK_BOX(fr),g_f_inst,FALSE,FALSE,0);
    g_f_upd=gtk_toggle_button_new_with_label("Updates");
    css(g_f_upd,"xxri-chip");
    g_signal_connect(g_f_upd,"toggled",G_CALLBACK(on_filter),NULL);
    gtk_box_pack_start(GTK_BOX(fr),g_f_upd,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),fr,FALSE,FALSE,0);

    g_res_count=lbl("","xxri-page-sub",0);
    gtk_widget_set_margin_top(g_res_count,8);
    gtk_box_pack_start(GTK_BOX(box),g_res_count,FALSE,FALSE,0);
    g_res_box=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_box_pack_start(GTK_BOX(box),g_res_box,FALSE,FALSE,0);
    do_search();
}

/* ----------------------------------------------------------- installed --- */
static void draw_glyph(cairo_t* cr,int g,double S,gboolean on);  /* rail glyphs */
/* Centred empty state, same language as Settings: a soft badge holding the
   page's own rail glyph, a bold line and a muted explanation.  The Store used
   to drop a hint bar at the top of an otherwise blank page, which read as an
   error message rather than "nothing here yet". */
typedef struct { int glyph; } EmptyBadge;
static gboolean empty_badge_draw(GtkWidget* w,cairo_t* cr,gpointer u){
    EmptyBadge* b=u;
    GtkAllocation al; gtk_widget_get_allocation(w,&al);
    double S=al.width;
    cairo_set_source_rgba(cr,0.514,0.443,0.969,0.13);
    cairo_arc(cr,S/2,S/2,S/2,0,2*G_PI); cairo_fill(cr);
    double g=S*0.46;
    cairo_save(cr); cairo_translate(cr,(S-g)/2,(S-g)/2);
    draw_glyph(cr,b->glyph,g,TRUE);
    cairo_restore(cr);
    return FALSE;
}
static GtkWidget* empty_state(int glyph,const char* title,const char* sub){
    GtkWidget* w=gtk_box_new(GTK_ORIENTATION_VERTICAL,12);
    gtk_widget_set_valign(w,GTK_ALIGN_CENTER);
    gtk_widget_set_halign(w,GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(w,40);
    GtkWidget* da=gtk_drawing_area_new();
    gtk_widget_set_size_request(da,78,78);
    gtk_widget_set_halign(da,GTK_ALIGN_CENTER);
    EmptyBadge* b=g_new0(EmptyBadge,1); b->glyph=glyph;
    g_signal_connect_data(da,"draw",G_CALLBACK(empty_badge_draw),b,(GClosureNotify)g_free,0);
    gtk_box_pack_start(GTK_BOX(w),da,FALSE,FALSE,0);
    GtkWidget* t=lbl(title,"xxri-empty-title",0.5);
    gtk_label_set_justify(GTK_LABEL(t),GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(w),t,FALSE,FALSE,0);
    GtkWidget* sl=lbl_wrap(sub,"xxri-empty-sub",0.5,46);
    gtk_label_set_justify(GTK_LABEL(sl),GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(w),sl,FALSE,FALSE,0);
    return w;
}
static void act_openloc(GtkWidget* w,gpointer p){ (void)w; run_bg("xxri-store open-location %s",(char*)p); }
static void build_installed(GtkWidget* box){
    gtk_box_pack_start(GTK_BOX(box),lbl("Installed apps","xxri-page-title",0),FALSE,FALSE,0);
    char* js=run_cmd("xxri-store installed");
    GPtrArray* a=jarr(js,"apps");
    char s[64]; snprintf(s,sizeof s,"%d app%s from the XXRI registry",a->len,a->len==1?"":"s");
    gtk_box_pack_start(GTK_BOX(box),lbl(s,"xxri-page-sub",0),FALSE,FALSE,0);
    if (!a->len) {
        gtk_box_pack_start(GTK_BOX(box),
            empty_state(5,"No apps yet",
                "Everything you install from the Store shows up here, with a Launch "
                "button and the date you installed it."),TRUE,TRUE,0);
    } else {
        GtkWidget* card=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(card,"xxri-card");
        gtk_widget_set_margin_top(card,10);
        for (guint i=0;i<a->len;i++) {
            char* o=a->pdata[i];
            char* rid=jget(o,"id"); char* nm=jget(o,"name"); char* vr=jget(o,"version");
            char* ll=jget(o,"last_launch"); char* inst=jget(o,"installed"); char* cid=jget(o,"catalog_id");
            App* ca = *cid ? g_hash_table_lookup(g_byid,cid) : NULL;
            if (!ca) for (guint k=0;k<g_apps->len;k++){ App* x=g_apps->pdata[k];
                if(!g_ascii_strcasecmp(x->name,nm)){ ca=x; break; } }
            GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12); css(r,"xxri-rowitem");
            if (ca) { GtkWidget* ic=icon_img(ca,44); gtk_widget_set_valign(ic,GTK_ALIGN_CENTER);
                gtk_box_pack_start(GTK_BOX(r),ic,FALSE,FALSE,0); }
            GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
            gtk_widget_set_valign(tv,GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(tv),lbl_ell(nm,"xxri-row-title",0,30),FALSE,FALSE,0);
            char sub[256];
            snprintf(sub,sizeof sub,"Version %s \xc2\xb7 installed %s \xc2\xb7 last opened %s",
                     vr,*inst?inst:"\xe2\x80\x94",(ll&&*ll&&strcmp(ll,"never"))?ll:"never");
            gtk_box_pack_start(GTK_BOX(tv),lbl_ell(sub,"xxri-row-sub",0,60),FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
            GtkWidget* bl=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
            gtk_widget_set_valign(bl,GTK_ALIGN_CENTER);
            if (ca && app_updatable(ca)) {
                GtkWidget* up=gtk_button_new_with_label("Update"); css(up,"xxri-accent");
                g_signal_connect(up,"clicked",G_CALLBACK(act_update),ca);
                gtk_box_pack_start(GTK_BOX(bl),up,FALSE,FALSE,0);
            }
            GtkWidget* op=gtk_button_new_with_label("Launch");
            css(op,ca&&app_updatable(ca)?"xxri-chip":"xxri-accent");
            g_object_set_data_full(G_OBJECT(op),"rid",g_strdup(rid),g_free);
            g_signal_connect(op,"clicked",G_CALLBACK(+[](GtkWidget* w,gpointer d){ (void)d;
                run_bg("xxri-app launch %s",(char*)g_object_get_data(G_OBJECT(w),"rid")); }),NULL);
            gtk_box_pack_start(GTK_BOX(bl),op,FALSE,FALSE,0);
            GtkWidget* fo=gtk_button_new_with_label("Folder"); css(fo,"xxri-chip");
            g_signal_connect_data(fo,"clicked",G_CALLBACK(act_openloc),g_strdup(rid),(GClosureNotify)g_free,0);
            gtk_box_pack_start(GTK_BOX(bl),fo,FALSE,FALSE,0);
            if (ca) {
                GtkWidget* rm=gtk_button_new_with_label("Remove"); css(rm,"xxri-chip");
                g_signal_connect(rm,"clicked",G_CALLBACK(act_remove),ca);
                gtk_box_pack_start(GTK_BOX(bl),rm,FALSE,FALSE,0);
            }
            gtk_box_pack_end(GTK_BOX(r),bl,FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(card),r,FALSE,FALSE,0);
            g_free(rid);g_free(nm);g_free(vr);g_free(ll);g_free(inst);g_free(cid);
        }
        gtk_box_pack_start(GTK_BOX(box),card,FALSE,FALSE,0);
    }
    g_ptr_array_free(a,TRUE); g_free(js);
}

/* ------------------------------------------------------------- updates --- */
static void act_update_all(GtkWidget* w,gpointer p){ (void)w;(void)p;
    run_bg("xxri-store update-all");
    open_page("downloads");
}
static void build_updates(GtkWidget* box){
    GtkWidget* hb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_box_pack_start(GTK_BOX(hb),lbl("Updates","xxri-page-title",0),TRUE,TRUE,0);
    char* js=run_cmd("xxri-store updates");
    GPtrArray* a=jarr(js,"updates");
    if (a->len) {
        GtkWidget* ua=gtk_button_new_with_label("Update all"); css(ua,"xxri-accent");
        gtk_widget_set_valign(ua,GTK_ALIGN_CENTER);
        g_signal_connect(ua,"clicked",G_CALLBACK(act_update_all),NULL);
        gtk_box_pack_end(GTK_BOX(hb),ua,FALSE,FALSE,0);
    }
    gtk_box_pack_start(GTK_BOX(box),hb,FALSE,FALSE,0);
    if (!a->len) {
        char sub[128];
        snprintf(sub,sizeof sub,"%s \xc2\xb7 %s",
                 g_online?"Checked against the online catalog":"Checked against the offline catalog",
                 g_repo_kind?g_repo_kind:"bundled");
        gtk_box_pack_start(GTK_BOX(box),lbl(sub,"xxri-page-sub",0),FALSE,FALSE,0);
        GtkWidget* h=lbl_wrap("Everything is up to date. The Store compares every installed app "
                              "against the catalog each time it opens.","xxri-hint",0,60);
        gtk_widget_set_margin_top(h,10);
        gtk_box_pack_start(GTK_BOX(box),h,FALSE,FALSE,0);
    } else {
        char s[64]; snprintf(s,sizeof s,"%d update%s available",a->len,a->len==1?"":"s");
        gtk_box_pack_start(GTK_BOX(box),lbl(s,"xxri-page-sub",0),FALSE,FALSE,0);
        GtkWidget* card=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(card,"xxri-card");
        gtk_widget_set_margin_top(card,10);
        for (guint i=0;i<a->len;i++) {
            char* o=a->pdata[i];
            char* nm=jget(o,"name"); char* cur=jget(o,"current"); char* lat=jget(o,"latest");
            char* cid=jget(o,"catalog_id"); char* szs=jget(o,"size");
            App* ca=g_hash_table_lookup(g_byid,cid);
            GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12); css(r,"xxri-rowitem");
            if (ca) { GtkWidget* ic=icon_img(ca,44); gtk_widget_set_valign(ic,GTK_ALIGN_CENTER);
                gtk_box_pack_start(GTK_BOX(r),ic,FALSE,FALSE,0); }
            GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
            gtk_box_pack_start(GTK_BOX(tv),lbl(nm,"xxri-row-title",0),FALSE,FALSE,0);
            char* hs=human_size(g_ascii_strtoll(szs,NULL,10));
            char sub[192];
            snprintf(sub,sizeof sub,"%s \xe2\x86\x92 %s%s%s",cur,lat,*hs?" \xc2\xb7 ":"",hs);
            g_free(hs);
            gtk_box_pack_start(GTK_BOX(tv),lbl(sub,"xxri-row-sub",0),FALSE,FALSE,0);
            if (ca && *ca->changelog) {
                GtkWidget* cl=lbl_ell(ca->changelog,"xxri-row-sub",0,70);
                gtk_box_pack_start(GTK_BOX(tv),cl,FALSE,FALSE,0);
            }
            gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
            GtkWidget* up=gtk_button_new_with_label("Update"); css(up,"xxri-accent");
            gtk_widget_set_valign(up,GTK_ALIGN_CENTER);
            if (ca) g_signal_connect(up,"clicked",G_CALLBACK(act_update),ca);
            else { g_object_set_data_full(G_OBJECT(up),"cid",g_strdup(cid),g_free);
                   g_signal_connect(up,"clicked",G_CALLBACK(act_update_id),NULL); }
            gtk_box_pack_end(GTK_BOX(r),up,FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(card),r,FALSE,FALSE,0);
            g_free(nm);g_free(cur);g_free(lat);g_free(cid);g_free(szs);
        }
        gtk_box_pack_start(GTK_BOX(box),card,FALSE,FALSE,0);
    }
    g_ptr_array_free(a,TRUE); g_free(js);
}

/* ----------------------------------------------------- download manager -- */
static GtkWidget* g_dl_list;
static void dl_action(GtkWidget* w,gpointer p){
    const char* verb=g_object_get_data(G_OBJECT(w),"verb");
    char* out=run_cmd("xxri-store %s %s",verb,(char*)p);
    g_free(out);
    if (!strcmp(verb,"retry")||!strcmp(verb,"resume")) run_bg("xxri-store install %s",(char*)p);
    refresh_state();
}
static gboolean poll_downloads(gpointer u){ (void)u;
    if (!g_dl_list||!GTK_IS_WIDGET(g_dl_list)) return G_SOURCE_REMOVE;
    GList* ch=gtk_container_get_children(GTK_CONTAINER(g_dl_list));
    for (GList* l=ch;l;l=l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);
    char* js=run_cmd("xxri-store downloads");
    GPtrArray* a=jarr(js,"downloads");
    if (!a->len) {
        gtk_container_add(GTK_CONTAINER(g_dl_list),
            empty_state(3,"Nothing downloading",
                "Downloads you start appear here with progress, speed and time left "
                "\xe2\x80\x94 and they survive a reboot."));
    }
    for (guint i=0;i<a->len;i++) {
        char* o=a->pdata[i];
        char* id=jget(o,"id"); char* nm=jget(o,"name"); char* st=jget(o,"state");
        char* pct=jget(o,"pct"); char* by=jget(o,"bytes"); char* tot=jget(o,"total");
        char* sp=jget(o,"speed"); char* eta=jget(o,"eta"); char* err=jget(o,"error");
        GtkWidget* r=gtk_box_new(GTK_ORIENTATION_VERTICAL,6); css(r,"xxri-rowitem");
        GtkWidget* top=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
        App* ca=g_hash_table_lookup(g_byid,id);
        if (ca) { GtkWidget* ic=icon_img(ca,36); gtk_widget_set_valign(ic,GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(top),ic,FALSE,FALSE,0); }
        GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
        gtk_box_pack_start(GTK_BOX(tv),lbl(nm,"xxri-row-title",0),FALSE,FALSE,0);
        const char* pretty = !strcmp(st,"downloading")?"Downloading" : !strcmp(st,"queued")?"Queued"
                           : !strcmp(st,"verifying")?"Verifying checksum" : !strcmp(st,"installing")?"Installing"
                           : !strcmp(st,"resolving")?"Finding the best source" : !strcmp(st,"completed")?"Completed"
                           : !strcmp(st,"paused")?"Paused" : !strcmp(st,"failed")?"Failed" : st;
        char sub[320];
        gint64 b=g_ascii_strtoll(by,NULL,10), t=g_ascii_strtoll(tot,NULL,10), s=g_ascii_strtoll(sp,NULL,10);
        char* hb=human_size(b); char* ht=human_size(t); char* hsp=human_size(s);
        int et=atoi(eta);
        if (!strcmp(st,"downloading")) {
            if (t>0 && s>0) snprintf(sub,sizeof sub,"%s \xc2\xb7 %s of %s \xc2\xb7 %s/s \xc2\xb7 %d:%02d left",
                                     pretty,hb,ht,hsp,et/60,et%60);
            else if (t>0) snprintf(sub,sizeof sub,"%s \xc2\xb7 %s of %s",pretty,hb,ht);
            else snprintf(sub,sizeof sub,"%s \xc2\xb7 %s",pretty,hb);
        } else if (*err) snprintf(sub,sizeof sub,"%s \xc2\xb7 %s",pretty,err);
        else if (!strcmp(st,"completed")&&*ht) snprintf(sub,sizeof sub,"%s \xc2\xb7 %s",pretty,ht);
        else snprintf(sub,sizeof sub,"%s",pretty);
        g_free(hb);g_free(ht);g_free(hsp);
        gtk_box_pack_start(GTK_BOX(tv),lbl_ell(sub,"xxri-row-sub",0,64),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(top),tv,TRUE,TRUE,0);
        /* per-state controls */
        GtkWidget* bl=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
        gtk_widget_set_valign(bl,GTK_ALIGN_CENTER);
        struct { const char* label; const char* verb; } acts[3]; int na=0;
        if (!strcmp(st,"downloading")) { acts[na].label="Pause"; acts[na].verb="pause"; na++;
                                         acts[na].label="Cancel"; acts[na].verb="cancel"; na++; }
        else if (!strcmp(st,"paused")) { acts[na].label="Resume"; acts[na].verb="resume"; na++;
                                         acts[na].label="Cancel"; acts[na].verb="cancel"; na++; }
        else if (!strcmp(st,"failed")) { acts[na].label="Retry"; acts[na].verb="retry"; na++;
                                         acts[na].label="Clear"; acts[na].verb="clear"; na++; }
        else if (!strcmp(st,"queued")) { acts[na].label="Cancel"; acts[na].verb="cancel"; na++; }
        else if (!strcmp(st,"completed")) { acts[na].label="Clear"; acts[na].verb="clear"; na++; }
        for (int k=0;k<na;k++) {
            GtkWidget* b=gtk_button_new_with_label(acts[k].label);
            css(b,strcmp(acts[k].verb,"retry")&&strcmp(acts[k].verb,"resume")?"xxri-chip":"xxri-accent");
            g_object_set_data_full(G_OBJECT(b),"verb",g_strdup(acts[k].verb),g_free);
            g_signal_connect_data(b,"clicked",G_CALLBACK(dl_action),g_strdup(id),(GClosureNotify)g_free,0);
            gtk_box_pack_start(GTK_BOX(bl),b,FALSE,FALSE,0);
        }
        if (!strcmp(st,"completed") && ca && app_installed(ca)) {
            GtkWidget* b=gtk_button_new_with_label("Open"); css(b,"xxri-accent");
            g_signal_connect(b,"clicked",G_CALLBACK(act_launch),ca);
            gtk_box_pack_start(GTK_BOX(bl),b,FALSE,FALSE,0);
        }
        gtk_box_pack_end(GTK_BOX(top),bl,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(r),top,FALSE,FALSE,0);
        GtkWidget* pb=gtk_progress_bar_new(); css(pb,"xxri-progress");
        double f=atoi(pct)/100.0; if(f<0)f=0; if(f>1)f=1;
        if (!strcmp(st,"completed")) f=1;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pb),f);
        if (!strcmp(st,"failed")) css(pb,"fail");
        else if (!strcmp(st,"completed")) css(pb,"done");
        gtk_box_pack_start(GTK_BOX(r),pb,FALSE,FALSE,0);
        gtk_container_add(GTK_CONTAINER(g_dl_list),r);
        g_free(id);g_free(nm);g_free(st);g_free(pct);g_free(by);g_free(tot);
        g_free(sp);g_free(eta);g_free(err);
    }
    g_ptr_array_free(a,TRUE); g_free(js);
    gtk_widget_show_all(g_dl_list);
    return G_SOURCE_CONTINUE;
}
static void act_clear_done(GtkWidget* w,gpointer p){ (void)w;(void)p;
    char* o=run_cmd("xxri-store clear"); g_free(o); poll_downloads(NULL);
}
static void build_downloads(GtkWidget* box){
    GtkWidget* hb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_box_pack_start(GTK_BOX(hb),lbl("Downloads","xxri-page-title",0),TRUE,TRUE,0);
    GtkWidget* cl=gtk_button_new_with_label("Clear finished"); css(cl,"xxri-chip");
    gtk_widget_set_valign(cl,GTK_ALIGN_CENTER);
    g_signal_connect(cl,"clicked",G_CALLBACK(act_clear_done),NULL);
    gtk_box_pack_end(GTK_BOX(hb),cl,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),hb,FALSE,FALSE,0);
    GtkWidget* card=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(card,"xxri-card");
    gtk_widget_set_margin_top(card,10);
    g_dl_list=card;
    gtk_box_pack_start(GTK_BOX(box),card,FALSE,FALSE,0);
    poll_downloads(NULL);
    g_timeout_add(1000,poll_downloads,NULL);
}

/* -------------------------------------------------------------- themes --- */
/* Phase 10+ will ship kind:"theme" entries; the page already reads the same
 * catalog, so nothing here changes when they arrive. */
static void build_kind(GtkWidget* box,const char* kind,const char* title,const char* soon){
    gtk_box_pack_start(GTK_BOX(box),lbl(title,"xxri-page-title",0),FALSE,FALSE,0);
    GPtrArray* r=g_ptr_array_new();
    for (guint i=0;i<g_apps->len;i++){ App* a=g_apps->pdata[i]; if(!strcmp(a->kind,kind)) g_ptr_array_add(r,a); }
    if (!r->len) {
        g_ptr_array_free(r,TRUE);
        GtkWidget* h=lbl_wrap(soon,"xxri-hint",0,60);
        gtk_widget_set_margin_top(h,10);
        gtk_box_pack_start(GTK_BOX(box),h,FALSE,FALSE,0);
        return;
    }
    gtk_box_pack_start(GTK_BOX(box),lazy_grid(r),FALSE,FALSE,0);
}

/* ------------------------------------------------------ page framework --- */
static GtkWidget* content_column(void){
    GtkWidget* scr=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget* outer=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(outer,"xxri-content");
    GtkWidget* col=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_widget_set_margin_start(col,22); gtk_widget_set_margin_end(col,22);
    gtk_widget_set_margin_top(col,14); gtk_widget_set_margin_bottom(col,24);
    gtk_widget_set_size_request(col,600,-1);
    g_object_set_data(G_OBJECT(scr),"col",col);
    gtk_box_pack_start(GTK_BOX(outer),col,TRUE,TRUE,0);
    gtk_container_add(GTK_CONTAINER(scr),outer);
    return scr;
}
static GtkWidget* build_page_content(const char* id){
    if (g_pending_icons) g_ptr_array_free(g_pending_icons,TRUE);
    g_pending_icons=g_ptr_array_new_with_free_func(iconwant_free);
    g_res_box=NULL; g_res_count=NULL; g_dl_list=NULL; g_shot_box=NULL;
    g_f_cat=g_f_arch=g_f_pub=g_f_inst=g_f_upd=NULL;

    GtkWidget* scr=content_column();
    GtkWidget* col=g_object_get_data(G_OBJECT(scr),"col");
    if      (!strcmp(id,"home"))       build_home(col);
    else if (!strcmp(id,"categories")) build_categories(col);
    else if (!strcmp(id,"search"))     build_search(col);
    else if (!strcmp(id,"installed"))  build_installed(col);
    else if (!strcmp(id,"updates"))    build_updates(col);
    else if (!strcmp(id,"downloads"))  build_downloads(col);
    else if (!strcmp(id,"themes"))     build_kind(col,"theme","Themes",
        "No themes in the catalog yet. When the XXRI repository starts publishing themes, "
        "wallpapers and fonts, they appear right here \xe2\x80\x94 the Store already knows how to read them.");
    else if (!strncmp(id,"cat:",4))    build_category(col,id+4);
    else if (!strncmp(id,"app:",4))    { App* a=g_hash_table_lookup(g_byid,id+4); if(a) build_appdetail(col,a); }
    /* keep the lazy grid fed as the user reaches the bottom */
    GtkWidget* wrap=NULL;
    if (g_res_box) { GList* c=gtk_container_get_children(GTK_CONTAINER(g_res_box));
        if (c) wrap=c->data; g_list_free(c); }
    if (!wrap) { GList* c=gtk_container_get_children(GTK_CONTAINER(col));
        for (GList* l=c;l;l=l->next) if (g_object_get_data(G_OBJECT(l->data),"lazy")) wrap=l->data;
        g_list_free(c); }
    if (wrap) {
        LazyGrid* lg=g_object_get_data(G_OBJECT(wrap),"lazy");
        if (lg) { gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(scr),TRUE);
                  g_signal_connect(scr,"edge-reached",G_CALLBACK(on_edge),lg); }
    }
    gtk_widget_show_all(scr);
    kick_icon_fetch();
    return scr;
}
static void rebuild(const char* id){
    if (!id) return;
    char* keep=g_strdup(id);
    GtkWidget* old=gtk_stack_get_child_by_name(GTK_STACK(g_stack),keep);
    GtkWidget* neu=build_page_content(keep);
    if (old) { gtk_container_remove(GTK_CONTAINER(g_stack),old); }
    gtk_stack_add_named(GTK_STACK(g_stack),neu,keep);
    gtk_stack_set_visible_child(GTK_STACK(g_stack),neu);
    if (g_page!=keep) { g_free(g_page); g_page=keep; }
}
static void open_page(const char* id){
    if (g_page && strcmp(g_page,id)) g_ptr_array_add(g_history,g_strdup(g_page));
    rebuild(id);
}
static void show_app(App* a){ char pg[160]; snprintf(pg,sizeof pg,"app:%s",a->id); open_page(pg); }
static void refresh_state(void){
    load_installed();
    /* free space decides whether an Install button can honestly be offered */
    char* fk = run_cmd("df -k / 2>/dev/null | awk 'NR==2{print $4}'");
    g_free_kb = g_ascii_strtoll(fk, NULL, 10);
    g_free(fk);
}

/* Live refresh: a lightweight background poll notices when the installed set
 * changes (an install finished, an update landed, a removal completed - whether
 * driven from the Store or from xxri-app elsewhere) and re-renders the visible
 * page, the rail badge and all state, with no manual refresh and no reboot.
 * Progress ticks do NOT trigger it (only the installed set + terminal download
 * states feed the signature), so active downloads are never interrupted. */
static guint g_live_sig = 0;
static guint live_signature(void){
    guint h=5381; char* p;
    char* js=run_cmd("xxri-store installed");
    for (p=js;*p;p++) h=((h<<5)+h)+(guint)(unsigned char)*p;
    g_free(js);
    char* uj=run_cmd("xxri-store updates");
    for (p=uj;*p;p++) h=((h<<5)+h)+(guint)(unsigned char)*p;
    g_free(uj);
    /* terminal download states only (not bytes), so completion shows at once */
    char* dj=run_cmd("xxri-store downloads");
    GPtrArray* d=jarr(dj,"downloads");
    for (guint i=0;i<d->len;i++){ char* id=jget(d->pdata[i],"id"); char* st=jget(d->pdata[i],"state");
        if(!strcmp(st,"completed")||!strcmp(st,"failed")||!strcmp(st,"cancelled")){
            for(p=id;*p;p++) h=((h<<5)+h)+(guint)(unsigned char)*p;
            for(p=st;*p;p++) h=((h<<5)+h)+(guint)(unsigned char)*p; }
        g_free(id); g_free(st); }
    g_ptr_array_free(d,TRUE); g_free(dj);
    return h;
}
static gboolean poll_live(gpointer u){ (void)u;
    guint s=live_signature();
    if (s==g_live_sig) return G_SOURCE_CONTINUE;
    g_live_sig=s;
    load_installed();
    if (g_rail) gtk_widget_queue_draw(g_rail);       /* updates badge dot */
    /* re-render whatever the user is looking at; the downloads page refreshes
     * itself (its own poller), so leave it alone to keep progress smooth */
    if (g_page && strcmp(g_page,"downloads")) rebuild(g_page);
    return G_SOURCE_CONTINUE;
}
static void go_back(GtkWidget* w,gpointer u){ (void)w;(void)u;
    if (!g_history->len) return;
    char* prev=g_ptr_array_index(g_history,g_history->len-1);
    g_ptr_array_remove_index(g_history,g_history->len-1);
    rebuild(prev); g_free(prev);
}

/* -------------------------------------------------------- categories map -- */
const char* ALL_CATS[]={"browser","office","development","graphics","photography","video","audio",
    "education","communication","ai","games","utilities","science","finance","security","system",
    "virtualization","networking","productivity","accessories",NULL};
static const char* CAT_TITLE(const char* id){
    static const struct{const char*i;const char*t;} M[]={
     {"browser","Browsers"},{"office","Office"},{"development","Development"},{"graphics","Graphics"},
     {"photography","Photography"},{"video","Video"},{"audio","Audio"},{"education","Education"},
     {"communication","Communication"},{"ai","AI"},{"games","Games"},{"utilities","Utilities"},
     {"science","Science"},{"finance","Finance"},{"security","Security"},{"system","System"},
     {"virtualization","Virtualization"},{"networking","Networking"},{"productivity","Productivity"},
     {"accessories","Accessories"},{NULL,NULL}};
    for (int i=0;M[i].i;i++) if(!strcmp(M[i].i,id)) return M[i].t;
    return id;
}

/* ---------------------------------------------------------------- rail --- */
/* The mockup's icon rail.  Glyphs are drawn with cairo so they stay crisp and
 * on-palette at any size - no bitmap set to keep in sync. */
typedef struct { const char* id; const char* label; int glyph; } Rail;
static const Rail RAIL[]={
    {"home","Home",0},{"categories","Categories",1},{"search","Search",2},
    {"downloads","Downloads",3},{"updates","Updates",4},{"installed","My apps",5},
};
static void draw_glyph(cairo_t* cr,int g,double S,gboolean on){
    if (on) { cairo_pattern_t* p=cairo_pattern_create_linear(0,0,S,S);
        cairo_pattern_add_color_stop_rgb(p,0,0.929,0.424,0.878);
        cairo_pattern_add_color_stop_rgb(p,1,0.514,0.443,0.969);
        cairo_set_source(cr,p); cairo_pattern_destroy(p);
    } else cairo_set_source_rgba(cr,0.25,0.24,0.33,0.85);
    cairo_set_line_width(cr,1.7);
    cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);
    switch (g) {
    case 0: /* shopping bag */
        cairo_move_to(cr,S*0.24,S*0.34); cairo_line_to(cr,S*0.76,S*0.34);
        cairo_line_to(cr,S*0.70,S*0.80); cairo_line_to(cr,S*0.30,S*0.80);
        cairo_close_path(cr); cairo_stroke(cr);
        cairo_arc(cr,S*0.50,S*0.34,S*0.14,G_PI,2*G_PI); cairo_stroke(cr);
        break;
    case 1: /* grid */
        for (int r=0;r<2;r++) for (int c=0;c<2;c++) {
            rr_path(cr,S*(0.26+c*0.26),S*(0.26+r*0.26),S*0.20,S*0.20,S*0.05);
            cairo_stroke(cr);
        }
        break;
    case 2: /* magnifier */
        cairo_arc(cr,S*0.46,S*0.46,S*0.19,0,2*G_PI); cairo_stroke(cr);
        cairo_move_to(cr,S*0.60,S*0.60); cairo_line_to(cr,S*0.76,S*0.76); cairo_stroke(cr);
        break;
    case 3: /* download */
        cairo_move_to(cr,S*0.5,S*0.24); cairo_line_to(cr,S*0.5,S*0.60); cairo_stroke(cr);
        cairo_move_to(cr,S*0.34,S*0.46); cairo_line_to(cr,S*0.5,S*0.62);
        cairo_line_to(cr,S*0.66,S*0.46); cairo_stroke(cr);
        cairo_move_to(cr,S*0.28,S*0.76); cairo_line_to(cr,S*0.72,S*0.76); cairo_stroke(cr);
        break;
    case 4: /* refresh */
        cairo_arc(cr,S*0.5,S*0.5,S*0.22,G_PI*0.15,G_PI*1.6); cairo_stroke(cr);
        cairo_move_to(cr,S*0.70,S*0.30); cairo_line_to(cr,S*0.72,S*0.50);
        cairo_line_to(cr,S*0.54,S*0.44); cairo_close_path(cr); cairo_fill(cr);
        break;
    default: /* checklist */
        rr_path(cr,S*0.26,S*0.24,S*0.48,S*0.52,S*0.10); cairo_stroke(cr);
        cairo_move_to(cr,S*0.37,S*0.50); cairo_line_to(cr,S*0.46,S*0.60);
        cairo_line_to(cr,S*0.64,S*0.38); cairo_stroke(cr);
        break;
    }
}
typedef struct { int glyph; const char* id; } RailBtn;
static gboolean draw_rail_btn(GtkWidget* w,cairo_t* cr,gpointer u){
    RailBtn* rb=u;
    GtkAllocation al; gtk_widget_get_allocation(w,&al);
    gboolean on = g_page && (!strcmp(g_page,rb->id) ||
                  (!strcmp(rb->id,"categories") && !strncmp(g_page,"cat:",4)));
    double S=al.width;
    if (on) { cairo_set_source_rgba(cr,1,1,1,0.95); rr_path(cr,2,2,al.width-4,al.height-4,13);
              cairo_fill(cr); }
    cairo_save(cr);
    cairo_translate(cr,(al.width-S)/2,(al.height-S)/2);
    draw_glyph(cr,rb->glyph,S,on);
    cairo_restore(cr);
    /* a dot when downloads or updates need attention */
    if (!strcmp(rb->id,"updates") && g_updates && g_hash_table_size(g_updates)) {
        cairo_set_source_rgb(cr,0.941,0.416,0.557);
        cairo_arc(cr,al.width-11,11,3.6,0,2*G_PI); cairo_fill(cr);
    }
    return FALSE;
}
static gboolean on_rail_click(GtkWidget* w,GdkEventButton* e,gpointer u){ (void)w;(void)e;
    RailBtn* rb=u; open_page(rb->id);
    gtk_widget_queue_draw(g_rail);
    return TRUE;
}
static GtkWidget* build_rail(void){
    GtkWidget* rail=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    css(rail,"xxri-rail-col");
    gtk_widget_set_size_request(rail,62,-1);
    GtkWidget* logo=gtk_image_new();
    GdkPixbuf* lp=load_png("/usr/local/share/xxri-settings/icons/logo-36.png",30,30);
    if (lp) { gtk_image_set_from_pixbuf(GTK_IMAGE(logo),lp); g_object_unref(lp); }
    gtk_widget_set_margin_top(logo,14); gtk_widget_set_margin_bottom(logo,8);
    gtk_box_pack_start(GTK_BOX(rail),logo,FALSE,FALSE,0);
    for (int i=0;i<(int)G_N_ELEMENTS(RAIL);i++) {
        RailBtn* rb=g_new0(RailBtn,1); rb->glyph=RAIL[i].glyph; rb->id=RAIL[i].id;
        GtkWidget* da=gtk_drawing_area_new();
        gtk_widget_set_size_request(da,44,44);
        g_signal_connect(da,"draw",G_CALLBACK(draw_rail_btn),rb);
        GtkWidget* ev=gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev),FALSE);
        gtk_widget_add_events(ev,GDK_BUTTON_PRESS_MASK);
        gtk_container_add(GTK_CONTAINER(ev),da);
        g_signal_connect(ev,"button-press-event",G_CALLBACK(on_rail_click),rb);
        gtk_widget_set_tooltip_text(ev,RAIL[i].label);
        gtk_widget_set_halign(ev,GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(rail),ev,FALSE,FALSE,0);
    }
    return rail;
}

/* -------------------------------------------------------------- header --- */
static void on_search_changed(GtkWidget* w,gpointer u){ (void)w;(void)u;
    if (!g_page || strcmp(g_page,"search")) open_page("search");
    else do_search();
    gtk_widget_grab_focus(g_search_entry);
    gtk_editable_set_position(GTK_EDITABLE(g_search_entry),-1);
}
static void update_status_chip(void){
    if (!g_status_chip) return;
    char t[200];
    if (!g_repo_available) {
        /* No catalog at all: the repository could not be reached and nothing
           was ever cached.  Say so plainly rather than showing an empty store. */
        snprintf(t,sizeof t,"Repository unavailable");
    } else if (g_repo_offline) {
        snprintf(t,sizeof t,"Repository unavailable \xc2\xb7 cached catalog%s%s",
                 (g_repo_updated&&*g_repo_updated)?" \xc2\xb7 ":"",
                 (g_repo_updated&&*g_repo_updated)?g_repo_updated:"");
    } else {
        snprintf(t,sizeof t,"Online \xc2\xb7 repo.xxri.flows.best%s%s \xc2\xb7 %s",
                 (g_repo_updated&&*g_repo_updated)?" \xc2\xb7 ":"",
                 (g_repo_updated&&*g_repo_updated)?g_repo_updated:"",
                 g_arch);
    }
    gtk_label_set_text(GTK_LABEL(g_status_chip),t);
    GtkStyleContext* sc=gtk_widget_get_style_context(g_status_chip);
    if (g_repo_available && !g_repo_offline) gtk_style_context_remove_class(sc,"off");
    else gtk_style_context_add_class(sc,"off");
}

/* startup: check connectivity + refresh the repository, without blocking */
static gboolean poll_startup(gpointer u){ (void)u;
    static int tries=0;
    char sp[512],dp[512];
    snprintf(sp,sizeof sp,"%s/status.json",g_cache);
    snprintf(dp,sizeof dp,"%s/startup.done",g_cache);
    char* doc=NULL; gsize len;
    if (g_file_get_contents(sp,&doc,&len,NULL)) {
        char* on=jget(doc,"online"); char* rk=jget(doc,"repo");
        gboolean was=g_online;
        g_online=!strcmp(on,"true");
        g_free(g_repo_kind); g_repo_kind=rk;
        g_free(on); g_free(doc);
        if (was!=g_online) update_status_chip(); else update_status_chip();
    }
    if (g_file_test(dp,G_FILE_TEST_EXISTS)) {
        g_unlink(dp);
        dbg("REFRESH_DONE marker seen after %d polls", tries);
        char rl[512]; snprintf(rl,sizeof rl,"%s/refresh.log",g_cache);
        char* log=NULL;
        /* Match what the backend actually reports.  cmd_refresh prints
           updated | unchanged | offline-cached | unavailable - it stopped
           printing "ok" when the catalog moved online, so this test silently
           never fired and the Store kept its empty startup model (and the
           "Repository unavailable" chip) even though the catalog had just been
           downloaded successfully. */
        gboolean got = g_file_get_contents(rl,&log,&len,NULL) && log &&
                       (strstr(log,"updated") || strstr(log,"unchanged") ||
                        strstr(log,"offline-cached") || strstr(log,"ok"));
        g_free(log);
        /* Belt and braces: on a fresh install the Store starts with no catalog
           at all.  If one exists now, reload regardless of what the log said -
           anything is better than the empty model we painted with. */
        if (!got && !g_repo_available) {
            char cp[1024]; snprintf(cp,sizeof cp,"%s/i686.json",g_cache);
            got = g_file_test(cp,G_FILE_TEST_EXISTS);
        }
        dbg("REFRESH_GOT=%d (log=%s)", got, "see refresh.log");
        if (got) {          /* the remote catalog replaced the bundled one */
            load_catalog(); load_installed(); rebuild(g_page);
            dbg("RELOADED apps=%u available=%d", g_apps?g_apps->len:0, g_repo_available);
        } else if (g_online) {
            kick_icon_fetch();
        }
        update_status_chip();
        tries=0; return G_SOURCE_REMOVE;
    }
    if (++tries>40) { tries=0; return G_SOURCE_REMOVE; }
    return G_SOURCE_CONTINUE;
}
static void kick_startup(void){
    char cmd[1024];
    snprintf(cmd,sizeof cmd,
        "xxri-store status > '%s/status.json' 2>/dev/null; "
        "xxri-store refresh > '%s/refresh.log' 2>&1; "
        "xxri-store status > '%s/status.json' 2>/dev/null; "
        "touch '%s/startup.done'",
        g_cache,g_cache,g_cache,g_cache);
    dbg("REFRESH_STARTED cmd=%s", cmd);
    run_bg("%s",cmd);
    g_timeout_add(700,poll_startup,NULL);
}
static void on_toggle_incompat(GtkWidget* w,gpointer u){ (void)u;
    g_show_incompat = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
    rebuild(g_page);        /* re-render the current page with the new filter */
}
static void on_refresh_click(GtkWidget* w,gpointer u){ (void)w;(void)u;
    char cmd[1024];
    snprintf(cmd,sizeof cmd,
        "xxri-store status > '%s/status.json' 2>/dev/null; "
        "xxri-store refresh --force > '%s/refresh.log' 2>&1; "
        "xxri-store status > '%s/status.json' 2>/dev/null; "
        "touch '%s/startup.done'",
        g_cache,g_cache,g_cache,g_cache);
    run_bg("%s",cmd);
    g_timeout_add(700,poll_startup,NULL);
}

static void activate(GtkApplication* app,gpointer u){ (void)u;
    GtkCssProvider* prov=gtk_css_provider_new();
    gtk_css_provider_load_from_path(prov,STORE_DIR "/xxri-store.css",NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);

    g_cache=run_cmd("xxri-store cache-path");
    if (!*g_cache) { g_free(g_cache); g_cache=g_strdup_printf("%s/.cache/xxri-store",g_get_home_dir()); }
    g_history=g_ptr_array_new();
    load_catalog(); load_installed();

    g_win=gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_win),"XXRI Store");
    /* fit the smallest supported panel, grow into anything larger */
    GdkRectangle wa; GdkMonitor* m=gdk_display_get_primary_monitor(gdk_display_get_default());
    if (m) gdk_monitor_get_workarea(m,&wa); else { wa.width=1024; wa.height=768; }
    int W=wa.width-80, H=wa.height-120;
    if (W>1080) W=1080; if (W<820) W=820;
    if (H>720) H=720;  if (H<560) H=560;
    gtk_window_set_default_size(GTK_WINDOW(g_win),W,H);
    css(g_win,"xxri-root");

    GtkWidget* hb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_container_add(GTK_CONTAINER(g_win),hb);
    g_rail=build_rail();
    gtk_box_pack_start(GTK_BOX(hb),g_rail,FALSE,FALSE,0);

    GtkWidget* right=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_box_pack_start(GTK_BOX(hb),right,TRUE,TRUE,0);

    /* header: back, wordmark, search, refresh, status chip */
    GtkWidget* head=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
    css(head,"xxri-header");
    GtkWidget* back=gtk_button_new_with_label("\xe2\x80\xb9");
    css(back,"xxri-backbtn");
    gtk_widget_set_valign(back,GTK_ALIGN_CENTER);
    g_signal_connect(back,"clicked",G_CALLBACK(go_back),NULL);
    gtk_box_pack_start(GTK_BOX(head),back,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(head),lbl("XXRI Store","xxri-wordmark",0),FALSE,FALSE,0);

    GtkWidget* sb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    css(sb,"xxri-searchbar");
    g_search_entry=gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search_entry),"Search apps, categories, publishers\xe2\x80\xa6");
    /* 34 characters made the header's minimum width 1097px, so on a 1024x768
       panel the window opened wider than the screen and the search box hung
       off the right edge.  16 is the minimum; the box still expands into
       whatever room the header has left. */
    gtk_entry_set_width_chars(GTK_ENTRY(g_search_entry),16);
    /* A cairo-drawn magnifier: the device has no icon theme and the TC
     * gdk-pixbuf cannot load GTK's fallback icon, so a themed-name icon would
     * ABORT the app on Xvesa.  Everything visual in the Store is cairo/PNG. */
    {
        cairo_surface_t* ms=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,18,18);
        cairo_t* mc=cairo_create(ms);
        cairo_set_source_rgba(mc,0.42,0.41,0.50,0.9); cairo_set_line_width(mc,1.6);
        cairo_arc(mc,7.5,7.5,4.5,0,2*G_PI); cairo_stroke(mc);
        cairo_move_to(mc,11,11); cairo_line_to(mc,15.5,15.5); cairo_stroke(mc);
        cairo_destroy(mc);
        GdkPixbuf* mp=gdk_pixbuf_get_from_surface(ms,0,0,18,18);
        cairo_surface_destroy(ms);
        if (mp) { gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(g_search_entry),GTK_ENTRY_ICON_PRIMARY,mp); g_object_unref(mp); }
    }
    g_signal_connect(g_search_entry,"changed",G_CALLBACK(on_search_changed),NULL);
    gtk_box_pack_start(GTK_BOX(sb),g_search_entry,TRUE,TRUE,0);
    gtk_widget_set_valign(sb,GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(head),sb,TRUE,TRUE,0);

    g_status_chip=lbl("","xxri-statuschip",0.5);
    gtk_label_set_ellipsize(GTK_LABEL(g_status_chip),PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(g_status_chip),30);
    gtk_widget_set_valign(g_status_chip,GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(head),g_status_chip,FALSE,FALSE,0);
    GtkWidget* rf=gtk_button_new_with_label("Refresh");
    css(rf,"xxri-chip");
    gtk_widget_set_valign(rf,GTK_ALIGN_CENTER);
    g_signal_connect(rf,"clicked",G_CALLBACK(on_refresh_click),NULL);
    gtk_box_pack_end(GTK_BOX(head),rf,FALSE,FALSE,0);
    /* architecture filter: OFF by default -> only apps this device can run */
    GtkWidget* si=gtk_toggle_button_new_with_label("Show all architectures");
    css(si,"xxri-chip");
    gtk_widget_set_valign(si,GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(si,"Also show apps built for other CPUs (they can't be installed here)");
    g_signal_connect(si,"toggled",G_CALLBACK(on_toggle_incompat),NULL);
    gtk_box_pack_end(GTK_BOX(head),si,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(right),head,FALSE,FALSE,0);

    g_stack=gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(g_stack),GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(g_stack),140);
    gtk_box_pack_start(GTK_BOX(right),g_stack,TRUE,TRUE,0);

    update_status_chip();
    gtk_widget_show_all(g_win);
    rebuild("home");
    kick_startup();
    g_live_sig = live_signature();          /* baseline; poll for live changes */
    g_timeout_add(1500, poll_live, NULL);
}

int main(int argc,char** argv){
    /* deep link: `xxri-store-gui <page>` (used by the dock, help and QA) */
    /* --dump <id>: print the catalog entry exactly as the GUI parsed it, plus
       icon resolution and the action the UI would build.  No X needed. */
    if (argc>2 && (!strcmp(argv[1],"--dump") || !strcmp(argv[1],"--click"))) {
        gboolean click = !strcmp(argv[1],"--click");
        gtk_init_check(&argc,&argv);          /* works headless too */
        g_cache = run_cmd("xxri-store cache-path");
        dbg("STARTUP pid=%d HOME=%s PATH=%s CWD=%s DISPLAY=%s",
            (int)getpid(), g_getenv("HOME"), g_getenv("PATH"),
            g_get_current_dir(), g_getenv("DISPLAY"));
        dbg("CACHE_PATH=%s", g_cache);
        g_arch  = run_cmd("xxri-store arch-name");
        if (!*g_arch) { g_free(g_arch); g_arch=g_strdup("i686"); }
        load_catalog();
        load_installed();
        App* a = g_hash_table_lookup(g_byid, argv[2]);
        if (!a) {
            printf("PARSE: id '%s' NOT FOUND in the parsed catalog (%u apps loaded)\n",
                   argv[2], g_apps?g_apps->len:0);
            return 3;
        }
        printf("=== catalog entry as parsed by the GUI: %s ===\n", argv[2]);
        printf("  id            : '%s'\n", a->id);
        printf("  name          : '%s'\n", a->name);
        printf("  category      : '%s'\n", a->category);
        printf("  kind          : '%s'\n", a->kind);
        printf("  source_type   : '%s'   (provider)\n", a->source_type);
        printf("  source_ref    : '%s'   (package id)\n", a->source_ref);
        printf("  version       : '%s'\n", a->version);
        printf("  arch          : '%s'\n", a->arch);
        printf("  size          : %lld\n", (long long)a->size);
        printf("  sha256        : '%s'\n", a->sha256);
        printf("  icon_url      : '%s'\n", a->icon_url);
        printf("  homepage      : '%s'\n", a->homepage);
        printf("  verified      : %d\n", a->verified);
        printf("  unavailable   : '%s'\n", a->unavailable?a->unavailable:"");
        printf("  desktop id    : 'xxri-app-%s.desktop'\n", a->id);
        printf("\n=== derived state ===\n");
        printf("  app_compatible : %d\n", app_compatible(a));
        printf("  app_installable: %d\n", app_installable(a));
        printf("  app_installed  : %d\n", app_installed(a));
        printf("  app_visible    : %d\n", app_visible(a));
        printf("  app_has_room   : %d   (free=%lld KB, needs=%lld KB)\n",
               app_has_room(a), (long long)g_free_kb, (long long)app_space_kb(a));
        printf("\n=== icon resolution ===\n");
        gboolean real=FALSE;
        GdkPixbuf* pb=app_icon(a,64,&real);
        printf("  pixbuf         : %s\n", pb?"OK":"NULL");
        printf("  from real file : %d  (0 = generated gradient tile)\n", real);
        char ip[1024];
        snprintf(ip,sizeof ip,"%s/%s.png",ICON_DIR,a->id);
        printf("  bundled path   : %s  (exists=%d)\n", ip, g_file_test(ip,G_FILE_TEST_EXISTS));
        printf("\n=== action the UI builds ===\n");
        if (!gdk_display_get_default()) {
            printf("  (no display: widget construction skipped)\n");
            printf("  branch it WOULD take: %s\n",
                app_updatable(a) ? "Update" :
                app_installed(a) ? "Open" :
                app_unavailable(a) ? "Currently unsupported" :
                (app_compatible(a) && !a->verified) ? "Unavailable" :
                !app_compatible(a) ? "Regular / Pro" :
                !app_has_room(a) ? "Not enough disk space" : "INSTALL");
        } else {
            GtkWidget* b=action_button(a,TRUE);
            const char* lbl = GTK_IS_BUTTON(b)?gtk_button_get_label(GTK_BUTTON(b)):"(not a button)";
            printf("  button label   : '%s'\n", lbl);
            printf("  sensitive      : %d\n", gtk_widget_get_sensitive(b));
            guint sig=g_signal_lookup("clicked",GTK_TYPE_BUTTON);
            printf("  install handler attached: %lu\n",
                   (unsigned long)g_signal_handler_find(G_OBJECT(b),G_SIGNAL_MATCH_FUNC,
                                                        sig,0,NULL,(gpointer)act_install,NULL));
        }
        if (click) {
            printf("\n=== simulating the click (exact handler body) ===\n");
            fflush(stdout);
            act_install(NULL,a);
            printf("  handler returned\n");
            g_usleep(3*1000*1000);
        }
        return 0;
    }
    const char* start = argc>1 ? argv[1] : NULL;
    GtkApplication* app=gtk_application_new("best.flows.xxri.store",G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app,"activate",G_CALLBACK(activate),NULL);
    if (start) {
        g_signal_connect_data(app,"activate",G_CALLBACK(+[](GtkApplication* a,gpointer d){ (void)a;
            open_page((const char*)d); }),(gpointer)start,NULL,G_CONNECT_AFTER);
    }
    int s=g_application_run(G_APPLICATION(app),1,argv);
    g_object_unref(app);
    return s;
}
