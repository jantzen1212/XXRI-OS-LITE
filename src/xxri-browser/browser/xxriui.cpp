/*
 * xxriui.cpp - XXRI OS Lite user interface for XXRI Browser.
 * See xxriui.h for the layout this reproduces.
 */

#include "xxriui.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QWindow>
#include <QFontMetrics>
#include <QCoreApplication>
#include <QApplication>
#include <QScreen>
#include <QVariant>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

namespace {
// The three control glyphs, in the order the mockup draws them.
enum { kBtnMinimise = 0, kBtnMaximise = 1, kBtnClose = 2, kBtnCount = 3 };
}

// measurements from assets/mockup/mockup1.jpg in artwork pixels, always * s
namespace {
// sidebar (artwork x 176..411 of a window at x 176..1500)
constexpr qreal mSidebar = 236, mWindowW = 1325, mWindowH = 898;
constexpr qreal mPad = 12, mTile = 50, mTileGap = 10;
constexpr qreal mSlotH = 28;    // the artwork's small-control height
constexpr qreal mItemPitch = 52.5, mItemIcon = 20, mItemTextLeft = 54;
constexpr qreal mCtlDot = 13, mCtlGap = 9;
// Vertical, from the window top (artwork y 112). The artwork's empty pills
// and rule under the tiles are gone (see createXxriSidebar), so the label
// heads the bookmark grid and everything below flows after the grid.
constexpr qreal mYControls = 5, mYWordmark = 39;
// the artwork's gaps in the new order: ~20 label to grid, ~30 grid to rule
constexpr qreal mYLabel = 68, mYTiles = 106;
constexpr qreal mGridGap = 30, mRuleGap = 6, mRuleW = 160;
// tool bar, across the content column (artwork x 412..1500): nav glyphs at
// 17/50/80, pill 103..928, tab counter / new tab / menu at 969/1019/1067
constexpr qreal mBarH = 37, mPillH = 26, mNavPitch = 30, mNavIcon = 20;
constexpr qreal mBarLead = 1, mBarPillGap = 7, mBarRightGap = 25;
constexpr qreal mBarBtnGap = 17, mBarEnd = 5;
// type, from the cap heights in the artwork
constexpr qreal mFWordmark = 28, mFItem = 17, mFLabel = 16, mFPill = 15;
// window
constexpr qreal mCorner = 18;

int px(qreal mockup, qreal s, int floorPx = 1)
{
    return qMax(floorPx, qRound(mockup * s));
}
}

XxriMetrics XxriMetrics::forWindow(int windowWidth)
{
    XxriMetrics m;
    m.s = windowWidth / mWindowW;

    m.sidebar      = px(mSidebar, m.s, 132);
    m.pad          = px(mPad, m.s, 6);
    m.tile         = px(mTile, m.s, 26);
    m.tileGap      = px(mTileGap, m.s, 4);
    m.slotH        = px(mSlotH, m.s, 16);
    m.itemH        = px(mItemPitch, m.s, 24);
    m.itemIcon     = px(mItemIcon, m.s, 12);
    m.itemGap      = px(mItemTextLeft - mPad - mItemIcon, m.s, 4);
    m.ctlDot       = px(mCtlDot, m.s, 8);
    m.ctlGap       = px(mCtlGap, m.s, 5);
    // glyph plus hover ring, no more (or the wordmark below moves)
    m.ctlBox       = m.ctlDot + 2 * qMax(2, m.ctlDot / 4);

    m.yControls    = px(mYControls, m.s, 2);
    m.yWordmark    = px(mYWordmark, m.s, 12);
    m.yLabel       = px(mYLabel, m.s, 24);
    m.yTiles       = px(mYTiles, m.s, 36);
    m.gridGap      = px(mGridGap, m.s, 10);
    m.ruleGap      = px(mRuleGap, m.s, 3);
    m.ruleW        = px(mRuleW, m.s, 70);

    // the only real floors: a usable text field, and a bar that fits it
    m.pillH        = px(mPillH, m.s, 20);
    m.barH         = qMax(px(mBarH, m.s, 28), m.pillH + px(11, m.s, 6));
    m.navBox       = px(mNavPitch, m.s, 18);
    m.navIcon      = px(mNavIcon, m.s, 12);
    m.barLead      = px(mBarLead, m.s, 1);
    m.barPillGap   = px(mBarPillGap, m.s, 3);
    m.barRightGap  = px(mBarRightGap, m.s, 8);
    m.barBtnGap    = px(mBarBtnGap, m.s, 5);
    m.barEnd       = px(mBarEnd, m.s, 2);

    m.fWordmark    = px(mFWordmark, m.s, 12);
    m.fItem        = px(mFItem, m.s, 10);
    m.fLabel       = px(mFLabel, m.s, 10);
    m.fClear       = px(mFItem - 3, m.s, 9);
    m.fPill        = px(mFPill, m.s, 10);

    m.corner       = px(mCorner, m.s, 6);
    m.gripEdge     = 5;         // a hit band, not a drawn thing
    m.gripCorner   = 14;
    return m;
}

QSize XxriMetrics::defaultWindow(const QRect &available)
{
    // the artwork's aspect; most of the screen width, unless that runs into
    // the dock, then height decides
    const qreal aspect = mWindowW / mWindowH;          // 1.476
    const int   margin = qRound(available.height() * 0.03);
    const int   dock   = 78;                            // the XXRI dock's strut

    int w = qMin(qRound(available.width() * 0.88), 1180);
    int h = qRound(w / aspect);

    const int maxH = available.height() - dock - margin * 2;
    if (h > maxH) {
        h = maxH;
        w = qRound(h * aspect);
    }
    return QSize(qMax(560, w), qMax(380, h));
}

// ---------------------------------------------------------------------------
// Palette: Light by default, Dark as Settings' Mode says
// ---------------------------------------------------------------------------
const char *XxriUi::kSidebarTop    = "#F2F2F4";
const char *XxriUi::kSidebarMid    = "#EEEEF1";
const char *XxriUi::kSidebarBottom = "#F4F4F6";
const char *XxriUi::kToolbarLeft   = "#F5F5F7";
const char *XxriUi::kToolbarRight  = "#FAFAFB";
const char *XxriUi::kFieldBg       = "#FFFFFF";
const char *XxriUi::kFieldBorder   = "#AEAEAE";
const char *XxriUi::kInk           = "#1C1C1E";
const char *XxriUi::kMuted         = "#6B6B73";
const char *XxriUi::kPage          = "#FFFFFF";

namespace { bool g_dark = false; }

bool XxriUi::dark() { return g_dark; }

bool XxriUi::loadPalette()
{
    QFile f(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/xxri/appearance"));
    bool d = false;
    if (f.open(QIODevice::ReadOnly))
        d = f.readAll().trimmed().startsWith("dark");
    if (d == g_dark)
        return false;
    g_dark = d;
    // Dark: Settings' dark surfaces (window #242124, cards #2A2B33)
    kSidebarTop    = d ? "#1D1B1D" : "#F2F2F4";
    kSidebarMid    = d ? "#1F1D20" : "#EEEEF1";
    kSidebarBottom = d ? "#222024" : "#F4F4F6";
    kToolbarLeft   = d ? "#242124" : "#F5F5F7";
    kToolbarRight  = d ? "#26252A" : "#FAFAFB";
    kFieldBg       = d ? "#34353E" : "#FFFFFF";
    kFieldBorder   = d ? "#4A4B55" : "#AEAEAE";
    kInk           = d ? "#F2F2F4" : "#1C1C1E";
    kMuted         = d ? "#A0A1AA" : "#6B6B73";
    kPage          = d ? "#242124" : "#FFFFFF";
    return true;
}

/* Dark only: the colours of styleSheet()'s rules again, appended to it so
 * the light sheet stays as it is (a later rule of the same selector wins) */
static QString darkOverrides()
{
    return QStringLiteral(
        "QLineEdit#xxriAddress { selection-background-color: #2E5FB0; }"
        "QToolButton#xxriNav:hover, QToolButton#xxriEditBookmarks:hover"
        "  { background: rgba(255,255,255,0.10); }"
        "QToolButton#xxriNav:pressed { background: rgba(255,255,255,0.16); }"
        "QToolButton#xxriSideItem:hover { background: rgba(255,255,255,0.08); }"
        "QToolButton#xxriSideItem:pressed { background: rgba(255,255,255,0.14); }"
        "QWidget#xxriTabRow[current=\"true\"] { background: rgba(255,255,255,0.10); }"
        "QToolButton#xxriTabItem:hover { background: rgba(255,255,255,0.08); }"
        "QToolButton#xxriTabItem[current=\"true\"] { color: #F2F2F4; }"
        "QToolButton#xxriTabItemClose:hover { background: rgba(255,255,255,0.12); }"
        "QToolButton#xxriTile { background: #34353E; border: 1px solid rgba(255,255,255,0.08); }"
        "QToolButton#xxriTile:hover { background: #3C3D47; border: 1px solid rgba(76,147,248,0.70); }"
        "QToolButton#xxriAddTile { background: rgba(255,255,255,0.06);"
        "  border: 1px dashed rgba(255,255,255,0.28); color: #A0A1AA; }"
        "QToolButton#xxriAddTile:hover { background: #34353E; border: 1px dashed #4C93F8; }"
        "QToolButton#xxriClear:hover { color: #4C93F8; }"
        "QToolButton#xxriClear:disabled { color: #5A5B66; }"
        "QToolButton#xxriEditBookmarks[on=\"true\"] { background: rgba(255,255,255,0.14); }"
        "QToolButton#xxriTileRemove { background: #34353E; border: 1px solid rgba(255,255,255,0.18); }"
        "QToolButton#xxriTileRemove:hover { background: #4A2433; border: 1px solid #E0417B; }"
        "QMenu { background: #2A2B33; border: 1px solid rgba(255,255,255,0.10); }"
        "QMenu::item:selected { background: #3C3D47; }"
        "QMenu::item:disabled { color: #6B6C75; }"
        "QMenu::separator { background: rgba(255,255,255,0.10); }"
        "QDialog#xxriDialog { background: #2A2B33; }"
        "QMessageBox#xxriDialog { background: #2A2B33; border: 1px solid rgba(255,255,255,0.10); }"
        "QMessageBox#xxriDialog QPushButton, QDialog#xxriDialog QPushButton { background: #3C3D47; }"
        "QMessageBox#xxriDialog QPushButton:hover, QDialog#xxriDialog QPushButton:hover"
        "  { background: #454650; }"
        "QDialog#xxriDialog QLabel#xxriDialogPageUrl { color: #A0A1AA; }"
        "QDialog#xxriDialog QLineEdit { background: #34353E; }"
        "QDialog#xxriDialog QLineEdit:focus { border: 1px solid #4C93F8; }"
        "QLineEdit#xxriAddress:focus { border: 1px solid #4C93F8; }"
    );
}

QString XxriUi::styleSheet(const XxriMetrics &m)
{
    // generated from the metrics so radii, padding and type scale together
    return QString(
        "QLineEdit#xxriAddress {"
        "  background: %1; border: 1px solid %2; border-radius: %3px;"
        "  padding: 0px %4px; color: %5; font-size: %6px;"
        "  selection-background-color: #B9D4FC;"
        "}"
        "QLineEdit#xxriAddress:focus { border: 1px solid #2E7CF6; }"
        "QToolButton#xxriNav {"
        "  background: transparent; border: none; border-radius: %7px;"
        "}"
        "QToolButton#xxriNav:hover { background: rgba(255,255,255,0.70); }"
        "QToolButton#xxriNav:pressed { background: rgba(255,255,255,0.95); }"
        "QToolButton#xxriSideItem {"
        "  background: transparent; border: none; border-radius: %8px;"
        "  text-align: left; padding: 0px %9px; color: %5;"
        "  font-size: %10px;"
        "}"
        "QToolButton#xxriSideItem:hover { background: rgba(255,255,255,0.60); }"
        "QToolButton#xxriSideItem:pressed { background: rgba(255,255,255,0.90); }"
        // open tabs are side items; the current one is shaded and bold
        "QWidget#xxriTabRow { background: transparent; border-radius: %8px; }"
        "QWidget#xxriTabRow[current=\"true\"] {"
        "  background: rgba(0,0,0,0.07);"
        "}"
        "QToolButton#xxriTabItem {"
        "  background: transparent; border: none; border-radius: %8px;"
        "  text-align: left; padding: 0px 0px 0px %9px; color: %5;"
        "  font-size: %10px;"
        "}"
        "QToolButton#xxriTabItem:hover { background: rgba(255,255,255,0.55); }"
        "QToolButton#xxriTabItem[current=\"true\"] {"
        "  font-weight: bold; color: #1C1C1E;"
        "}"
        "QToolButton#xxriTabItemClose {"
        "  background: transparent; border: none; border-radius: %8px;"
        "}"
        "QToolButton#xxriTabItemClose:hover { background: rgba(0,0,0,0.10); }"
        "QToolButton#xxriTile {"
        "  background: #FFFFFF; border: 1px solid rgba(0,0,0,0.10);"
        "  border-radius: %11px;"
        "}"
        "QToolButton#xxriTile:hover {"
        "  background: #FFFFFF; border: 1px solid rgba(46,124,246,0.60); }"
        // the add tile is dashed so it doesn't look like a broken site
        "QToolButton#xxriAddTile {"
        "  background: rgba(255,255,255,0.55);"
        "  border: 1px dashed rgba(0,0,0,0.28);"
        "  border-radius: %11px; color: #6B6B73;"
        "}"
        "QToolButton#xxriAddTile:hover { background: #FFFFFF;"
        "  border: 1px dashed #2E7CF6; }"
        // "Bookmark" is a section label: regular weight, not a second
        // bold title next to the wordmark
        "QLabel#xxriSectionLabel {"
        "  color: %5; font-size: %12px; font-weight: normal;"
        "  letter-spacing: 0.3px;"
        "}"
        "QToolButton#xxriClear {"
        "  background: transparent; border: none; color: %5;"
        "  font-size: %13px; padding: 0px;"
        "}"
        "QToolButton#xxriClear:hover { color: #2E7CF6; }"
        "QToolButton#xxriClear:disabled { color: #B4B4BA; }"
        // pencil next to "Bookmark", quiet until hovered or on
        "QToolButton#xxriEditBookmarks {"
        "  background: transparent; border: none; border-radius: %14px;"
        "}"
        "QToolButton#xxriEditBookmarks:hover { background: rgba(255,255,255,0.70); }"
        "QToolButton#xxriEditBookmarks[on=\"true\"] {"
        "  background: rgba(0,0,0,0.10);"
        "}"
        // remove badge on each tile in edit mode
        "QToolButton#xxriTileRemove {"
        "  background: #FFFFFF; border: 1px solid rgba(0,0,0,0.18);"
        "  border-radius: %15px;"
        "}"
        "QToolButton#xxriTileRemove:hover {"
        "  background: #FFE9F2; border: 1px solid #E0417B;"
        "}"
        "QTabWidget#xxriTabs::pane { border: none; margin: 0px; padding: 0px; }"
        "QProgressBar#xxriProgress { border: none; background: transparent; }"
        "QProgressBar#xxriProgress::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "      stop:0 %16, stop:1 %17);"
        "}"
        // menus, dialogs, tooltips
        "QMenu {"
        "  background: #FFFFFF; border: 1px solid rgba(0,0,0,0.12);"
        "  border-radius: %18px; padding: 4px; color: %5; font-size: %10px;"
        "}"
        "QMenu::item { padding: %19px 22px; border-radius: %8px; }"
        "QMenu::item:selected { background: #ECECEF; }"
        "QMenu::item:disabled { color: #A8A8B0; }"
        "QMenu::separator { height: 1px; margin: 4px 8px;"
        "  background: rgba(0,0,0,0.10); }"
        "QMenu::icon { padding-left: 8px; }"
        "QToolTip { background: #2A2A2E; color: #FFFFFF; font-size: %13px;"
        "  border: none; padding: 3px 7px; }"
        "QDialog#xxriDialog { background: #FFFFFF; }"
        "QMessageBox#xxriDialog {"
        "  background: #FFFFFF; border: 1px solid rgba(0,0,0,0.12);"
        "  border-radius: %18px; }"
        "QMessageBox#xxriDialog QLabel { color: %5; font-size: %10px; }"
        "QMessageBox#xxriDialog QPushButton {"
        "  border: none; border-radius: %8px;"
        "  padding: %19px 16px; color: %5; font-size: %10px;"
        "  background: #ECECEF; min-width: 74px; }"
        "QMessageBox#xxriDialog QPushButton:hover { background: #E2E2E6; }"
        "QMessageBox#xxriDialog QPushButton#xxriPrimary {"
        "  border: none; color: #FFFFFF;"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "      stop:0 %16, stop:1 %17); }"
        "QDialog#xxriDialog QLabel { color: %5; font-size: %10px; }"
        "QDialog#xxriDialog QLabel#xxriDialogTitle {"
        "  font-size: %12px; font-weight: bold; }"
        "QDialog#xxriDialog QLabel#xxriDialogPageTitle {"
        "  font-size: %10px; font-weight: bold; color: %5; }"
        "QDialog#xxriDialog QLabel#xxriDialogPageUrl {"
        "  font-size: %13px; color: #8E8E96; }"
        "QDialog#xxriDialog QLineEdit {"
        "  background: #FFFFFF; border: 1px solid %2; border-radius: %8px;"
        "  padding: %19px 8px; color: %5; font-size: %10px; }"
        "QDialog#xxriDialog QLineEdit:focus { border: 1px solid #2E7CF6; }"
        "QDialog#xxriDialog QPushButton {"
        "  border: none; border-radius: %8px;"
        "  padding: %19px 14px; color: %5; font-size: %10px;"
        "  background: #ECECEF; }"
        "QDialog#xxriDialog QPushButton:hover { background: #E2E2E6; }"
        "QDialog#xxriDialog QPushButton#xxriPrimary {"
        "  border: none; color: #FFFFFF;"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "      stop:0 %16, stop:1 %17); }"
        "QDialog#xxriDialog QPushButton#xxriPrimary:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "      stop:0 #2459D0, stop:1 #5AA5F8); }"
    )
     .arg(kFieldBg).arg(kFieldBorder)
     .arg(m.pillH / 2).arg(qMax(6, qRound(12 * m.s))).arg(kInk).arg(m.fPill)
     .arg(qMax(4, m.navBox / 4)).arg(qMax(5, qRound(9 * m.s)))
     .arg(qMax(3, qRound(5 * m.s))).arg(m.fItem)
     .arg(qMax(6, qRound(15 * m.s))).arg(m.fLabel).arg(m.fClear)
     .arg(m.slotH / 2).arg(qMax(5, qRound(8 * m.s)))
     .arg(kPrimaryFrom).arg(kPrimaryTo)
     .arg(qMax(6, qRound(10 * m.s))).arg(qMax(3, qRound(6 * m.s)))
     + (g_dark ? darkOverrides() : QString());
}

// ---------------------------------------------------------------------------
// Painted icons
// ---------------------------------------------------------------------------
void XxriUi::setGlyph(QAbstractButton *b, Glyph g, int px, bool muted)
{
    b->setProperty("xxriGlyph", int(g));
    b->setProperty("xxriGlyphPx", px);
    b->setProperty("xxriGlyphMuted", muted);
    const QColor ink(muted ? kMuted : kInk);
    b->setIcon(g == NewTabMark ? newTabIcon(px, ink)
               : g == MenuMark ? menuIcon(px, ink) : glyph(g, px, ink));
}

void XxriUi::restyleGlyphs(QWidget *root)
{
    const auto buttons = root->findChildren<QAbstractButton *>();
    for (QAbstractButton *b : buttons) {
        const QVariant g = b->property("xxriGlyph");
        if (g.isValid())
            setGlyph(b, Glyph(g.toInt()), b->property("xxriGlyphPx").toInt(),
                     b->property("xxriGlyphMuted").toBool());
    }
}
QIcon XxriUi::glyph(Glyph g, int px, const QColor &ink)
{
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal w = qMax(qreal(1.2), px / 11.0);     // stroke weight
    QPen pen(ink, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal m = px * 0.22;                        // inset
    const qreal a = m, b = px - m, c = px / 2.0;

    switch (g) {
    case Plus:
        p.drawLine(QPointF(c, a), QPointF(c, b));
        p.drawLine(QPointF(a, c), QPointF(b, c));
        break;
    case ChevronLeft: {
        const qreal xr = px * 0.62, xl = px * 0.36;
        p.drawPolyline(QPolygonF() << QPointF(xr, a) << QPointF(xl, c)
                                   << QPointF(xr, b));
        break;
    }
    case ChevronRight: {
        const qreal xl = px * 0.38, xr = px * 0.64;
        p.drawPolyline(QPolygonF() << QPointF(xl, a) << QPointF(xr, c)
                                   << QPointF(xl, b));
        break;
    }
    case Clock: {
        // history: open circle, hands, a "rewind" tick top left
        const QRectF r(a, a + px * 0.06, b - a, b - a);
        p.drawArc(r, 70 * 16, -320 * 16);
        p.drawLine(QPointF(c, r.center().y()),
                   QPointF(c, r.center().y() - r.height() * 0.28));
        p.drawLine(QPointF(c, r.center().y()),
                   QPointF(c + r.width() * 0.22, r.center().y()));
        p.drawLine(QPointF(r.left() + r.width() * 0.18, a),
                   QPointF(r.left() + r.width() * 0.40, a + px * 0.10));
        break;
    }
    case Cross: {
        p.drawLine(QPointF(a, a), QPointF(b, b));
        p.drawLine(QPointF(b, a), QPointF(a, b));
        break;
    }
    case Minus: {
        p.drawLine(QPointF(a, c), QPointF(b, c));
        break;
    }
    case Pencil: {
        // nib on a shaft, with a line under it
        const qreal t = px * 0.20, u = px * 0.74;
        QPainterPath shaft;
        shaft.moveTo(t, u);                       // the nib
        shaft.lineTo(t + px * 0.10, u - px * 0.14);
        shaft.lineTo(px * 0.72, px * 0.16);       // up to the top of the shaft
        shaft.lineTo(px * 0.86, px * 0.30);
        shaft.lineTo(t + px * 0.24, u);
        shaft.closeSubpath();
        p.drawPath(shaft);
        p.drawLine(QPointF(t, u + px * 0.10), QPointF(px * 0.62, u + px * 0.10));
        break;
    }
    case ArrowDown: {
        p.drawLine(QPointF(c, a), QPointF(c, b));
        p.drawPolyline(QPolygonF() << QPointF(c - px * 0.20, b - px * 0.20)
                                   << QPointF(c, b)
                                   << QPointF(c + px * 0.20, b - px * 0.20));
        break;
    }
    case TabStack: {
        // the tool bar's tab counter sheets, at row size
        p.setPen(Qt::NoPen);
        XxriUi::paintTabStack(&p, QRectF(0.5, 0.5, px - 1, px - 1), ink,
                              QString());
        break;
    }
    case House: {
        // thinner stroke than the rest, or door and outline merge at bar size
        pen.setWidthF(qMax(qreal(1.0), w * 0.78));
        p.setPen(pen);
        const qreal eave = px * 0.44, base = px * 0.80;
        QPainterPath path;
        path.moveTo(px * 0.22, base);
        path.lineTo(px * 0.22, eave);
        path.quadTo(c, px * 0.16, px * 0.78, eave);
        path.lineTo(px * 0.78, base);
        path.closeSubpath();
        p.drawPath(path);
        QPainterPath door;
        door.moveTo(px * 0.40, base);
        door.lineTo(px * 0.40, px * 0.62);
        door.quadTo(c, px * 0.50, px * 0.60, px * 0.62);
        door.lineTo(px * 0.60, base);
        p.drawPath(door);
        break;
    }
    }
    p.end();
    return QIcon(pm);
}

// ---------------------------------------------------------------------------
// Tool bar sheet icons, drag handle
// ---------------------------------------------------------------------------

void XxriUi::paintTabStack(QPainter *p, const QRectF &box, const QColor &ink,
                           const QString &centre)
{
    const qreal s = qMin(box.width(), box.height());
    const qreal w = qMax(qreal(1.1), s / 13.0);
    p->setPen(QPen(ink, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    const qreal off = s * 0.22;
    // the sheet behind, drawn as the two edges that show
    QPainterPath back;
    back.moveTo(box.left() + off, box.top() + w / 2);
    back.lineTo(box.right() - w / 2, box.top() + w / 2);
    back.lineTo(box.right() - w / 2, box.bottom() - off);
    p->drawPath(back);
    // the sheet in front
    const QRectF front(box.left() + w / 2, box.top() + off,
                       box.width() - off - w, box.height() - off - w);
    p->drawRoundedRect(front, s * 0.14, s * 0.14);
    if (centre == QLatin1String("+")) {
        const QPointF c = front.center();
        const qreal a = front.width() * 0.26;
        p->drawLine(QPointF(c.x() - a, c.y()), QPointF(c.x() + a, c.y()));
        p->drawLine(QPointF(c.x(), c.y() - a), QPointF(c.x(), c.y() + a));
    } else if (!centre.isEmpty()) {
        QFont f = p->font();
        f.setPixelSize(qMax(7, int(front.height() * 0.72)));
        p->setFont(f);
        p->setPen(QPen(ink, w));
        p->drawText(front, Qt::AlignCenter, centre);
    }
}

QIcon XxriUi::newTabIcon(int px, const QColor &ink)
{
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintTabStack(&p, QRectF(0.5, 0.5, px - 1, px - 1), ink, QStringLiteral("+"));
    p.end();
    return QIcon(pm);
}

QIcon XxriUi::menuIcon(int px, const QColor &ink)
{
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    const qreal r = qMax(qreal(0.9), px / 12.0);
    for (int i = 0; i < 3; ++i)
        p.drawEllipse(QPointF(px / 2.0, px * (0.24 + 0.26 * i)), r, r);
    p.end();
    return QIcon(pm);
}

XxriDragArea::XxriDragArea(QWidget *window, QWidget *parent)
    : QWidget(parent), m_window(window) {}

void XxriDragArea::mousePressEvent(QMouseEvent *e)
{
    // not startSystemMove(): XCB "succeeds" with a _NET_WM_MOVERESIZE that
    // flwm doesn't implement, and nothing moves
    if (e->button() != Qt::LeftButton || !m_window) {
        QWidget::mousePressEvent(e);
        return;
    }
    m_dragging = true;
    m_grab = e->globalPos() - m_window->frameGeometry().topLeft();
    e->accept();
}

void XxriDragArea::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && m_window && (e->buttons() & Qt::LeftButton)) {
        m_window->move(e->globalPos() - m_grab);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void XxriDragArea::mouseReleaseEvent(QMouseEvent *e)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(e);
}

void XxriDragArea::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_window) {
        m_dragging = false;
        XxriUi::toggleMaximise(m_window);
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

// ---------------------------------------------------------------------------
// The window material
// ---------------------------------------------------------------------------
bool XxriUi::composited(const QWidget *w)
{
    const QWidget *top = w ? w->window() : nullptr;
    return top && top->testAttribute(Qt::WA_TranslucentBackground);
}

namespace {
const char *const kRestoreProp = "xxriRestoreGeometry";
}

bool XxriUi::isMaximised(const QWidget *window)
{
    return window && window->property(kRestoreProp).toRect().isValid();
}

void XxriUi::toggleMaximise(QWidget *window)
{
    if (!window)
        return;
    if (isMaximised(window)) {
        const QRect r = window->property(kRestoreProp).toRect();
        window->setProperty(kRestoreProp, QVariant());
        window->move(r.topLeft());
        window->resize(r.size());
        return;
    }
    // dock strut, same variable xxri-chrome.h reads
    int reserve = qEnvironmentVariableIntValue("XXRI_DOCK_RESERVE");
    if (reserve <= 0)
        reserve = 78;
    const QRect screen = QApplication::primaryScreen()->geometry();
    window->setProperty(kRestoreProp, window->geometry());
    window->move(screen.topLeft());
    window->resize(screen.width(), screen.height() - reserve);
}

QPainterPath XxriUi::windowClip(const QWidget *w, int radius)
{
    QPainterPath path;
    const QWidget *top = w ? w->window() : nullptr;
    if (!top) {
        path.addRect(w ? QRectF(w->rect()) : QRectF());
        return path;
    }
    path.addRoundedRect(QRectF(0, 0, top->width(), top->height()),
                        radius, radius);
    // into w's coordinates
    const QPoint origin = w->mapFrom(const_cast<QWidget *>(top), QPoint(0, 0));
    path.translate(origin);
    return path;
}

namespace {
// white at 0.83 (214/255), measured off the mockup, see XxriSidebar; Dark:
// Settings' dark sidebar colour at the same alpha
QColor glassFill() { return XxriUi::dark() ? QColor(29, 27, 29, 214) : QColor(255, 255, 255, 214); }

void paintGlass(QPainter *p, QWidget *w, const XxriMetrics &m,
                const QColor &opaqueFrom, const QColor &opaqueTo,
                bool vertical)
{
    const QRect r = w->rect();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setClipPath(XxriUi::windowClip(w, m.corner), Qt::IntersectClip);
    if (XxriUi::composited(w)) {
        p->fillRect(r, glassFill());
        return;
    }
    // no compositor: an opaque look-alike of the glass over the wallpaper
    QLinearGradient grad(0, 0, vertical ? r.width() * 0.16 : r.width(),
                         vertical ? r.height() : 0);
    grad.setColorAt(0.0, opaqueFrom);
    grad.setColorAt(1.0, opaqueTo);
    p->fillRect(r, grad);
}
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------
XxriSidebar::XxriSidebar(QWidget *window, const XxriMetrics &m, QWidget *parent)
    : XxriDragArea(window, parent), m_metrics(m)
{
    setObjectName(QStringLiteral("xxriSidebar"));
    setAttribute(Qt::WA_StyledBackground, false); // we paint ourselves
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

QSize XxriSidebar::sizeHint() const
{
    return QSize(m_metrics.sidebar, 0);
}

void XxriSidebar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintGlass(&p, this, m_metrics, QColor(XxriUi::kSidebarTop),
               QColor(XxriUi::kSidebarBottom), true);
}

// ---------------------------------------------------------------------------
// Tool bar
// ---------------------------------------------------------------------------
XxriToolBar::XxriToolBar(QWidget *window, const XxriMetrics &m, QWidget *parent)
    : XxriDragArea(window, parent), m_metrics(m)
{
    setObjectName(QStringLiteral("xxriToolbar"));
    setAttribute(Qt::WA_StyledBackground, false); // we paint ourselves
    setFixedHeight(m_metrics.barH);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize XxriToolBar::sizeHint() const
{
    return QSize(0, m_metrics.barH);
}

void XxriToolBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintGlass(&p, this, m_metrics, QColor(XxriUi::kToolbarLeft),
               QColor(XxriUi::kToolbarRight), false);
}

// ---------------------------------------------------------------------------
// Tab counter
// ---------------------------------------------------------------------------
XxriTabCounter::XxriTabCounter(QWidget *parent) : QToolButton(parent)
{
    setObjectName(QStringLiteral("xxriNav"));
    setCursor(Qt::PointingHandCursor);
}

void XxriTabCounter::setCount(int n)
{
    if (n == m_count)
        return;
    m_count = n;
    update();
}

QSize XxriTabCounter::sizeHint() const { return QSize(23, 23); }

void XxriTabCounter::paintEvent(QPaintEvent *e)
{
    QToolButton::paintEvent(e);   // style sheet paints the hover background
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor ink(XxriUi::kInk);
    const qreal s  = qMin(width(), height()) * 0.78;      // icon box
    const qreal ox = (width() - s) / 2.0, oy = (height() - s) / 2.0;
    XxriUi::paintTabStack(&p, QRectF(ox, oy, s, s), ink,
                          QString::number(m_count));
}

/* Glyphs at the artwork's size; the widget is bigger so the hover ring fits
 * and the target is comfortable. It sizes itself, or the glyphs get clipped. */
XxriWindowControls::XxriWindowControls(QWidget *window, const XxriMetrics &m,
                                        QWidget *parent)
    : QWidget(parent), m_window(window), m_metrics(m)
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip(QString());
    setFixedHeight(sizeHint().height());
}

QSize XxriWindowControls::sizeHint() const
{
    const int d = m_metrics.ctlDot, g = m_metrics.ctlGap;
    const int ring = qMax(2, d / 4);           // room for the hover ring
    return QSize(kBtnCount * d + (kBtnCount - 1) * g + ring * 2,
                 m_metrics.ctlBox);
}

QRect XxriWindowControls::buttonRect(int i) const
{
    const int d = m_metrics.ctlDot, g = m_metrics.ctlGap;
    const int ring = qMax(2, d / 4);
    return QRect(ring + i * (d + g), (height() - d) / 2, d, d);
}

int XxriWindowControls::hitTest(const QPoint &p) const
{
    for (int i = 0; i < kBtnCount; ++i) {
        // full row height and half a gap either side
        QRect r = buttonRect(i);
        r.setTop(0);
        r.setBottom(height() - 1);
        r.adjust(-m_metrics.ctlGap / 2, 0, m_metrics.ctlGap / 2, 0);
        if (r.contains(p))
            return i;
    }
    return -1;
}

void XxriWindowControls::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    // Minimise: a magenta triangle pointing left.
    const QRect r0 = buttonRect(kBtnMinimise);
    p.setBrush(QColor("#F400DA"));
    QPainterPath tri;
    tri.moveTo(r0.right() + 1, r0.top());
    tri.lineTo(r0.right() + 1, r0.bottom() + 1);
    tri.lineTo(r0.left(), r0.center().y() + 0.5);
    tri.closeSubpath();
    p.drawPath(tri);

    // Maximise: a purple rounded square.
    const QRect r1 = buttonRect(kBtnMaximise);
    p.setBrush(QColor("#8A07EA"));
    p.drawRoundedRect(r1, r1.width() * 0.30, r1.height() * 0.30);

    // Close: a blue circle.
    const QRect r2 = buttonRect(kBtnClose);
    p.setBrush(QColor("#2A0CF7"));
    p.drawEllipse(r2);

    if (m_hover >= 0) {
        const QRect h = buttonRect(m_hover);
        const int ring = qMax(2, m_metrics.ctlDot / 4);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(XxriUi::dark() ? QColor(255, 255, 255, 70) : QColor(0, 0, 0, 60), 1));
        p.drawEllipse(h.adjusted(-ring, -ring, ring, ring));
    }
}

void XxriWindowControls::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_window) {
        QWidget::mousePressEvent(e);
        return;
    }
    switch (hitTest(e->pos())) {
    case kBtnMinimise: m_window->showMinimized(); break;
    case kBtnMaximise: XxriUi::toggleMaximise(m_window); break;
    case kBtnClose:    m_window->close(); break;
    default:
        QWidget::mousePressEvent(e);
        return;
    }
    e->accept();
}

void XxriWindowControls::mouseMoveEvent(QMouseEvent *e)
{
    const int h = hitTest(e->pos());
    if (h != m_hover) {
        m_hover = h;
        static const char *kTips[kBtnCount] = { "Minimise", "Maximise", "Close" };
        setToolTip(h >= 0 ? QCoreApplication::translate("XxriWindowControls",
                                                        kTips[h]) : QString());
        update();
    }
}

void XxriWindowControls::leaveEvent(QEvent *)
{
    if (m_hover != -1) { m_hover = -1; setToolTip(QString()); update(); }
}

// ---------------------------------------------------------------------------
// Resize edges
// ---------------------------------------------------------------------------
namespace {
Qt::CursorShape gripCursor(Qt::Edges e)
{
    if ((e.testFlag(Qt::LeftEdge)  && e.testFlag(Qt::TopEdge)) ||
        (e.testFlag(Qt::RightEdge) && e.testFlag(Qt::BottomEdge)))
        return Qt::SizeFDiagCursor;
    if ((e.testFlag(Qt::RightEdge) && e.testFlag(Qt::TopEdge)) ||
        (e.testFlag(Qt::LeftEdge)  && e.testFlag(Qt::BottomEdge)))
        return Qt::SizeBDiagCursor;
    if (e.testFlag(Qt::LeftEdge) || e.testFlag(Qt::RightEdge))
        return Qt::SizeHorCursor;
    return Qt::SizeVerCursor;
}
}

XxriResizeGrip::XxriResizeGrip(QWidget *window, Qt::Edges edges, QWidget *parent)
    : QWidget(parent), m_window(window), m_edges(edges)
{
    setCursor(gripCursor(edges));
    // hit area only, paints nothing
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void XxriResizeGrip::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_window) {
        QWidget::mousePressEvent(e);
        return;
    }
    // not startSystemResize(), same flwm problem as XxriDragArea
    m_active = true;
    m_start  = e->globalPos();
    m_geom   = m_window->geometry();
    e->accept();
}

void XxriResizeGrip::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_active || !m_window || !(e->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(e);
        return;
    }
    const QPoint d = e->globalPos() - m_start;
    int x = m_geom.x(), y = m_geom.y(), w = m_geom.width(), h = m_geom.height();
    if (m_edges & Qt::LeftEdge)   { x += d.x(); w -= d.x(); }
    if (m_edges & Qt::RightEdge)  {             w += d.x(); }
    if (m_edges & Qt::TopEdge)    { y += d.y(); h -= d.y(); }
    if (m_edges & Qt::BottomEdge) {             h += d.y(); }
    if (w < m_window->minimumWidth() || h < m_window->minimumHeight()) {
        e->accept();
        return;
    }
    // no move unless needed: flwm re-applies its frame offset on every move
    // and the window walks across the screen
    if (x == m_geom.x() && y == m_geom.y())
        m_window->resize(w, h);
    else
        m_window->setGeometry(x, y, w, h);
    e->accept();
}

void XxriResizeGrip::mouseReleaseEvent(QMouseEvent *e)
{
    m_active = false;
    QWidget::mouseReleaseEvent(e);
}

XxriWordmark::XxriWordmark(int pixelSize, QWidget *parent)
    : QWidget(parent), m_px(pixelSize)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QFont XxriWordmark::markFont() const
{
    QFont f = font();
    f.setPixelSize(m_px);
    f.setBold(true);
    return f;
}

QSize XxriWordmark::sizeHint() const
{
    const QFontMetrics fm(markFont());
    // the ink only: cap height, no leading or descent ("XXRI Browser" has no
    // descenders), so the section below sits close. paintEvent puts the
    // baseline at the bottom of this box.
    return QSize(fm.horizontalAdvance(QStringLiteral("XXRI Browser")) + 2,
                 fm.capHeight());
}

void XxriWordmark::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QFont f = markFont();
    p.setFont(f);
    const QFontMetrics fm(f);

    const QString mark = QStringLiteral("XXRI");
    const QString name = QStringLiteral(" Browser");
    const int markW = fm.horizontalAdvance(mark);
    // bottom of the box, not height()-descent() (see sizeHint)
    const int baseline = height();

    QLinearGradient g(0, 0, markW, 0);
    g.setColorAt(0.0, QColor(XxriUi::kAccentFrom));
    g.setColorAt(1.0, QColor(XxriUi::kAccentTo));
    p.setPen(QPen(QBrush(g), 1));
    p.drawText(0, baseline, mark);

    p.setPen(QColor(XxriUi::kInk));
    p.drawText(markW, baseline, name);
}

XxriDottedRule::XxriDottedRule(QWidget *parent) : QWidget(parent) {}

QSize XxriDottedRule::sizeHint() const { return QSize(10, 9); }

void XxriDottedRule::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QPen pen(XxriUi::dark() ? QColor(255, 255, 255, 100) : QColor(0, 0, 0, 110),
             1.0, Qt::DotLine, Qt::RoundCap);
    p.setPen(pen);
    const int y = height() / 2;
    p.drawLine(0, y, width(), y);
}

// ---------------------------------------------------------------------------
// Site icons, see xxriui.h
// ---------------------------------------------------------------------------
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QPixmap>
#include <QBuffer>
#include <QTimer>

XxriIcons::XxriIcons() : m_nam(new QNetworkAccessManager(this))
{
}

XxriIcons *XxriIcons::instance()
{
    static XxriIcons *s = new XxriIcons;
    return s;
}

QString XxriIcons::cachePath(const QString &host) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/favicons");
    QDir().mkpath(dir);
    const QByteArray h = QCryptographicHash::hash(host.toUtf8(),
                                                  QCryptographicHash::Md5).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(h) + QStringLiteral(".png");
}

void XxriIcons::remember(const QUrl &pageUrl, const QIcon &icon)
{
    const QString host = pageUrl.host();
    // "newtab" and "history" are this browser's own pages, not sites.
    if (pageUrl.scheme() == QLatin1String("xxri"))
        return;
    if (host.isEmpty() || icon.isNull() || icon.availableSizes().isEmpty())
        return;
    // keep the largest size, it serves 16px rows and 40px tiles alike
    QSize best;
    for (const QSize &sz : icon.availableSizes())
        if (sz.width() > best.width())
            best = sz;
    const QPixmap pm = icon.pixmap(best);
    if (pm.isNull())
        return;
    m_mem.insert(host, QIcon(pm));
    pm.save(cachePath(host), "PNG");
    emit changed(host);
}

QIcon XxriIcons::iconFor(const QUrl &url)
{
    const QString host = url.host();
    if (host.isEmpty())
        return QIcon();
    if (m_mem.contains(host))
        return m_mem.value(host);

    // www.x and x share an icon
    const QString alt = host.startsWith(QLatin1String("www."))
            ? host.mid(4) : QStringLiteral("www.") + host;
    if (m_mem.contains(alt))
        return m_mem.value(alt);
    if (QFile::exists(cachePath(alt))) {
        QPixmap pm(cachePath(alt));
        if (!pm.isNull()) {
            const QIcon ic(pm);
            m_mem.insert(host, ic);
            return ic;
        }
    }

    const QString path = cachePath(host);
    if (QFile::exists(path)) {
        QPixmap pm(path);
        if (!pm.isNull()) {
            const QIcon ic(pm);
            m_mem.insert(host, ic);
            return ic;
        }
    }

    // never visited: fetch. Three spaced-out attempts (see fetch()), the
    // network is often not up yet when the sidebar is built at login
    if (!m_tried.contains(host)) {
        m_tried.insert(host);
        fetch(host, url.scheme(), 0);
    }
    return QIcon();
}

QNetworkRequest XxriIcons::iconRequest(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) "
                                 "AppleWebKit/537.36 (KHTML, like Gecko) "
                                 "Chrome/110.0.0.0 Safari/537.36"));
    return req;
}

/* <link rel="icon"> from the site's page, for sites without /favicon.ico
 * (xda-developers.com 404s there) */
void XxriIcons::discover(const QString &host, const QString &scheme)
{
    QUrl page;
    page.setScheme(scheme.isEmpty() || scheme == QLatin1String("http")
                   ? QStringLiteral("https") : scheme);
    page.setHost(host);
    page.setPath(QStringLiteral("/"));
    QNetworkReply *r = m_nam->get(iconRequest(page));
    connect(r, &QNetworkReply::finished, this, [this, r, host]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError)
            return;
        // <link> lives in <head>, the start is enough
        const QString head = QString::fromUtf8(r->read(96 * 1024));
        static const QRegularExpression linkRe(
            QStringLiteral("<link\\b[^>]*>"),
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression relRe(
            QStringLiteral("rel\\s*=\\s*[\"']([^\"']*)[\"']"),
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression hrefRe(
            QStringLiteral("href\\s*=\\s*[\"']([^\"']*)[\"']"),
            QRegularExpression::CaseInsensitiveOption);
        QString best;
        int bestScore = -1;
        QRegularExpressionMatchIterator it = linkRe.globalMatch(head);
        while (it.hasNext()) {
            const QString tag = it.next().captured(0);
            const QString rel = relRe.match(tag).captured(1).toLower();
            if (!rel.contains(QLatin1String("icon")))
                continue;
            const QString href = hrefRe.match(tag).captured(1);
            if (href.isEmpty())
                continue;
            // prefer apple-touch and PNG, but take anything
            int score = 1;
            if (rel.contains(QLatin1String("apple"))) score += 1;
            if (href.contains(QLatin1String(".png"), Qt::CaseInsensitive)) score += 2;
            if (score > bestScore) { bestScore = score; best = href; }
        }
        if (best.isEmpty())
            return;
        const QUrl iconUrl = r->url().resolved(QUrl(best));
        if (!iconUrl.isValid())
            return;
        QNetworkReply *ir = m_nam->get(iconRequest(iconUrl));
        const QString path = cachePath(host);
        connect(ir, &QNetworkReply::finished, this, [this, ir, host, path]() {
            ir->deleteLater();
            QPixmap pm;
            if (ir->error() != QNetworkReply::NoError
                    || !pm.loadFromData(ir->readAll()) || pm.isNull())
                return;
            if (pm.width() > 64)
                pm = pm.scaled(64, 64, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
            pm.save(path, "PNG");
            m_mem.insert(host, QIcon(pm));
            emit changed(host);
        });
    });
}

void XxriIcons::fetch(const QString &host, const QString &scheme, int attempt)
{
    if (host.isEmpty() || attempt >= 3)
        return;
    // Attempt 2 stops guessing and asks the site's own document.
    if (attempt == 2) {
        discover(host, scheme);
        return;
    }
    // Attempt 1 asks the other of www/non-www.
    QString ask = host;
    if (attempt == 1) {
        ask = host.startsWith(QLatin1String("www."))
                ? host.mid(4) : QStringLiteral("www.") + host;
    }
    QUrl ico;
    ico.setScheme(scheme.isEmpty() || scheme == QLatin1String("http")
                  ? QStringLiteral("https") : scheme);
    ico.setHost(ask);
    ico.setPath(QStringLiteral("/favicon.ico"));
    QNetworkReply *r = m_nam->get(iconRequest(ico));
    const QString path = cachePath(host);
    connect(r, &QNetworkReply::finished, this,
            [this, r, host, scheme, path, attempt]() {
        r->deleteLater();
        QPixmap pm;
        const bool ok = r->error() == QNetworkReply::NoError
                && pm.loadFromData(r->readAll()) && !pm.isNull();
        if (!ok) {
            // Spaced out, because the usual reason for failing this early is
            // that the interface has no address yet.
            const QString h = host, s = scheme;
            const int next = attempt + 1;
            QTimer::singleShot(4000 * next, this,
                               [this, h, s, next]() { fetch(h, s, next); });
            return;
        }
        if (pm.width() > 64)
            pm = pm.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pm.save(path, "PNG");
        m_mem.insert(host, QIcon(pm));
        emit changed(host);
    });
}

QIcon XxriIcons::iconOrGeneric(const QUrl &url, int px)
{
    const QIcon ic = iconFor(url);
    return ic.isNull() ? genericSite(px) : ic;
}

QIcon XxriIcons::genericSite(int px)
{
    // a globe in the accent gradient, not letters that look like branding
    px = qMax(8, px);
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient g(0, 0, px, px);
    g.setColorAt(0, QColor(XxriUi::kAccentFrom));
    g.setColorAt(1, QColor(XxriUi::kAccentTo));
    const qreal w = qMax(qreal(1.0), px / 12.0);
    const QRectF r(w, w, px - 2 * w, px - 2 * w);
    p.setPen(QPen(QBrush(g), w));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r);
    p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));
    // the two meridians
    QPainterPath m1;
    m1.moveTo(r.center().x(), r.top());
    m1.cubicTo(r.left() + r.width() * 0.18, r.top() + r.height() * 0.30,
               r.left() + r.width() * 0.18, r.top() + r.height() * 0.70,
               r.center().x(), r.bottom());
    p.drawPath(m1);
    QPainterPath m2;
    m2.moveTo(r.center().x(), r.top());
    m2.cubicTo(r.right() - r.width() * 0.18, r.top() + r.height() * 0.30,
               r.right() - r.width() * 0.18, r.top() + r.height() * 0.70,
               r.center().x(), r.bottom());
    p.drawPath(m2);
    p.end();
    return QIcon(pm);
}
