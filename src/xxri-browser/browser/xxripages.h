/*
 * xxripages.h - the browser's own pages (new tab, history), served over an
 * "xxri:" URL scheme.
 *
 * Not setHtml(): that gives an opaque origin and a render widget that never
 * takes the keyboard (the search field ate every keystroke, history links
 * didn't navigate). From a real scheme they behave like any page, and Reload
 * regenerates them.
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
