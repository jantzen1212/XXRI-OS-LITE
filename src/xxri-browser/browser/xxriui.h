/*
 * xxriui.h - XXRI OS Lite user interface for XXRI Browser.
 *
 * XXRI Browser is a fork of Qt's WebEngine "Simple Browser" example.  The
 * engine integration (WebView/WebPage/TabWidget) is upstream's; everything in
 * this file is the XXRI shell that replaces the stock QMainWindow chrome -
 * native menu bar, tool bar and title bar - with the layout in
 * assets/mockup/mockup1.jpg:
 *
 *   +--------+-------------------------------------------+
 *   | sidebar|  < > ^   (        address pill        )   |
 *   |        +-------------------------------------------+
 *   | XXRI   |                                           |
 *   | Browser|             web content                   |
 *   | [][][] |                                           |
 *   | Bookmk |                                           |
 *   | ...    |                                           |
 *   +--------+-------------------------------------------+
 *
 * Geometry is measured from the mockup, not estimated: the window is 1323px
 * wide with a 232px sidebar (17.5%) and a ~37px tool bar.
 */

#ifndef XXRIUI_H
#define XXRIUI_H

#include <QWidget>
#include <QPainterPath>
#include <QToolButton>
#include <QLineEdit>
#include <QIcon>
#include <QNetworkRequest>
#include <QHash>
#include <QSet>
#include <QUrl>
#include <QPoint>
#include <QSize>
#include <QRect>

QT_BEGIN_NAMESPACE
class QLabel;
class QVBoxLayout;
QT_END_NAMESPACE

class BrowserWindow;

/*
 * The visual coordinate system.
 *
 * There is exactly one scale in this application.  The mockup's browser window
 * is 1325x898 device pixels, drawn on a 4:3 desktop 2210px wide - so the
 * artwork is 1:1 for a machine with a 2210px screen, which XXRI's 1024x768 is
 * not.  Rather than scaling individual widgets by eye, the shell computes
 *
 *     s = window width / 1325
 *
 * once, from a default window sized to the actual screen at the mockup's own
 * 1.476 aspect ratio, and every measurement in the artwork is multiplied by it.
 * Nothing in the UI carries a hand-picked pixel size, so the whole window is
 * one consistent piece of design at whatever size it ends up.
 *
 * Floors exist only where a control would otherwise stop working - a text
 * field below 17px cannot be read - and they are listed here rather than
 * buried at the call site.
 */
struct XxriMetrics
{
    qreal s = 1.0;

    // sidebar
    int sidebar = 0, pad = 0, tile = 0, tileGap = 0;
    int slotH = 0;              // the small-control radius the style uses
    int itemH = 0, itemIcon = 0, itemGap = 0;
    int gridGap = 0;            // bookmark grid down to the dotted rule
    int ruleGap = 0;            // dotted rule down to the rows
    int ruleW = 0;              // the rule's own share of the row
    int ctlDot = 0, ctlGap = 0, ctlBox = 0;

    // vertical rhythm: distance from the window's top edge to each element
    int yControls = 0, yWordmark = 0, yLabel = 0, yTiles = 0;

    // tool bar
    int barH = 0, pillH = 0, navBox = 0, navIcon = 0;
    int barLead = 0, barPillGap = 0, barRightGap = 0, barBtnGap = 0, barEnd = 0;

    // type
    int fWordmark = 0, fItem = 0, fLabel = 0, fClear = 0, fPill = 0;

    // window
    int corner = 0, gripEdge = 0, gripCorner = 0;

    /* Derives every number above from one window width. */
    static XxriMetrics forWindow(int windowWidth);
    /* The default window for a screen: the mockup's aspect, sized to fit. */
    static QSize defaultWindow(const QRect &available);
};

// Palette, sampled from the mockup.
namespace XxriUi {
    const char *const kSidebarTop    = "#FFDCF5";
    const char *const kSidebarMid    = "#F0E7FD";
    const char *const kSidebarBottom = "#FFE6FE";
    const char *const kToolbarLeft   = "#FFE2F6";
    const char *const kToolbarRight  = "#F4F7FE";
    const char *const kFieldBg       = "#FFFFFF";
    const char *const kFieldBorder   = "#AEAEAE";
    const char *const kInk           = "#1C1B24";
    const char *const kMuted         = "#6B6880";
    const char *const kAccentFrom    = "#EF00FF";
    const char *const kAccentTo      = "#4400FF";

/*
 * The window's look.  The sidebar and toolbar now paint themselves in paintEvent
 * with proper alpha handling, so the stylesheet no longer needs the composited flag.
 */
QString styleSheet(const XxriMetrics &m);
}

/*
 * The XXRI window controls.  These windows are undecorated and draw their own
 * controls, exactly as Settings, Store and XXRI File do; the glyphs match the
 * shared GTK xxri-chrome.h so all four applications look like one system.
 */
class XxriWindowControls : public QWidget
{
    Q_OBJECT
public:
    XxriWindowControls(QWidget *window, const XxriMetrics &m,
                       QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    int   hitTest(const QPoint &p) const;
    QRect buttonRect(int i) const;
    QWidget    *m_window;
    XxriMetrics m_m;
    int         m_hover = -1;
};

/*
 * A drag handle: pressing anywhere on it moves the whole top-level window
 * through the window manager, so the entire UI travels together.
 */
class XxriDragArea : public QWidget
{
    Q_OBJECT
public:
    explicit XxriDragArea(QWidget *window, QWidget *parent = nullptr);
protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
private:
    QWidget *m_window;
    bool     m_dragging = false;
    QPoint   m_grab;
};

/*
 * The sidebar and the tool bar are one material.
 *
 * Reconstructing the source colour from five points of assets/mockup/mockup1.jpg
 * - inverting  result = a*C + (1-a)*wallpaper  at each - gives C = white and
 * a = 0.83 every time, for the sidebar and the tool bar alike.  The artwork's
 * apparent pink-to-violet-to-pink progression down the sidebar is not a
 * gradient in the sidebar at all: it is the wallpaper showing through a flat
 * white wash.  So that is what these paint, and the compositor supplies the
 * rest.  Without a compositor there is nothing behind to show through, and the
 * fallback is an opaque approximation of what the artwork ends up looking like.
 *
 * The flag is read at paint time, never cached: these widgets are built before
 * the window has decided whether it can be translucent, and a value latched in
 * the constructor is always the wrong one.
 */
class XxriSidebar : public XxriDragArea
{
    Q_OBJECT
public:
    XxriSidebar(QWidget *window, const XxriMetrics &m, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    XxriMetrics m_m;
};

/* The top tool bar: the same material as the sidebar. */
class XxriToolBar : public XxriDragArea
{
    Q_OBJECT
public:
    XxriToolBar(QWidget *window, const XxriMetrics &m, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    XxriMetrics m_m;
};

/* The "XXRI Browser" wordmark: "XXRI" in the accent gradient, "Browser" ink. */
class XxriWordmark : public QWidget
{
    Q_OBJECT
public:
    XxriWordmark(int pixelSize, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QFont markFont() const;
    int m_px;
};

/*
 * An invisible resize edge.
 *
 * A frameless window gets no resize border from the window manager, and the
 * window's own mouse events never reach the edges because the content fills
 * them.  XXRI Settings, Store and File solve this with a GTK overlay of thin
 * transparent edge widgets (xxri-chrome.h, 5px edges and 14px corners); this
 * is the same thing for Qt, so all four applications resize identically.
 */
class XxriResizeGrip : public QWidget
{
    Q_OBJECT
public:
    XxriResizeGrip(QWidget *window, Qt::Edges edges, QWidget *parent = nullptr);
protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    QWidget  *m_window;
    Qt::Edges m_edges;
    bool      m_active = false;
    QPoint    m_start;
    QRect     m_geom;
};

/* The dotted rule the mockup draws above "Clear". */
class XxriDottedRule : public QWidget
{
    Q_OBJECT
public:
    explicit XxriDottedRule(QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
};


/*
 * The mockup's line icons.  These are drawn rather than shipped as files:
 * they are single-stroke geometry (a plus, a clock, two chevrons, a house, a
 * stack of tabs) whose weight has to track the text colour and the widget
 * size, and painting them keeps that relationship exact at any scale.
 */
namespace XxriUi {
    enum Glyph { Plus, Clock, ChevronLeft, ChevronRight, House, ArrowDown, Cross,
                 TabStack, Pencil, Minus };
    QIcon glyph(Glyph g, int px, const QColor &ink);

    /* True when this window really has an alpha channel to paint into. */
    bool composited(const QWidget *w);

    /*
     * Maximise / restore, done by the window itself.
     *
     * flwm is ICCCM-only: it never reads _NET_WM_STATE, so showMaximized()
     * sets a property nothing acts on and the window simply does not move.
     * XXRI's GTK applications each maximise themselves and stop above the
     * dock (xxri-chrome.h); this is the same thing for Qt, so all four behave
     * alike.  The pre-maximise geometry is kept on the window itself.
     */
    void toggleMaximise(QWidget *window);
    bool isMaximised(const QWidget *window);

    /*
     * The window's rounded outline, in the coordinates of `w`.  A frameless
     * window has to draw its own corners; every surface that touches an outer
     * corner clips to this so the radius is the same one everywhere.
     */
    QPainterPath windowClip(const QWidget *w, int radius);

    /* The two stacked-sheet icons at the right of the mockup's tool bar. */
    QIcon newTabIcon(int px, const QColor &ink);
    QIcon menuIcon(int px, const QColor &ink);

    /* Paints the tab-stack outline used by both of them. */
    void  paintTabStack(class QPainter *p, const QRectF &box, const QColor &ink,
                        const QString &centre);

}

/*
 * The tab counter the mockup puts at the right of the tool bar: two stacked
 * rounded squares with the open-tab count inside the front one.
 */
class XxriTabCounter : public QToolButton
{
    Q_OBJECT
public:
    explicit XxriTabCounter(QWidget *parent = nullptr);
    void setCount(int n);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int m_count = 1;
};

/*
 * Site icons.
 *
 * Three sources, in order, and no fourth:
 *
 *   1. the icon the engine itself decoded for the page (QWebEngineView gives
 *      it to us through iconChanged) - this is the site's real favicon, and it
 *      arrives for free on every page that is visited;
 *   2. the on-disk cache, so a site keeps its icon in bookmarks, history and
 *      the shortcut rail long after the tab is closed, and across restarts;
 *   3. one request to the site's /favicon.ico, for a shortcut that has never
 *      been opened - a browser has nowhere else to get it from.
 *
 * If all three fail the caller gets the XXRI generic site mark, which is a
 * drawn icon, not a letter.  Nothing in this browser ever shows an invented
 * abbreviation in place of a site's own branding.
 */
class XxriIcons : public QObject
{
    Q_OBJECT
public:
    static XxriIcons *instance();

    /* Record what the engine decoded for a page. */
    void remember(const QUrl &pageUrl, const QIcon &icon);

    /* Best known icon, or a null icon; starts a fetch if nothing is known. */
    QIcon iconFor(const QUrl &url);

    /* iconFor(), falling back to the XXRI generic site mark. */
    QIcon iconOrGeneric(const QUrl &url, int px);

    /* The fallback: a drawn globe in the XXRI accent, at any size. */
    static QIcon genericSite(int px);

signals:
    /* A site's icon became available; anything showing it should refresh. */
    void changed(const QString &host);

private:
    XxriIcons();
    QString cachePath(const QString &host) const;
    void    fetch(const QString &host, const QString &scheme, int attempt);
    void    discover(const QString &host, const QString &scheme);
    QNetworkRequest iconRequest(const QUrl &url) const;

    class QNetworkAccessManager *m_nam;
    QHash<QString, QIcon> m_mem;
    QSet<QString>         m_tried;
};

#endif // XXRIUI_H
