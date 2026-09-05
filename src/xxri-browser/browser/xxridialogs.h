/*
 * xxridialogs.h - the browser's own small dialogs, in the XXRI style.
 */

#ifndef XXRIDIALOGS_H
#define XXRIDIALOGS_H

#include <QDialog>
#include <QIcon>
#include <QString>
#include <QUrl>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

/*
 * Add or edit a bookmark.  Frameless and styled like the rest of the browser,
 * with the same window controls, so it does not arrive as a stock Qt dialog in
 * the middle of an XXRI application.
 */
class XxriBookmarkDialog : public QDialog
{
    Q_OBJECT
public:
    /*
     * `icon` is the page's own favicon.  The dialog shows the thing being
     * saved - its mark, its title and its address - rather than only naming
     * it, so what lands in the grid is what was on screen.
     */
    XxriBookmarkDialog(const QString &title, const QString &name,
                       const QUrl &url, const QIcon &icon = QIcon(),
                       QWidget *parent = nullptr);

    QString bookmarkName() const;
    QUrl    bookmarkUrl() const;

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    QLineEdit *m_name;
    QLineEdit *m_url;
    bool       m_rounded = false;
    bool       m_dragging = false;
    QPoint     m_grab;
};

/*
 * The tab list.
 *
 * The artwork's tool bar has no tab strip - the open-tab count is a control on
 * the right of the bar - so this is where tabs live.  It is a real tab surface
 * rather than a list of names: every row carries the page's own favicon, its
 * title, its host, whether it is the current tab, and its own close button.
 */
class TabWidget;

class XxriTabPopover : public QWidget
{
    Q_OBJECT
public:
    XxriTabPopover(TabWidget *tabs, const class XxriMetrics &m,
                   QWidget *parent = nullptr);

    /* Shows the popover under a tool-bar button. */
    void popupUnder(QWidget *anchor);

signals:
    void newTabRequested();
    void reopenRequested();
    void closeRequested(int index);
    void switchRequested(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    void rebuild();
    TabWidget  *m_tabs;
    QWidget    *m_rows;
    int         m_width;
    int         m_rowH;
    int         m_icon;
};

#endif // XXRIDIALOGS_H
