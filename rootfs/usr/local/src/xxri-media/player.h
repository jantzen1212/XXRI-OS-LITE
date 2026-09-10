/* player.h - the playback engine, behind one small interface.
 *
 * XXRI does not decode anything itself.  MPlayer does the demuxing, decoding,
 * seeking, subtitles and audio, and this file is the whole of the coupling to
 * it: a child process driven over its slave protocol (commands on stdin,
 * "ANS_name=value" replies on stdout) rendering into an X window we own.
 *
 * WHY MPLAYER AND NOT LIBMPV.  libmpv was the first choice and is not
 * available: the Tiny Core 16.x x86 repository has no mpv or libmpv package,
 * and mpv builds with meson, which is Python - and TC's python3 extension
 * ships libraries with no interpreter, so the i686 build root cannot run it.
 * The alternatives that ARE packaged for i686 are MPlayer (1.9MB, 46 new
 * extensions, 16MB) and VLC (6.6MB, 54 new extensions, 57MB, and it drags in
 * Qt5, GStreamer and Lua).  On an OS whose whole point is being light, MPlayer
 * is the one that fits; it is the same FFmpeg decoders underneath either way.
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
/* True once the engine process exists and owns the window it was given.  From
 * that moment nothing else may paint there - see video_draw. */
gboolean  xm_engine_running(XmPlayer *p);
gboolean  xm_ended(XmPlayer *p);   /* played to the end; play restarts it */
gboolean  xm_paused(XmPlayer *p);
gboolean  xm_muted(XmPlayer *p);
double    xm_position(XmPlayer *p);   /* seconds */
double    xm_duration(XmPlayer *p);   /* seconds, 0 if unknown */
double    xm_volume(XmPlayer *p);     /* 0..1 */
double    xm_speed(XmPlayer *p);
const char *xm_title(XmPlayer *p);
/* The file currently loaded, or NULL - what "Save a Copy" copies. */
const char *xm_current_file(XmPlayer *p);
gboolean  xm_has_video(XmPlayer *p);

#endif
