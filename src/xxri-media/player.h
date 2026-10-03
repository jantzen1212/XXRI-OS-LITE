/* player.h - the playback engine.
 *
 * MPlayer as a child process over its slave protocol (commands on stdin,
 * "ANS_name=value" on stdout), drawing into an X window we own. This file is
 * the only coupling to it.
 *
 * Why not libmpv: TC 16.x has no mpv package and mpv needs meson/python,
 * which the i686 build root can't run. Of what TC does package, MPlayer is
 * 16MB with deps and VLC 57MB (Qt5, GStreamer, Lua); same FFmpeg underneath.
 */
#ifndef XXRI_MEDIA_PLAYER_H
#define XXRI_MEDIA_PLAYER_H
#include <glib.h>

typedef struct XmPlayer XmPlayer;

/* Called whenever position, duration, pause state or track info changes. */
typedef void (*XmChanged)(gpointer user);

XmPlayer *xm_new(unsigned long wid, XmChanged cb, gpointer user);
void      xm_free(XmPlayer *p);

/* The engine is started lazily, and restarted if it ever dies. */
gboolean  xm_open(XmPlayer *p, const char *path);
void      xm_toggle_pause(XmPlayer *p);
void      xm_set_paused(XmPlayer *p, gboolean paused);
void      xm_seek_relative(XmPlayer *p, double seconds);
void      xm_seek_fraction(XmPlayer *p, double frac);
void      xm_set_volume(XmPlayer *p, double vol01);
void      xm_toggle_mute(XmPlayer *p);
void      xm_set_speed(XmPlayer *p, double speed);
void      xm_cycle_subtitles(XmPlayer *p);
void      xm_quit(XmPlayer *p);

/* state, all cheap reads of the last polled values */
gboolean  xm_has_media(XmPlayer *p);
/* the engine process exists and owns its window; nothing else may paint
 * there (see video_draw) */
gboolean  xm_engine_running(XmPlayer *p);
gboolean  xm_ended(XmPlayer *p);   /* played to the end; play restarts it */
gboolean  xm_paused(XmPlayer *p);
gboolean  xm_muted(XmPlayer *p);
double    xm_position(XmPlayer *p);   /* seconds */
double    xm_duration(XmPlayer *p);   /* seconds, 0 if unknown */
double    xm_volume(XmPlayer *p);     /* 0..1 */
double    xm_speed(XmPlayer *p);
const char *xm_title(XmPlayer *p);
const char *xm_current_file(XmPlayer *p);   /* or NULL */
gboolean  xm_has_video(XmPlayer *p);

#endif
