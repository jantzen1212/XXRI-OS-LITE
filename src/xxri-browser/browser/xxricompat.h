/*
 * xxricompat.h - web-platform compatibility for Chromium 87.
 *
 * XXRI runs on i686, and Chromium 87 is the last branch that still builds for
 * it.  Chromium 87 is a complete, correct browser engine - its TLS, layout,
 * JavaScript and media stacks all work - but it predates a handful of small
 * JavaScript standard-library additions from 2021 and 2022.  Modern sites use
 * them unguarded, and a single missing function throws during start-up and
 * leaves the page unbuilt: GitHub, for instance, renders nothing at all
 * because `crypto.randomUUID` and `Array.prototype.at` are missing.
 *
 * These are library functions, not engine capabilities, so they can simply be
 * supplied.  The script below is injected into every page before the page's
 * own scripts run, and each polyfill is guarded - where the engine already
 * has the function, nothing is replaced.
 */

#ifndef XXRICOMPAT_H
#define XXRICOMPAT_H

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QWebEngineProfile;
QT_END_NAMESPACE

namespace XxriCompat {
    /* Installs the polyfill script and the user agent on a profile. */
    void install(QWebEngineProfile *profile);
}

#endif // XXRICOMPAT_H
