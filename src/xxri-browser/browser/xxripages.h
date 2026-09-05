/*
 * xxripages.h - the browser's own pages (new tab, history).
 *
 * These are served over a registered "xxri:" URL scheme rather than pushed
 * into a view with setHtml().
 *
 * That is not a stylistic choice.  A setHtml() document is loaded as data with
 * a base URL, which gives it an opaque origin and a render widget that never
 * takes the keyboard: on the booted image the start page's search field showed
 * its focus ring and then swallowed every keystroke, and a click on a history
 * row did not navigate.  Served from a real scheme the pages are ordinary
 * documents - they type, they follow links, they reload, and Reload
 * regenerates them, so History is current every time it is opened.
 */

#ifndef XXRIPAGES_H
#define XXRIPAGES_H

#include <QString>
#include <QWebEngineUrlSchemeHandler>

QT_BEGIN_NAMESPACE
class QWebEngineProfile;
QT_END_NAMESPACE

namespace XxriPages {
    /* The scheme these pages live on.  "xxri://newtab/", "xxri://history/". */
    const char *const kScheme  = "xxri";
    const char *const kNewTab  = "xxri://newtab/";
    const char *const kHistory = "xxri://history/";

    QString newTab();
    QString history();

    /*
     * Registers the scheme.  Qt requires this before the first QtWebEngine
     * object exists, which in practice means before QApplication.
     */
    void registerScheme();

    /* Installs the handler that answers for it. */
    void installHandler(QWebEngineProfile *profile);
}

/* Answers xxri:// requests with a freshly generated page. */
class XxriSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    explicit XxriSchemeHandler(QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent) {}
    void requestStarted(QWebEngineUrlRequestJob *job) override;
};

#endif // XXRIPAGES_H
