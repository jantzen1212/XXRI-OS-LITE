/*
 * xxriui.h - the XXRI shell for XXRI Browser.
 *
 * XXRI Browser is a fork of Qt's WebEngine "Simple Browser" example. The
 * engine side (WebView/WebPage/TabWidget) is upstream's; this replaces the
 * QMainWindow menu bar, tool bar and title bar with assets/mockup/mockup1.jpg:
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
 * One scale for the whole UI. The mockup draws the window 1325x898 (on a
 * 2210px wide desktop), so every artwork measurement is multiplied by
 *
 *     s = window width / 1325
 *
 * computed once for a default window at the mockup's 1.476 aspect. Minimum
 * sizes exist only where a control would stop working (a field under 17px).
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

// Palette: neutral surfaces, the system blue for actions. The accent gradient
// is for artwork only (wordmark, fallback site icon). The surfaces and the
// ink follow Settings' Mode, Light or Dark (loadPalette()); the rest is the
// same in both.
namespace XxriUi {
    extern const char *kSidebarTop;
    extern const char *kSidebarMid;
    extern const char *kSidebarBottom;
    extern const char *kToolbarLeft;
    extern const char *kToolbarRight;
    extern const char *kFieldBg;
    extern const char *kFieldBorder;
    extern const char *kInk;
    extern const char *kMuted;
    extern const char *kPage;                     // behind web content
    const char *const kAccentFrom    = "#EF00FF";
    const char *const kAccentTo      = "#4400FF";
    const char *const kPrimaryFrom   = "#1E4FC2";   // primary buttons, progress
    const char *const kPrimaryTo     = "#4A9AF5";
    const char *const kAction        = "#2E7CF6";   // focus, links, selection

/* ~/.config/xxri/appearance, written by xxri-desktop: sets the palette above.
 * True when the mode changed since the last call. */
bool loadPalette();
bool dark();
/* the window's stylesheet (sidebar and tool bar paint themselves) */
QString styleSheet(const XxriMetrics &m);
}

/* window controls, same glyphs as the GTK apps' xxri-chrome.h */
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
    XxriMetrics m_metrics;
    int         m_hover = -1;
};

/* drag handle for the frameless window; double-click maximises */
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
 * Sidebar and tool bar are flat white at 0.83 alpha (solved from five points
 * of the mockup; the "gradient" there is just the wallpaper behind). Without
 * a compositor they paint an opaque approximation. composited() is checked at
 * paint time: these are built before the window knows if it's translucent.
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
    XxriMetrics m_metrics;
};

class XxriToolBar : public XxriDragArea
{
    Q_OBJECT
public:
    XxriToolBar(QWidget *window, const XxriMetrics &m, QWidget *parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    XxriMetrics m_metrics;
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

/* invisible resize edge for the frameless window, like xxri-chrome.h's
 * (5px edges, 14px corners) */
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


/* the mockup's line icons, painted so stroke and colour follow size and ink */
namespace XxriUi {
    enum Glyph { Plus, Clock, ChevronLeft, ChevronRight, House, ArrowDown, Cross,
                 TabStack, Pencil, Minus,
                 NewTabMark, MenuMark };   // setGlyph only: newTabIcon, menuIcon
    QIcon glyph(Glyph g, int px, const QColor &ink);
    /* b's icon in the palette's ink (or muted) colour, remembered so
     * restyleGlyphs() paints it again after a Light/Dark switch */
    void setGlyph(QAbstractButton *b, Glyph g, int px, bool muted = false);
    void restyleGlyphs(QWidget *root);

    /* True when this window really has an alpha channel to paint into. */
    bool composited(const QWidget *w);

    /* flwm ignores _NET_WM_STATE, so showMaximized() does nothing: maximise
     * by hand and stop above the dock, like xxri-chrome.h */
    void toggleMaximise(QWidget *window);
    bool isMaximised(const QWidget *window);

    /* the window's rounded outline in w's coordinates; everything touching
     * an outer corner clips to it */
    QPainterPath windowClip(const QWidget *w, int radius);

    /* The two stacked-sheet icons at the right of the mockup's tool bar. */
    QIcon newTabIcon(int px, const QColor &ink);
    QIcon menuIcon(int px, const QColor &ink);

    /* Paints the tab-stack outline used by both of them. */
    void  paintTabStack(class QPainter *p, const QRectF &box, const QColor &ink,
                        const QString &centre);

}

/* tab counter: two stacked sheets with the tab count in front */
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
 * Site icons, in this order:
 *   1. what the engine decoded for a visited page (iconChanged)
 *   2. the on-disk cache, so bookmarks/history keep icons across restarts
 *   3. fetched: /favicon.ico (both www and bare host), then <link rel=icon>
 * Otherwise the generic drawn globe - never invented letters.
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
