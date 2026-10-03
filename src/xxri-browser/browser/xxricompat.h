/*
 * xxricompat.h - JS polyfills for Chromium 87, the last Chromium that builds
 * for i686. It lacks a few 2021/22 library functions that modern sites call
 * unguarded (GitHub renders nothing without crypto.randomUUID and
 * Array.prototype.at). Injected before page scripts; every polyfill is
 * guarded, so nothing the engine has is replaced.
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
