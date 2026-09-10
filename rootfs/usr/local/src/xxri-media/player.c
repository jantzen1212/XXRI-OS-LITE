/* player.c - see player.h. */
#include "player.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

struct XmPlayer {
    unsigned long wid;
    XmChanged     cb;
    gpointer      user;

    GPid          pid;
    int           in_fd;          /* our end of MPlayer's stdin  */
    GIOChannel   *out;            /* our end of MPlayer's stdout */
    guint         out_src, poll_src, child_src;

    char         *path;
    char         *title;
    gboolean      has_media, paused, muted, has_video;
    /*
     * The engine goes back to idle at the end of a file, and an idle engine
     * answers no properties and acts on no transport command.  Without
     * noticing that, the clock froze at the last frame while the controls went
     * on pretending to work: pressing play did nothing and the seek bar moved
     * a handle attached to nothing.
     */
    gboolean      ended;
    int           silent_polls;   /* polls since the engine last answered */
    double        pos, dur, vol, speed;
    double        pending_seek;   /* <0 = none; applied once length is known */
};

/* --- talking to the engine ------------------------------------------------ */

static void send_cmd(XmPlayer *p, const char *fmt, ...)
{
    if (p->in_fd < 0) return;
    va_list ap; va_start(ap, fmt);
    char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    char *line = g_strconcat(s, "\n", NULL);
    ssize_t n = write(p->in_fd, line, strlen(line));
    (void)n;
    g_free(line); g_free(s);
}

/*
 * "pausing_keep_force" is the prefix that matters: without it a get_property
 * while paused makes MPlayer resume for a frame, so simply watching the clock
 * would quietly un-pause the film.
 */
static void ask(XmPlayer *p, const char *prop)
{
    send_cmd(p, "pausing_keep_force get_property %s", prop);
}

static gboolean poll_state(gpointer d)
{
    XmPlayer *p = d;
    /* Nothing loaded means every property is unavailable, and asking anyway
     * makes MPlayer print "Failed to get value of property" four times a
     * second for as long as the player sits idle. */
    if (p->in_fd < 0 || !p->has_media || p->ended) return G_SOURCE_CONTINUE;
    /*
     * End of file is detected by the engine going quiet, not by reading a
     * message.  MPlayer announces the end on stderr ("Failed to get value of
     * property"), and its ID_EXIT line never comes at all while it is running
     * -idle, so there is nothing on the pipe we read to key off.  What does
     * change, reliably, is that an idle engine stops answering time_pos - so
     * that is what is watched.  Roughly a second of silence is the end.
     */
    if (++p->silent_polls > 6) {
        p->ended  = TRUE;
        p->paused = TRUE;
        if (p->dur > 0) p->pos = p->dur;
        if (p->cb) p->cb(p->user);
        return G_SOURCE_CONTINUE;
    }
    ask(p, "time_pos");
    ask(p, "length");
    ask(p, "pause");
    if (!p->title) ask(p, "path");
    return G_SOURCE_CONTINUE;
}

static gboolean on_output(GIOChannel *src, GIOCondition cond, gpointer d)
{
    XmPlayer *p = d;
    if (cond & (G_IO_HUP | G_IO_ERR)) return G_SOURCE_REMOVE;

    char *line = NULL; gsize len = 0;
    while (g_io_channel_read_line(src, &line, &len, NULL, NULL) == G_IO_STATUS_NORMAL
           && line) {
        g_strchomp(line);
        if (g_str_has_prefix(line, "ANS_time_pos=")) {
            p->pos = g_ascii_strtod(line + 13, NULL);
            p->silent_polls = 0;              /* the engine is alive and playing */
        } else if (g_str_has_prefix(line, "ANS_length=")) {
            double v = g_ascii_strtod(line + 11, NULL);
            if (v > 0) p->dur = v;
        } else if (g_str_has_prefix(line, "ANS_pause=")) {
            p->paused = g_str_has_suffix(line, "yes");
        } else if (g_str_has_prefix(line, "ANS_path=")) {
            g_free(p->title);
            p->title = g_path_get_basename(line + 9);
        } else if (g_str_has_prefix(line, "ANS_ERROR=")) {
            /* a property that does not apply to this file - not a failure */
        } else if (g_str_has_prefix(line, "ID_VIDEO_ID=")) {
            p->has_video = TRUE;
        } else if (g_str_has_prefix(line, "ID_LENGTH=")) {
            double v = g_ascii_strtod(line + 10, NULL);
            if (v > 0) p->dur = v;
        } else if (g_str_has_prefix(line, "ID_PAUSED")) {
            p->paused = TRUE;
        } else if (strstr(line, "Starting playback")) {
            p->paused = FALSE;
            p->ended = FALSE;
        } else if (g_str_has_prefix(line, "ID_EXIT=")
                   || g_str_has_prefix(line, "ANS_ERROR=PROPERTY_UNAVAILABLE")) {
            if (p->has_media && !p->ended) {
                p->ended  = TRUE;
                p->paused = TRUE;
                if (p->dur > 0) p->pos = p->dur;
            }
        }
        g_free(line); line = NULL;
    }
    g_free(line);
    if (p->cb) p->cb(p->user);
    return G_SOURCE_CONTINUE;
}

static void on_child_gone(GPid pid, gint status, gpointer d)
{
    XmPlayer *p = d;
    (void)status;
    g_spawn_close_pid(pid);
    p->pid = 0;
    if (p->in_fd >= 0) { close(p->in_fd); p->in_fd = -1; }
    p->has_media = FALSE;
    if (p->cb) p->cb(p->user);
}

/*
 * The engine runs idle with no file so the window it draws into exists before
 * anything is opened, and so opening a second file is a "loadfile" rather than
 * a process restart with the flicker that would bring.
 */
static gboolean spawn_engine(XmPlayer *p)
{
    if (p->pid) return TRUE;
    char wid[32]; g_snprintf(wid, sizeof wid, "%lu", p->wid);
    char vol[16]; g_snprintf(vol, sizeof vol, "%d", (int)(p->vol * 100 + 0.5));

    char *argv[] = {
        (char*)"mplayer",
        (char*)"-slave", (char*)"-idle", (char*)"-quiet",
        (char*)"-noconsolecontrols",
        (char*)"-input", (char*)"nodefault-bindings",
        (char*)"-nostop-xscreensaver",
        (char*)"-identify",
        /* Our own chrome draws every indicator, so the engine draws none. */
        (char*)"-osdlevel", (char*)"0",
        /* Software volume: the slider belongs to this application and must not
         * move the system mixer that xxri-audio set up for everything else. */
        (char*)"-softvol", (char*)"-volume", vol,
        (char*)"-ao", (char*)"alsa",
        (char*)"-vo", (char*)"x11",
        /* Scale the picture to the window.  Without -zoom the x11 output
         * blits at the file's own size and a 640x360 clip sits in the middle
         * of the viewport surrounded by black, where the artwork has the
         * picture filling it. */
        (char*)"-zoom",
        (char*)"-wid", wid,
        NULL
    };
    GError *err = NULL;
    int outfd = -1;
    if (!g_spawn_async_with_pipes(NULL, argv, NULL,
            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
            NULL, NULL, &p->pid, &p->in_fd, &outfd, NULL, &err)) {
        g_warning("xxri-media: cannot start the playback engine: %s",
                  err ? err->message : "?");
        if (err) g_error_free(err);
        return FALSE;
    }
    p->out = g_io_channel_unix_new(outfd);
    g_io_channel_set_flags(p->out, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_encoding(p->out, NULL, NULL);
    g_io_channel_set_close_on_unref(p->out, TRUE);
    p->out_src = g_io_add_watch(p->out, G_IO_IN | G_IO_HUP | G_IO_ERR,
                                on_output, p);
    p->child_src = g_child_watch_add(p->pid, on_child_gone, p);
    if (!p->poll_src)
        p->poll_src = g_timeout_add(200, poll_state, p);
    return TRUE;
}

/* --- interface ------------------------------------------------------------ */

XmPlayer *xm_new(unsigned long wid, XmChanged cb, gpointer user)
{
    XmPlayer *p = g_new0(XmPlayer, 1);
    p->wid = wid; p->cb = cb; p->user = user;
    p->in_fd = -1; p->vol = 0.85; p->speed = 1.0; p->paused = TRUE;
    p->pending_seek = -1;
    return p;
}

void xm_free(XmPlayer *p)
{
    if (!p) return;
    xm_quit(p);
    g_free(p->path); g_free(p->title);
    g_free(p);
}

gboolean xm_open(XmPlayer *p, const char *path)
{
    if (!path || !*path) return FALSE;
    if (!spawn_engine(p)) return FALSE;
    g_free(p->path); p->path = g_strdup(path);
    g_free(p->title); p->title = g_path_get_basename(path);
    p->pos = 0; p->dur = 0; p->has_video = FALSE;
    p->has_media = TRUE; p->paused = FALSE; p->ended = FALSE;
    p->silent_polls = 0;
    /* Quoted: a path with spaces is one argument, not several. */
    send_cmd(p, "loadfile \"%s\"", path);
    send_cmd(p, "speed_set %.3f", p->speed);
    if (p->cb) p->cb(p->user);
    return TRUE;
}

/* A finished file is reloaded before anything is asked of it, so play and
 * seek mean what they say rather than silently doing nothing. */
static gboolean rewind_if_ended(XmPlayer *p)
{
    if (!p->ended || !p->path) return FALSE;
    p->ended = FALSE; p->pos = 0; p->paused = FALSE; p->silent_polls = 0;
    send_cmd(p, "loadfile \"%s\"", p->path);
    send_cmd(p, "speed_set %.3f", p->speed);
    send_cmd(p, "pausing_keep_force volume %d 1", (int)(p->vol * 100 + 0.5));
    if (p->muted) send_cmd(p, "pausing_keep_force mute 1");
    return TRUE;
}

void xm_toggle_pause(XmPlayer *p)
{
    if (!p->has_media) return;
    if (rewind_if_ended(p)) { if (p->cb) p->cb(p->user); return; }
    send_cmd(p, "pause");
    p->paused = !p->paused;                 /* echoed back by the next poll */
    if (p->cb) p->cb(p->user);
}

void xm_set_paused(XmPlayer *p, gboolean paused)
{
    if (!p->has_media || p->paused == paused) return;
    xm_toggle_pause(p);
}

void xm_seek_relative(XmPlayer *p, double seconds)
{
    if (!p->has_media) return;
    if (rewind_if_ended(p)) { xm_set_paused(p, TRUE); }
    send_cmd(p, "pausing_keep seek %.3f 0", seconds);
    p->pos += seconds;
    if (p->pos < 0) p->pos = 0;
    if (p->dur > 0 && p->pos > p->dur) p->pos = p->dur;
    if (p->cb) p->cb(p->user);
}

void xm_seek_fraction(XmPlayer *p, double frac)
{
    if (!p->has_media) return;
    rewind_if_ended(p);
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    /* Percent rather than seconds: it is exact even before the duration of a
     * stream is known, and MPlayer accepts it for everything seekable. */
    send_cmd(p, "pausing_keep seek %.2f 1", frac * 100.0);
    if (p->dur > 0) p->pos = frac * p->dur;
    if (p->cb) p->cb(p->user);
}

void xm_set_volume(XmPlayer *p, double v)
{
    if (v < 0) v = 0; if (v > 1) v = 1;
    p->vol = v;
    if (p->muted && v > 0) { p->muted = FALSE; send_cmd(p, "mute 0"); }
    send_cmd(p, "pausing_keep_force volume %d 1", (int)(v * 100 + 0.5));
    if (p->cb) p->cb(p->user);
}

void xm_toggle_mute(XmPlayer *p)
{
    p->muted = !p->muted;
    send_cmd(p, "pausing_keep_force mute %d", p->muted ? 1 : 0);
    if (p->cb) p->cb(p->user);
}

void xm_set_speed(XmPlayer *p, double s)
{
    if (s < 0.25) s = 0.25; if (s > 4.0) s = 4.0;
    p->speed = s;
    send_cmd(p, "pausing_keep_force speed_set %.3f", s);
    if (p->cb) p->cb(p->user);
}

void xm_cycle_subtitles(XmPlayer *p)
{
    if (!p->has_media) return;
    send_cmd(p, "pausing_keep_force sub_select");
}

void xm_quit(XmPlayer *p)
{
    if (p->poll_src)  { g_source_remove(p->poll_src);  p->poll_src = 0; }
    if (p->out_src)   { g_source_remove(p->out_src);   p->out_src = 0; }
    if (p->child_src) { g_source_remove(p->child_src); p->child_src = 0; }
    if (p->in_fd >= 0) { send_cmd(p, "quit"); close(p->in_fd); p->in_fd = -1; }
    if (p->out) { g_io_channel_unref(p->out); p->out = NULL; }
    if (p->pid) { kill(p->pid, SIGTERM); g_spawn_close_pid(p->pid); p->pid = 0; }
    p->has_media = FALSE;
}

gboolean    xm_has_media(XmPlayer *p) { return p->has_media; }
gboolean    xm_ended(XmPlayer *p)     { return p->ended; }
gboolean    xm_engine_running(XmPlayer *p) { return p->pid != 0; }
gboolean    xm_paused(XmPlayer *p)    { return p->paused; }
gboolean    xm_muted(XmPlayer *p)     { return p->muted; }
double      xm_position(XmPlayer *p)  { return p->pos; }
double      xm_duration(XmPlayer *p)  { return p->dur; }
double      xm_volume(XmPlayer *p)    { return p->vol; }
double      xm_speed(XmPlayer *p)     { return p->speed; }
const char *xm_title(XmPlayer *p)     { return p->title ? p->title : ""; }
const char *xm_current_file(XmPlayer *p) { return p->path; }
gboolean    xm_has_video(XmPlayer *p) { return p->has_video; }
