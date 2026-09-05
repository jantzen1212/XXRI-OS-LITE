/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "browser.h"
#include "browserwindow.h"
#include "tabwidget.h"
#include "xxripages.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QWebEngineProfile>
#include "xxricompat.h"
#include <QWebEngineSettings>

QUrl commandLineUrlArgument()
{
    const QStringList args = QCoreApplication::arguments();
    for (const QString &arg : args.mid(1)) {
        // The same classification the address bar uses, so `xxri-browser
        // github.com` and typing it into the field cannot disagree.
        if (!arg.startsWith(QLatin1Char('-')))
            return BrowserWindow::xxriResolveInput(arg);
    }
    // No argument: the window opens on the XXRI start page it built for
    // itself, so nothing is loaded from the network before the user asks.
    return QUrl();
}

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("XXRI"));
    QCoreApplication::setApplicationName(QStringLiteral("xxri-browser"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("XXRI Browser"));
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    /*
     * An alpha channel on the default surface format.
     *
     * The XXRI shell is translucent where the artwork is translucent, and this
     * window is not an ordinary widget window: Qt WebEngine's view is a
     * QQuickWidget, which puts the whole window through Qt's OpenGL
     * compose-and-flush path.  In that path the window's own alpha comes from
     * the GL surface, so a surface configured without alpha bits throws it
     * away no matter what the widgets painted, and WA_TranslucentBackground
     * has no effect at all.  This has to be set before the first window is
     * created, which means before QApplication.
     */
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    // The browser's own pages are served over a real scheme; Qt requires the
    // scheme to be registered before the first QtWebEngine object exists.
    XxriPages::registerScheme();

    QApplication app(argc, argv);
    // Use the XXRI Browser SVG icon for the application
    QIcon appIcon(QStringLiteral(":/xxri/browser.svg"));
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral(":/xxri/appicon.png"));
    }
    app.setWindowIcon(appIcon);

    // Enable modern web features for website compatibility
    auto *settings = QWebEngineSettings::defaultSettings();
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::XSSAuditingEnabled, true);
    settings->setAttribute(QWebEngineSettings::SpatialNavigationEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, true);
    settings->setAttribute(QWebEngineSettings::TouchIconsEnabled, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, false);
    Browser::profile()->setUseForGlobalCertificateVerification();
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings->setAttribute(QWebEngineSettings::AllowGeolocationOnInsecureOrigins, false);
#endif
    // Chromium 87 predates a handful of JavaScript standard-library additions
    // that modern sites call unguarded; supply them before anything loads.
    XxriCompat::install(Browser::profile());
    XxriPages::installHandler(Browser::profile());

    QUrl url = commandLineUrlArgument();

    Browser browser;
    BrowserWindow *window = browser.createWindow();
    if (!url.isEmpty())
        window->tabWidget()->setUrl(url);

    return app.exec();
}
