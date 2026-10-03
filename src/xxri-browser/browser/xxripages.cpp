/*
 * xxripages.cpp - see xxripages.h.
 *
 * Plain HTML in the XXRI palette; site icons are data: URIs from the same
 * cache the sidebar uses.
 */

#include "xxripages.h"
#include "xxridata.h"
#include "xxriui.h"

#include <QDateTime>
#include <QLocale>
#include <QBuffer>
#include <QPixmap>
#include <QIcon>
#include <QUrl>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QSet>
#include <cstdio>

namespace {

const char *kStyle = R"CSS(
<style>
 :root { color-scheme: light; }
 * { box-sizing: border-box; }
 body { margin:0; background:#FFFFFF; color:#1C1C1E;
        font-family:'XXRI Sans','DejaVu Sans',sans-serif; font-size:14px; }
 .wrap { max-width:620px; margin:0 auto; padding:40px 26px 48px; }
 .mark { font-size:29px; font-weight:700; letter-spacing:-0.4px; line-height:1.1; }
 .mark .x { background:linear-gradient(90deg,#EF00FF,#4400FF);
            -webkit-background-clip:text; background-clip:text; color:transparent; }
 .sub { color:#6B6B73; font-size:12.5px; margin-top:5px; }
 form { margin:22px 0 0; }
 .field { display:flex; align-items:center; gap:9px; height:40px;
          border:1px solid #AEAEAE; border-radius:20px; background:#fff;
          padding:0 16px; }
 .field:focus-within { border-color:#2E7CF6; box-shadow:0 0 0 3px rgba(46,124,246,.16); }
 .field svg { flex:0 0 auto; }
 .field input { flex:1; border:0; outline:0; font-size:14px; background:none;
                color:#1C1C1E; min-width:0; }
 h2 { font-size:12.5px; font-weight:700; letter-spacing:.03em;
      text-transform:uppercase; color:#6B6B73; margin:30px 0 10px; }
 .head { display:flex; align-items:baseline; justify-content:space-between;
         margin:30px 0 10px; }
 .head h2 { margin:0; }
 a { color:inherit; text-decoration:none; }
 .tiles { display:flex; flex-wrap:wrap; gap:14px; margin-top:24px; }
 .tile { width:76px; text-align:center; }
 .tile .sq { width:52px; height:52px; margin:0 auto 7px; border-radius:16px;
             background:#F5F5F7;
             box-shadow:none;
             display:flex; align-items:center; justify-content:center; }
 .tile:hover .sq { background:#ECECEF; }
 .tile .l { font-size:11.5px; color:#48484E; overflow:hidden;
            text-overflow:ellipsis; white-space:nowrap; }
 .row { display:flex; align-items:center; gap:11px; padding:8px 11px;
        border-radius:11px; }
 .row:hover { background:#F5F5F7; }
 .row img { flex:0 0 auto; border-radius:3px; }
 .row .t { flex:1 1 auto; min-width:0; overflow:hidden;
           text-overflow:ellipsis; white-space:nowrap; font-size:13.5px; }
 .row .u { flex:0 0 auto; color:#8E8E96; font-size:11.5px; max-width:190px;
           overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
 .row .w { flex:0 0 auto; color:#8E8E96; font-size:11.5px; width:42px;
           text-align:right; }
 .empty { color:#8E8E96; font-size:13.5px; padding:22px 11px;
          border:1px dashed rgba(0,0,0,.18); border-radius:12px;
          margin-top:14px; text-align:center; }
 .btn { font-size:11.5px; color:#2E7CF6; cursor:pointer; }
 .btn:hover { text-decoration:underline; }
</style>
)CSS";

/* Dark: the same rules in Settings' dark palette, after kStyle */
const char *kDarkStyle = R"CSS(
<style>
 :root { color-scheme: dark; }
 body { background:#242124; color:#F2F2F4; }
 .sub, h2 { color:#A0A1AA; }
 .field { border-color:#4A4B55; background:#34353E; }
 .field:focus-within { border-color:#4C93F8; box-shadow:0 0 0 3px rgba(76,147,248,.22); }
 .field input { color:#F2F2F4; }
 .tile .sq { background:#2A2B33; }
 .tile:hover .sq { background:#34353E; }
 .tile .l { color:#C8C8CF; }
 .row:hover { background:#2A2B33; }
 .row .u, .row .w, .empty { color:#8A8B94; }
 .empty { border-color:rgba(255,255,255,.18); }
 .btn { color:#4C93F8; }
</style>
)CSS";

/* the pages' style block for the Mode in use */
QString pageStyle()
{
    return QString::fromUtf8(kStyle)
            + (XxriUi::dark() ? QString::fromUtf8(kDarkStyle) : QString());
}

QString esc(const QString &s)
{
    QString o = s;
    o.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    o.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    o.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    o.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return o;
}

/* A site's own icon, or the XXRI generic mark, as a data: URI. */
QString iconUri(const QUrl &url, int px)
{
    // XXRI's own site uses the project's mark, which ships with the browser.
    QIcon ic = (url.host() == QLatin1String("xxri.flows.best"))
            ? QIcon(QStringLiteral(":/xxri/logo.png"))
            : XxriIcons::instance()->iconFor(url);
    if (ic.isNull())
        ic = XxriIcons::genericSite(px);
    const QPixmap pm = ic.pixmap(px, px);
    if (pm.isNull())
        return QString();
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return QStringLiteral("data:image/png;base64,")
            + QString::fromLatin1(data.toBase64());
}

QString head(const QString &markTail, const QString &sub)
{
    return QStringLiteral("<!doctype html><html><head><meta charset='utf-8'>"
                          "<meta name='viewport' content='width=device-width,"
                          "initial-scale=1'>"
                          "<title>XXRI %1</title>%2</head><body><div class='wrap'>"
                          "<div class='mark'><span class='x'>XXRI</span> %1</div>"
                          "<div class='sub'>%3</div>")
        .arg(esc(markTail), pageStyle(), esc(sub));
}

QString rowFor(const XxriEntry &e, const QString &time)
{
    const QString label = e.title.isEmpty() ? e.url.host() : e.title;
    return QStringLiteral("<a class='row' href='%1'>"
                          "<img src='%2' width='16' height='16' alt=''>"
                          "<span class='t'>%3</span>"
                          "<span class='u'>%4</span>%5</a>")
        .arg(esc(e.url.toString()), iconUri(e.url, 16), esc(label),
             esc(e.url.host()),
             time.isEmpty() ? QString()
                            : QStringLiteral("<span class='w'>%1</span>").arg(esc(time)));
}

} // namespace

QString XxriPages::newTab()
{
    QString h = QStringLiteral("<!doctype html><html><head><meta charset='utf-8'>"
                               "<meta name='viewport' content='width=device-width,"
                               "initial-scale=1'>"
                               "<title>New Tab</title>%1</head><body><div class='wrap'>"
                               "<div class='mark'><span class='x'>XXRI</span> Browser</div>"
                               "<div class='sub'>XXRI OS Lite</div>"
                               "<form action='%2' method='get'>"
                               "<label class='field'>"
                               "<svg width='15' height='15' viewBox='0 0 16 16' fill='none'"
                               " stroke='#8E8E96' stroke-width='1.6'>"
                               "<circle cx='7' cy='7' r='4.6'/>"
                               "<path d='M10.4 10.4 L14 14' stroke-linecap='round'/></svg>"
                               "<input type='text' name='q' "
                               "placeholder='Search the web'></label></form>")
                    .arg(pageStyle(),
                         esc(XxriStore::instance()->searchUrl()));

    const QVector<XxriEntry> sc = XxriStore::instance()->bookmarks();
    QString tiles;
    for (const XxriEntry &e : sc) {
        if (e.url.isEmpty())
            continue;
        tiles += QStringLiteral("<a class='tile' href='%1' title='%2'>"
                                "<div class='sq'><img src='%3' width='30' height='30'"
                                " alt=''></div><div class='l'>%4</div></a>")
                     .arg(esc(e.url.toString()), esc(e.url.toString()),
                          iconUri(e.url, 32), esc(e.title));
    }
    if (!tiles.isEmpty())
        h += QStringLiteral("<div class='tiles'>%1</div>").arg(tiles);

    // no bookmark list: the tiles above are the bookmarks
    const QVector<XxriEntry> bm = XxriStore::instance()->bookmarks();
    const QVector<XxriEntry> hist = XxriStore::instance()->history();
    if (!hist.isEmpty()) {
        h += QStringLiteral("<div class='head'><h2>Recent</h2>"
                            "<a class='btn' href='xxri://do/history'>All history</a></div>");
        // one row per page, newest visit first (History keeps every visit)
        QSet<QString> seen;
        int shown = 0;
        for (const XxriEntry &e : hist) {
            const QString key = e.url.adjusted(QUrl::StripTrailingSlash).toString();
            if (seen.contains(key))
                continue;
            seen.insert(key);
            h += rowFor(e, QString());
            if (++shown >= 6)
                break;
        }
    } else if (bm.isEmpty()) {
        h += QStringLiteral("<div class='empty'>Pages you visit will appear here.</div>");
    }

    if (qEnvironmentVariableIsSet("XXRI_BROWSER_PAGEDIAG")) {
        // on-page input counter, for when a screenshot can't tell if the page
        // gets input. Own variable: it changes the page, the window
        // diagnostic doesn't.
        h += QStringLiteral(
            "<script>(function(){var c=0,k=0;var s=document.querySelector('.sub');"
            "function u(){s.textContent='DIAG clicks='+c+' keys='+k;}"
            "document.addEventListener('click',function(){c++;u();},true);"
            "document.addEventListener('keydown',function(){k++;u();},true);"
            "u();})();</script>");
    }
    h += QStringLiteral("</div></body></html>");
    return h;
}

QString XxriPages::history()
{
    QString h = head(QStringLiteral("History"),
                     QStringLiteral("Everything this browser has visited"));
    const QVector<XxriEntry> hist = XxriStore::instance()->history();
    if (hist.isEmpty()) {
        h += QStringLiteral("<div class='empty'>No history yet.<br>"
                            "Pages you visit will be listed here.</div>");
    } else {
        h += QStringLiteral("<div class='head'><h2>%1 page%2</h2>"
                            "<a class='btn' href='xxri://do/clear-history'>"
                            "Clear browsing history</a></div>")
                 .arg(hist.size()).arg(hist.size() == 1 ? QString()
                                                        : QStringLiteral("s"));
        QString day;
        const QLocale loc;
        bool first = true;
        for (const XxriEntry &e : hist) {
            const QString d = e.when.date() == QDate::currentDate()
                    ? QStringLiteral("Today")
                    : (e.when.date() == QDate::currentDate().addDays(-1)
                       ? QStringLiteral("Yesterday")
                       : loc.toString(e.when.date(), QLocale::LongFormat));
            if (d != day) {
                day = d;
                h += QStringLiteral("<h2%1>%2</h2>")
                        .arg(first ? QStringLiteral(" style='margin-top:16px'")
                                   : QString(), esc(d));
                first = false;
            }
            h += rowFor(e, e.when.toString(QStringLiteral("HH:mm")));
        }
    }
    h += QStringLiteral("</div></body></html>");
    return h;
}

// ---------------------------------------------------------------------------
// The xxri: scheme
// ---------------------------------------------------------------------------

void XxriPages::registerScheme()
{
    // Braces, not parentheses: QWebEngineUrlScheme s(QByteArray(kScheme))
    // declares a function.
    QWebEngineUrlScheme s{QByteArray(kScheme)};
    s.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    // a secure origin, so the pages behave like https documents (setHtml()'s
    // untrusted data: documents left the search field dead to the keyboard)
    s.setFlags(QWebEngineUrlScheme::SecureScheme
               | QWebEngineUrlScheme::ContentSecurityPolicyIgnored);
    QWebEngineUrlScheme::registerScheme(s);
}

void XxriPages::installHandler(QWebEngineProfile *profile)
{
    if (!profile || profile->urlSchemeHandler(QByteArray(kScheme)))
        return;
    profile->installUrlSchemeHandler(QByteArray(kScheme),
                                     new XxriSchemeHandler(profile));
}

void XxriSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    const QString host = job->requestUrl().host();
    if (qEnvironmentVariableIsSet("XXRI_BROWSER_DIAG")) {
        static int n = 0;
        fprintf(stderr, "xxri-page: serve #%d %s at %s\n", ++n,
                qPrintable(host),
                qPrintable(QDateTime::currentDateTime().toString(
                               QStringLiteral("HH:mm:ss.zzz"))));
        fflush(stderr);
    }
    QString html;
    if (host == QLatin1String("newtab"))
        html = XxriPages::newTab();
    else if (host == QLatin1String("history"))
        html = XxriPages::history();
    else {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    // The buffer is parented to the job, so the engine's own lifetime rules
    // free it - a raw buffer here leaks one document per page view.
    QBuffer *buf = new QBuffer(job);
    buf->setData(html.toUtf8());
    // exactly "text/html": with "; charset=..." the engine doesn't know the
    // type and shows plain text. The charset is in the page's <meta>.
    job->reply(QByteArrayLiteral("text/html"), buf);
}
