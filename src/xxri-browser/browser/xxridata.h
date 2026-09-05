/*
 * xxridata.h - persistent state for XXRI Browser.
 *
 * Shortcut tiles, bookmarks and history are what make the sidebar in
 * assets/mockup/mockup1.jpg a real control surface rather than a picture, so
 * they live in one process-wide store that every window observes.  Storage is
 * a plain INI file under the user's config directory.
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

    /*
     * Bookmarks - one list, shown as the sidebar's tile grid.
     *
     * There used to be two: a "shortcuts" row of tiles and a "Bookmark" list
     * of slots, which were the same thing to anyone looking at them and left
     * the sidebar holding three permanently empty pills.  A saved page is a
     * bookmark; it has one home, the grid grows to fit however many there are,
     * and nothing reserves space for one that does not exist.
     */
    QVector<XxriEntry> bookmarks() const { return m_bookmarks; }
    bool isBookmarked(const QUrl &url) const;
    void addBookmark(const XxriEntry &e);
    void replaceBookmark(int index, const XxriEntry &e);
    void removeBookmark(int index);
    void toggleBookmark(const XxriEntry &e);
    void clearBookmarks();

    /*
     * The search engine.
     *
     * Google is the default, because that is what the XXRI Browser design
     * expects and what the artwork's own shortcut rail lists.  It is a setting
     * rather than a constant because on some networks Google answers this
     * engine's requests with its "unusual traffic" challenge rather than with
     * results - a fingerprint judgement about Chromium 87 that nothing in this
     * browser can change - and a browser whose address bar cannot complete a
     * search on such a network is not usable.  The menu offers the choice; the
     * default is unchanged.
     */
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
