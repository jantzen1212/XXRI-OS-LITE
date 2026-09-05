/*
 * xxridialogs.cpp - see xxridialogs.h.
 */

#include "xxridialogs.h"
#include "xxriui.h"

#include <QLabel>
#include <QFontMetrics>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>

XxriBookmarkDialog::XxriBookmarkDialog(const QString &title, const QString &name,
                                       const QUrl &url, const QIcon &icon,
                                       QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("xxriDialog"));
    setWindowTitle(title);
    setWindowFlag(Qt::FramelessWindowHint, true);
    // Rounded corners need an alpha channel behind the window.  XXRI's
    // compositor provides one; without it a translucent window is drawn over
    // black, so the dialog stays square-cornered instead.
    m_rounded = QGuiApplication::primaryScreen()
            && QGuiApplication::primaryScreen()->depth() >= 24
            && parent && parent->window()->testAttribute(Qt::WA_TranslucentBackground);
    if (m_rounded)
        setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(18, 14, 18, 14);
    v->setSpacing(8);

    QLabel *heading = new QLabel(title, this);
    heading->setObjectName(QStringLiteral("xxriDialogTitle"));
    v->addWidget(heading);
    v->addSpacing(4);

    // The page itself: its mark beside its title, above the two fields.
    if (!icon.isNull() || !url.isEmpty()) {
        QHBoxLayout *page = new QHBoxLayout;
        page->setSpacing(9);
        QLabel *mark = new QLabel(this);
        mark->setFixedSize(28, 28);
        mark->setAlignment(Qt::AlignCenter);
        if (!icon.isNull())
            mark->setPixmap(icon.pixmap(24, 24));
        page->addWidget(mark, 0, Qt::AlignVCenter);
        QVBoxLayout *text = new QVBoxLayout;
        text->setSpacing(1);
        QLabel *t = new QLabel(name.isEmpty() ? url.host() : name, this);
        t->setObjectName(QStringLiteral("xxriDialogPageTitle"));
        QLabel *u = new QLabel(url.toString(), this);
        u->setObjectName(QStringLiteral("xxriDialogPageUrl"));
        for (QLabel *l : { t, u }) {
            l->setTextFormat(Qt::PlainText);
            l->setMaximumWidth(300);
            l->setWordWrap(false);
        }
        // Elided rather than allowed to widen the dialog.
        const QFontMetrics fmT(t->font()), fmU(u->font());
        t->setText(fmT.elidedText(t->text(), Qt::ElideRight, 290));
        u->setText(fmU.elidedText(u->text(), Qt::ElideRight, 290));
        text->addWidget(t);
        text->addWidget(u);
        page->addLayout(text, 1);
        v->addLayout(page);
        v->addSpacing(8);
    }

    v->addWidget(new QLabel(tr("Name"), this));
    m_name = new QLineEdit(name, this);
    m_name->setPlaceholderText(tr("Page name"));
    v->addWidget(m_name);

    v->addWidget(new QLabel(tr("URL"), this));
    m_url = new QLineEdit(url.toString(), this);
    m_url->setPlaceholderText(QStringLiteral("https://"));
    v->addWidget(m_url);

    v->addSpacing(6);
    QHBoxLayout *row = new QHBoxLayout;
    row->setSpacing(8);
    row->addStretch(1);
    QPushButton *cancel = new QPushButton(tr("Cancel"), this);
    QPushButton *add    = new QPushButton(tr("Add"), this);
    add->setObjectName(QStringLiteral("xxriPrimary"));
    add->setDefault(true);
    row->addWidget(cancel);
    row->addWidget(add);
    v->addLayout(row);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, &QDialog::accept);
    // Enter in either field is the same as pressing Add.
    connect(m_name, &QLineEdit::returnPressed, this, &QDialog::accept);
    connect(m_url, &QLineEdit::returnPressed, this, &QDialog::accept);

    setMinimumWidth(320);
    m_name->setFocus();
    m_name->selectAll();

    // Centred on the window it belongs to, not wherever the WM would put it.
    if (parent) {
        const QRect pg = parent->window()->geometry();
        adjustSize();
        move(pg.center().x() - width() / 2, pg.center().y() - height() / 2);
    }
}

QString XxriBookmarkDialog::bookmarkName() const { return m_name->text().trimmed(); }

QUrl XxriBookmarkDialog::bookmarkUrl() const
{
    return QUrl::fromUserInput(m_url->text().trimmed());
}

void XxriBookmarkDialog::paintEvent(QPaintEvent *)
{
    // The dialog is translucent so its corners can be rounded; the panel is
    // painted here rather than by the style sheet, which cannot round a
    // top-level window's own edges.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal r = m_rounded ? 12.0 : 0.0;
    QPainterPath path;
    path.addRoundedRect(QRectF(0.5, 0.5, width() - 1, height() - 1), r, r);
    p.fillPath(path, QColor("#FBF5FE"));
    p.setPen(QPen(QColor(120, 90, 130, 90), 1));
    p.drawPath(path);
}

void XxriBookmarkDialog::mousePressEvent(QMouseEvent *e)
{
    // Frameless, so the dialog carries its own drag handle.
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_grab = e->globalPos() - frameGeometry().topLeft();
        e->accept();
        return;
    }
    QDialog::mousePressEvent(e);
}

void XxriBookmarkDialog::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPos() - m_grab);
        e->accept();
        return;
    }
    QDialog::mouseMoveEvent(e);
}

void XxriBookmarkDialog::mouseReleaseEvent(QMouseEvent *e)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(e);
}

// ---------------------------------------------------------------------------
// The tab list
// ---------------------------------------------------------------------------

#include "tabwidget.h"
#include "webview.h"
#include "xxriui.h"

#include <QToolButton>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QScreen>

XxriTabPopover::XxriTabPopover(TabWidget *tabs, const XxriMetrics &m,
                               QWidget *parent)
    : QWidget(parent, Qt::Popup), m_tabs(tabs)
{
    setObjectName(QStringLiteral("xxriTabPopover"));
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_width = qMax(240, qRound(330 * m.s));
    m_rowH  = qMax(24, qRound(38 * m.s));
    m_icon  = qMax(12, qRound(20 * m.s));

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(2);
    m_rows = new QWidget(this);
    QVBoxLayout *rl = new QVBoxLayout(m_rows);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(2);
    v->addWidget(m_rows);

    setStyleSheet(QStringLiteral(
        "QWidget#xxriTabPopover { background: transparent; }"
        "QToolButton#xxriTabRow { background: transparent; border: none;"
        "  border-radius: %1px; text-align: left; padding: 0px 6px;"
        "  color: #1C1B24; font-size: %2px; }"
        "QToolButton#xxriTabRow:hover { background: rgba(180,140,255,0.20); }"
        "QToolButton#xxriTabRow[current=\"true\"] {"
        "  background: rgba(180,140,255,0.30); font-weight: bold; }"
        "QToolButton#xxriTabClose { background: transparent; border: none;"
        "  border-radius: %3px; }"
        "QToolButton#xxriTabClose:hover { background: rgba(0,0,0,0.10); }"
        "QToolButton#xxriTabAction { background: transparent; border: none;"
        "  border-radius: %1px; text-align: left; padding: 0px 6px;"
        "  color: #7A1BE0; font-size: %2px; }"
        "QToolButton#xxriTabAction:hover { background: rgba(180,140,255,0.20); }"
    ).arg(qMax(5, qRound(9 * m.s))).arg(qMax(10, qRound(15 * m.s)))
     .arg(qMax(4, m_icon / 2)));

    rebuild();
}

void XxriTabPopover::rebuild()
{
    QVBoxLayout *rl = qobject_cast<QVBoxLayout *>(m_rows->layout());
    if (!rl)
        return;
    while (QLayoutItem *it = rl->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            // Detached before it is scheduled: deleteLater() alone leaves the
            // widget parented and visible until the event loop runs.
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete it;
    }

    const int current = m_tabs->currentIndex();
    const QFontMetrics fm(font());

    for (int i = 0; i < m_tabs->count(); ++i) {
        WebView *view = m_tabs->viewAt(i);
        const QUrl u = view ? view->url() : QUrl();
        QString title = m_tabs->tabText(i).trimmed();
        if (title.isEmpty())
            title = u.host().isEmpty() ? tr("New Tab") : u.host();
        QIcon ic = view ? view->icon() : QIcon();
        if (ic.isNull())
            ic = XxriIcons::instance()->iconOrGeneric(u, m_icon);

        QWidget *row = new QWidget(m_rows);
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);

        QToolButton *b = new QToolButton(row);
        b->setObjectName(QStringLiteral("xxriTabRow"));
        b->setIcon(ic);
        b->setIconSize(QSize(m_icon, m_icon));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFixedHeight(m_rowH);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("current", i == current);
        // A very long title is elided into the row, never allowed to widen it.
        b->setText(fm.elidedText(title, Qt::ElideRight,
                                 m_width - m_icon - m_rowH - 40));
        b->setToolTip(u.isEmpty() ? title
                                  : title + QLatin1Char('\n') + u.toString());
        connect(b, &QToolButton::clicked, this, [this, i]() {
            emit switchRequested(i);
            close();
        });
        h->addWidget(b, 1);

        QToolButton *x = new QToolButton(row);
        x->setObjectName(QStringLiteral("xxriTabClose"));
        x->setFixedSize(m_rowH - 8, m_rowH - 8);
        x->setIcon(XxriUi::glyph(XxriUi::Cross, m_icon - 4,
                                 QColor(XxriUi::kMuted)));
        x->setIconSize(QSize(m_icon - 4, m_icon - 4));
        x->setToolTip(tr("Close this tab"));
        x->setCursor(Qt::PointingHandCursor);
        connect(x, &QToolButton::clicked, this, [this, i]() {
            emit closeRequested(i);
            // The list is rebuilt in place, so the popover stays open and the
            // user can close several tabs in one visit.
            if (m_tabs->count() == 0)
                close();
            else
                rebuild();
        });
        h->addWidget(x, 0);

        rl->addWidget(row);
    }

    // Painted icons, not text glyphs: the shipped face has no U+21BA and the
    // row would silently lose its symbol.
    auto action = [&](const QString &text, const QIcon &icon,
                      const QString &tip, const std::function<void()> &fn) {
        QToolButton *a = new QToolButton(m_rows);
        a->setObjectName(QStringLiteral("xxriTabAction"));
        a->setText(text);
        a->setIcon(icon);
        a->setIconSize(QSize(m_icon - 4, m_icon - 4));
        a->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        a->setToolTip(tip);
        a->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        a->setFixedHeight(m_rowH - 4);
        a->setCursor(Qt::PointingHandCursor);
        connect(a, &QToolButton::clicked, this, [this, fn]() { fn(); close(); });
        rl->addWidget(a);
    };
    const QColor accent(0x7A, 0x1B, 0xE0);
    action(tr("New Tab"), XxriUi::glyph(XxriUi::Plus, m_icon - 4, accent),
           tr("Ctrl+T"), [this]() { emit newTabRequested(); });
    action(tr("Reopen Closed Tab"),
           XxriUi::glyph(XxriUi::Clock, m_icon - 4, accent),
           tr("Ctrl+Shift+T"), [this]() { emit reopenRequested(); });

    setFixedWidth(m_width);
    adjustSize();
}

void XxriTabPopover::popupUnder(QWidget *anchor)
{
    adjustSize();
    QPoint p = anchor->mapToGlobal(QPoint(anchor->width() - width(),
                                          anchor->height() + 4));
    // Keep it on the screen even when the window is near an edge.
    const QRect avail = QApplication::primaryScreen()->availableGeometry();
    p.setX(qBound(avail.left() + 4, p.x(), avail.right() - width() - 4));
    p.setY(qMin(p.y(), avail.bottom() - height() - 4));
    move(p);
    show();
    setFocus();
}

void XxriTabPopover::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(0.5, 0.5, width() - 1, height() - 1), 10, 10);
    p.fillPath(path, QColor("#FDF8FF"));
    p.setPen(QPen(QColor(120, 90, 130, 80), 1));
    p.drawPath(path);
}

void XxriTabPopover::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(e);
}
