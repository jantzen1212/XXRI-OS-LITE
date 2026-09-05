/*
 * xxridata.cpp - see xxridata.h.
 */

#include "xxridata.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>

namespace {
/*
 * The bookmarks a fresh profile starts with - the tiles the artwork draws.
 * Four of its five are legible; the fifth is not, so rather than invent a site
 * that position is where the add tile falls, which is also how the grid grows.
 * No third-party logo is bundled: each tile shows the site's own favicon.
 */
struct Seed { const char *title; const char *url; };
const Seed kSeeds[] = {
    { "XXRI OS Lite", "https://xxri.flows.best"        },
    { "GitHub",       "https://github.com"             },
    { "Google",       "https://www.google.com"         },
    { "XDA",          "https://www.xda-developers.com" },
};
const int kMaxHistory = 500;
}

XxriStore::XxriStore()
{
    load();
}

XxriStore *XxriStore::instance()
{
    static XxriStore *s = new XxriStore;
    return s;
}

QString XxriStore::filePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/.config/xxri-browser");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/xxri-browser.conf");
}

static void readGroup(QSettings &s, const QString &group, QVector<XxriEntry> *out)
{
    const int n = s.beginReadArray(group);
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        XxriEntry e;
        e.title = s.value(QStringLiteral("title")).toString();
        e.url   = QUrl(s.value(QStringLiteral("url")).toString());
        e.when  = s.value(QStringLiteral("when")).toDateTime();
        out->append(e);
    }
    s.endArray();
}

static void writeGroup(QSettings &s, const QString &group,
                       const QVector<XxriEntry> &in)
{
    s.beginWriteArray(group, in.size());
    for (int i = 0; i < in.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("title"), in.at(i).title);
        s.setValue(QStringLiteral("url"), in.at(i).url.toString());
        s.setValue(QStringLiteral("when"), in.at(i).when);
    }
    s.endArray();
}

void XxriStore::setSearchUrl(const QString &url)
{
    if (url.isEmpty() || url == m_searchUrl)
        return;
    m_searchUrl = url;
    save();
    emit searchEngineChanged();
}

void XxriStore::load()
{
    QSettings s(filePath(), QSettings::IniFormat);
    m_searchUrl = s.value(QStringLiteral("search/url"),
                          QStringLiteral("https://www.google.com/search"))
                    .toString();
    readGroup(s, QStringLiteral("bookmarks"), &m_bookmarks);
    readGroup(s, QStringLiteral("history"), &m_history);

    /*
     * A profile written by an earlier version kept its tiles in a separate
     * "shortcuts" group.  They were bookmarks in everything but name, so they
     * are read in behind whatever is already bookmarked rather than dropped.
     */
    QVector<XxriEntry> legacy;
    readGroup(s, QStringLiteral("shortcuts"), &legacy);
    for (const XxriEntry &e : qAsConst(legacy)) {
        if (e.url.isEmpty() || isBookmarked(e.url))
            continue;
        m_bookmarks.append(e);
    }

    if (m_bookmarks.isEmpty()) {
        for (const Seed &sd : kSeeds) {
            XxriEntry e;
            e.title = QString::fromUtf8(sd.title);
            e.url   = QUrl(QString::fromLatin1(sd.url));
            m_bookmarks.append(e);
        }
    }
}

void XxriStore::save()
{
    QSettings s(filePath(), QSettings::IniFormat);
    s.setValue(QStringLiteral("search/url"), m_searchUrl);
    s.remove(QStringLiteral("shortcuts"));      // migrated into bookmarks
    writeGroup(s, QStringLiteral("bookmarks"), m_bookmarks);
    writeGroup(s, QStringLiteral("history"), m_history);
    s.sync();
}

bool XxriStore::isBookmarked(const QUrl &url) const
{
    for (const XxriEntry &e : m_bookmarks)
        if (e.url == url)
            return true;
    return false;
}

void XxriStore::addBookmark(const XxriEntry &e)
{
    if (!e.url.isValid() || e.url.isEmpty())
        return;
    // Bookmarking the same page twice moves it rather than duplicating it.
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks.at(i).url == e.url) {
            m_bookmarks.remove(i);
            break;
        }
    }
    XxriEntry n = e;
    if (!n.when.isValid())
        n.when = QDateTime::currentDateTime();
    // Appended, not prepended: the grid's "+" is at the end, so a bookmark
    // added from it appears where the user was just looking.
    m_bookmarks.append(n);
    save();
    emit bookmarksChanged();
}

void XxriStore::replaceBookmark(int index, const XxriEntry &e)
{
    if (index < 0 || index >= m_bookmarks.size() || !e.url.isValid())
        return;
    XxriEntry n = e;
    n.when = m_bookmarks.at(index).when;
    m_bookmarks[index] = n;
    save();
    emit bookmarksChanged();
}

void XxriStore::removeBookmark(int index)
{
    if (index < 0 || index >= m_bookmarks.size())
        return;
    m_bookmarks.remove(index);
    save();
    emit bookmarksChanged();
}

void XxriStore::toggleBookmark(const XxriEntry &e)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks.at(i).url == e.url) {
            m_bookmarks.remove(i);
            save();
            emit bookmarksChanged();
            return;
        }
    }
    XxriEntry n = e;
    if (!n.when.isValid())
        n.when = QDateTime::currentDateTime();
    m_bookmarks.append(n);
    save();
    emit bookmarksChanged();
}

void XxriStore::clearBookmarks()
{
    if (m_bookmarks.isEmpty())
        return;
    m_bookmarks.clear();
    save();
    emit bookmarksChanged();
}

void XxriStore::noteVisit(const XxriEntry &e)
{
    if (!e.url.isValid() || e.url.scheme() == QLatin1String("about"))
        return;
    /*
     * Collapse a reload or a redirect chain onto one entry.  "google.com" and
     * "google.com/" are the same visit, and a redirect that only adds the
     * trailing slash must not leave two rows in History.
     */
    const QUrl::FormattingOptions norm = QUrl::StripTrailingSlash
                                       | QUrl::NormalizePathSegments;
    auto same = [norm](const QUrl &a, const QUrl &b) {
        return a.adjusted(norm) == b.adjusted(norm);
    };
    /*
     * A redirect is one visit, not two.  The engine reports the URL again for
     * each hop, and those arrive within a moment of each other on the same
     * host, so a same-host change that close behind the last entry updates it
     * instead of adding a row.
     */
    const bool redirect = !m_history.isEmpty()
            && m_history.first().url.host() == e.url.host()
            && m_history.first().when.isValid()
            && m_history.first().when.secsTo(QDateTime::currentDateTime()) <= 3;
    if (!m_history.isEmpty() && (same(m_history.first().url, e.url) || redirect)) {
        m_history[0].url = e.url;
        m_history[0].title = e.title;
        m_history[0].when  = QDateTime::currentDateTime();
    } else {
        XxriEntry n = e;
        n.when = QDateTime::currentDateTime();
        m_history.prepend(n);
        while (m_history.size() > kMaxHistory)
            m_history.removeLast();
    }
    save();
    emit historyChanged();
}

void XxriStore::nameVisit(const QUrl &url, const QString &title)
{
    if (title.isEmpty() || m_history.isEmpty())
        return;
    const QUrl::FormattingOptions f = QUrl::StripTrailingSlash
                                    | QUrl::NormalizePathSegments;
    if (m_history.first().url.adjusted(f) != url.adjusted(f))
        return;
    if (m_history.first().title == title)
        return;
    m_history[0].title = title;
    save();
    emit historyChanged();
}

void XxriStore::clearHistory()
{
    if (m_history.isEmpty())
        return;
    m_history.clear();
    save();
    emit historyChanged();
}
