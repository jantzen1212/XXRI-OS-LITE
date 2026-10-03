/*
 * xxridata.h - persistent state for XXRI Browser: bookmarks, history and the
 * search engine, in one process-wide store every window observes. Stored as
 * an INI file in the user's config directory.
 */

#ifndef XXRIDATA_H
#define XXRIDATA_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QDateTime>

struct XxriEntry
{
    QString   title;
    QUrl      url;
    QDateTime when;
};

class XxriStore : public QObject
{
    Q_OBJECT
public:
    static XxriStore *instance();

    // bookmarks: one list, the sidebar's tile grid (old profiles' separate
    // "shortcuts" are merged in on load)
    QVector<XxriEntry> bookmarks() const { return m_bookmarks; }
    bool isBookmarked(const QUrl &url) const;
    void addBookmark(const XxriEntry &e);
    void replaceBookmark(int index, const XxriEntry &e);
    void removeBookmark(int index);
    void toggleBookmark(const XxriEntry &e);
    void clearBookmarks();

    // Search engine, Google by default. A setting because on some networks
    // Google answers Chromium 87 with its "unusual traffic" page instead.
    QString searchUrl() const { return m_searchUrl; }
    void    setSearchUrl(const QString &url);

    /* Browsing history, newest first. */
    QVector<XxriEntry> history() const { return m_history; }
    void noteVisit(const XxriEntry &e);
    /* Renames the newest entry for a URL; never creates one. */
    void nameVisit(const QUrl &url, const QString &title);
    void clearHistory();

signals:
    void searchEngineChanged();
    void bookmarksChanged();
    void historyChanged();

private:
    XxriStore();
    QString filePath() const;
    void load();
    void save();

    QString            m_searchUrl;
    QVector<XxriEntry> m_bookmarks;
    QVector<XxriEntry> m_history;
};

#endif // XXRIDATA_H
