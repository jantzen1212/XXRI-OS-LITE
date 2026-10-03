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

/* add or edit a bookmark; frameless, with the browser's own window controls */
class XxriBookmarkDialog : public QDialog
{
    Q_OBJECT
public:
    // icon = the page's favicon, shown with title and address
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

/* The tab list: the mockup's tool bar has only a tab count, so tabs live
 * here. Each row: favicon, title, host, current marker, close button. */
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
