/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "browser.h"
#include "browserwindow.h"
#include "xxriui.h"
#include <functional>
#include "downloadmanagerwidget.h"
#include "tabwidget.h"
#include "webview.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QScreen>
#include <QStatusBar>
#include <QToolBar>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QDesktopServices>
#include <QVBoxLayout>
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#include <QWebEngineFindTextResult>
#endif
#include <QWebEngineProfile>
#include <QPainterPath>
#include <QX11Info>
#include <QTabBar>
#include <QTimer>
#include <QWindow>
#include <QLabel>
#include <QMenu>
#include "xxridata.h"
#include "xxripages.h"
#include "xxridialogs.h"
#include <QStyle>
#include <QShortcut>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QWindow>
#include <QPushButton>
#include <QStyle>
#include <functional>
#include <X11/Xlib.h>
// Xlib's unqualified macros collide with Qt's enumerators.
#undef Bool
#undef Status
#undef None
#undef CursorShape
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef Expose
#undef Unsorted

BrowserWindow::BrowserWindow(Browser *browser, QWebEngineProfile *profile, bool forDevTools)
    : m_browser(browser)
    , m_profile(profile)
    , m_tabWidget(new TabWidget(profile, this))
    , m_progressBar(nullptr)
    , m_historyBackAction(nullptr)
    , m_historyForwardAction(nullptr)
    , m_stopAction(nullptr)
    , m_reloadAction(nullptr)
    , m_stopReloadAction(nullptr)
    , m_urlLineEdit(nullptr)
    , m_favAction(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setFocusPolicy(Qt::ClickFocus);

    // One scale for the whole window, derived from the screen - see xxriui.h.
    const QSize def = XxriMetrics::defaultWindow(
                QApplication::primaryScreen()->availableGeometry());
    m_xxri = XxriMetrics::forWindow(def.width());

    // Translucency first: only with a compositor (_NET_WM_CM_S<n>, same test
    // as the GTK apps), before the sidebar and tool bar exist (they check it
    // to decide what to paint) and before the platform window is created (or
    // Qt picks a visual without alpha).
    if (!forDevTools && QX11Info::isCompositingManagerRunning()) {
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

    // ---- XXRI shell -------------------------------------------------------
    // Undecorated, own chrome. The menus below are still built (they own the
    // actions this class looks up) but never shown.
    if (!forDevTools) {
        m_progressBar = new QProgressBar(this);

        QToolBar *hiddenBar = createToolBar();
        hiddenBar->setVisible(false);
        QMenu *fileMenu = createFileMenu(m_tabWidget);
        QMenu *editMenu = createEditMenu();
        QMenu *viewMenu = createViewMenu(hiddenBar);
        QMenu *winMenu  = createWindowMenu(m_tabWidget);
        QMenu *helpMenu = createHelpMenu();
        // keep the accelerators alive without showing a menu bar
        for (QMenu *m : {fileMenu, editMenu, viewMenu, winMenu, helpMenu})
            addActions(m->actions());
        menuBar()->setVisible(false);
    }

    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout;
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    if (!forDevTools) {
        m_xxriSidebar = createXxriSidebar();
        rootLayout->addWidget(m_xxriSidebar);
    }

    QWidget *contentSide = new QWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    if (!forDevTools) {
        QWidget *bar = createXxriToolBar();
        layout->addWidget(bar);

        // on the tool bar's bottom edge while loading; outside the layout so
        // the page doesn't jump
        m_progressBar->setObjectName(QStringLiteral("xxriProgress"));
        m_progressBar->setParent(bar);
        m_progressBar->setTextVisible(false);
        m_progressBar->setRange(0, 100);
        m_progressBar->hide();
        m_xxriProgressHost = bar;
    }

    if (!forDevTools) {
        // Only the sidebar and the tool bar are translucent.  The page column
        // is opaque, so text never competes with the wallpaper and no gap the
        // web view has not painted yet shows the desktop through it.
        m_tabWidget->setAutoFillBackground(true);
        QPalette pagePal = m_tabWidget->palette();
        pagePal.setColor(QPalette::Window, QColor(XxriUi::kPage));
        m_tabWidget->setPalette(pagePal);
    }
    layout->addWidget(m_tabWidget);
    contentSide->setLayout(layout);
    rootLayout->addWidget(contentSide, 1);
    centralWidget->setLayout(rootLayout);
    setCentralWidget(centralWidget);

    if (!forDevTools) {
        // The artwork shows no tab strip: the open-tab count is a control on
        // the right of the tool bar, and that is where tabs are managed.
        m_tabWidget->setObjectName(QStringLiteral("xxriTabs"));
        m_tabWidget->setDocumentMode(true);
        if (QTabBar *tb = m_tabWidget->findChild<QTabBar *>())
            tb->hide();
        statusBar()->hide();

        setWindowFlag(Qt::FramelessWindowHint, true);
        setStyleSheet(XxriUi::styleSheet(m_xxri));
        setWindowTitle(tr("XXRI Browser"));
        QIcon winIcon(QStringLiteral(":/xxri/browser.svg"));
        if (winIcon.isNull()) {
            winIcon = QIcon(QStringLiteral(":/xxri/appicon.png"));
        }
        setWindowIcon(winIcon);
        setMinimumSize(qRound(560 * m_xxri.s / 0.7), qRound(380 * m_xxri.s / 0.7));

        // The five edges XXRI's GTK applications overlay (xxri-chrome.h):
        // east, west, south and the two southern corners.  The top edge is
        // the window controls and the drag area, so it is not a grip.
        static const Qt::Edges kGrips[] = {
            Qt::RightEdge, Qt::LeftEdge, Qt::BottomEdge,
            Qt::RightEdge | Qt::BottomEdge,
            Qt::LeftEdge  | Qt::BottomEdge,
        };
        for (Qt::Edges g : kGrips) {
            XxriResizeGrip *grip = new XxriResizeGrip(this, g, this);
            grip->show();
            m_xxriGrips.append(grip);
        }

        const QRect avail = QApplication::primaryScreen()->availableGeometry();
        const QSize d = XxriMetrics::defaultWindow(avail);
        resize(d);
        // inset from the top left like the artwork, a bit more vertically
        move(avail.left() + qRound(avail.width() * 0.02),
             avail.top() + qRound(avail.height() * 0.04));
        m_xxriSidebar->setFixedWidth(xxriSidebarWidth());
        installXxriShortcuts();

        // XXRI_BROWSER_DIAG=1: compositor, visual and GL surface alpha can
        // each fail and look the same on screen, so print all three
        if (qEnvironmentVariableIsSet("XXRI_BROWSER_DIAG")) {
            QTimer::singleShot(1500, this, [this]() {
                QWindow *h = windowHandle();
                int depth = -1;
                if (Display *d = QX11Info::display()) {
                    XWindowAttributes a;
                    if (XGetWindowAttributes(d, Window(winId()), &a))
                        depth = a.depth;
                }
                fprintf(stderr,
                        "xxri-diag: compositingManager=%d translucentAttr=%d "
                        "winDepth=%d surfaceAlpha=%d geometry=%dx%d+%d+%d\n",
                        int(QX11Info::isCompositingManagerRunning()),
                        int(testAttribute(Qt::WA_TranslucentBackground)),
                        depth,
                        h ? h->format().alphaBufferSize() : -1,
                        width(), height(), x(), y());
                fflush(stderr);
            });
        }
    }

    connect(m_tabWidget, &TabWidget::titleChanged, this, &BrowserWindow::handleWebViewTitleChanged);
    if (!forDevTools) {
        connect(m_tabWidget, &TabWidget::linkHovered, [this](const QString& url) {
            statusBar()->showMessage(url);
        });
        connect(m_tabWidget, &TabWidget::loadProgress, this, &BrowserWindow::handleWebViewLoadProgress);
        connect(m_tabWidget, &TabWidget::webActionEnabledChanged, this, &BrowserWindow::handleWebActionEnabledChanged);
        connect(m_tabWidget, &TabWidget::urlChanged, [this](const QUrl &url) {
            if (url.scheme() == QLatin1String("xxri"))
                m_urlLineEdit->clear();
            else
                m_urlLineEdit->setText(url.toDisplayString());
        });
        connect(m_tabWidget, &TabWidget::favIconChanged, m_favAction, &QAction::setIcon);
        connect(m_tabWidget, &TabWidget::devToolsRequested, this, &BrowserWindow::handleDevToolsRequested);
        connect(m_urlLineEdit, &QLineEdit::returnPressed, this, &BrowserWindow::handleXxriAddressEntered);

        connect(m_tabWidget, &TabWidget::loadFinished, this, [this](bool ok) {
            // A host typed without a scheme was tried over TLS.  If that did
            // not load, fall back to http once - a site that only answers on
            // port 80 has to remain reachable by typing its name.
            if (ok || m_xxriHttpsUpgrade.isEmpty())
                return;
            WebView *v = m_tabWidget->currentWebView();
            if (!v || v->url() != m_xxriHttpsUpgrade)
                return;
            QUrl plain = m_xxriHttpsUpgrade;
            m_xxriHttpsUpgrade = QUrl();
            plain.setScheme(QStringLiteral("http"));
            m_tabWidget->setUrl(plain);
        });
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        connect(m_tabWidget, &TabWidget::findTextFinished, this, &BrowserWindow::handleFindTextFinished);
#endif

        // record visits for History
        connect(m_tabWidget, &TabWidget::urlChanged, this, [this](const QUrl &u) {
            if (u.scheme() == QLatin1String("xxri"))
                return;
            XxriEntry e;
            e.url = u;
            // no title yet: at urlChanged() it's still the previous page's;
            // titleChanged() fills it in
            XxriStore::instance()->noteVisit(e);
            rebuildXxriBookmarks();
            refreshXxriIcons();
        });
        connect(m_tabWidget, &TabWidget::titleChanged, this,
                [this](const QString &title) {
            // Naming an entry only; a title arriving late must never create a
            // second row for a page that is already recorded.
            if (!m_tabWidget->currentWebView())
                return;
            XxriStore::instance()->nameVisit(
                        m_tabWidget->currentWebView()->url(), title);
        });

        // Keep the tool bar's tab counter in step with the tab set.
        connect(m_tabWidget, &QTabWidget::currentChanged, this,
                &BrowserWindow::syncXxriTabs);
        // Title, icon or address of ANY tab - the sidebar shows them all, so
        // the current-tab-only signals are not enough on their own.
        connect(m_tabWidget, &TabWidget::tabsChanged, this,
                &BrowserWindow::rebuildXxriTabRows);
        connect(m_tabWidget, &QTabWidget::tabCloseRequested, this,
                &BrowserWindow::syncXxriTabs);
        QTimer::singleShot(0, this, &BrowserWindow::syncXxriTabs);

        connect(m_tabWidget, &TabWidget::xxriActionRequested, this,
                [this](const QUrl &u) {
            const QString what = u.path().mid(1);          // after the leading /
            if (what == QLatin1String("clear-history")) {
                XxriStore::instance()->clearHistory();
                if (WebView *v = m_tabWidget->currentWebView())
                    v->page()->triggerAction(QWebEnginePage::Reload);
                else
                    openXxriHistory();
            } else if (what == QLatin1String("history")) {
                openXxriHistory();
            }
        });

        // every site icon (tab list, bookmark tiles, history page) starts here
        connect(m_tabWidget, &TabWidget::favIconChanged, this,
                [this](const QIcon &) {
            // page()->icon(), not the signal's favIcon(): that one substitutes
            // Qt's loading/document/error pixmaps, which got filed as site
            // icons and made our own pages reload in a loop (every 715ms)
            if (WebView *v = m_tabWidget->currentWebView())
                XxriIcons::instance()->remember(v->url(), v->page()->icon());
        });

        QAction *reopenAction = new QAction(tr("Reopen Closed Tab"), this);
        reopenAction->setText(tr("Reopen Closed Tab")
                              + QLatin1String("\tCtrl+Shift+T"));
        addAction(reopenAction);
        connect(reopenAction, &QAction::triggered, this,
                &BrowserWindow::reopenXxriClosedTab);

        // Ctrl+D bookmarks the current page into the sidebar's slots.
        m_xxriBookmarkAction = new QAction(tr("Bookmark This Page"), this);
        m_xxriBookmarkAction->setText(tr("Bookmark This Page")
                                      + QLatin1String("\tCtrl+D"));
        addAction(m_xxriBookmarkAction);
        connect(m_xxriBookmarkAction, &QAction::triggered, this,
                [this]() { addBookmarkForCurrentPage(); });

        QAction *focusUrlLineEditAction = new QAction(this);
        addAction(focusUrlLineEditAction);

        connect(focusUrlLineEditAction, &QAction::triggered, this, [this] () {
            m_urlLineEdit->setFocus(Qt::ShortcutFocusReason);
        });
    }

    handleWebViewTitleChanged(QString());
    WebView *first = m_tabWidget->createTab();
    if (!forDevTools && first)
        first->setUrl(QUrl(QLatin1String(XxriPages::kNewTab)));
}

QSize BrowserWindow::sizeHint() const
{
    // the artwork's 1.476 aspect at most of the screen width; on a short
    // screen the height decides
    const QRect avail = QApplication::primaryScreen()->availableGeometry();
    int w = qBound(640, qRound(avail.width() * 0.88), 1100);
    int h = qRound(w / 1.476);
    if (h > avail.height() - 120) {
        h = qMax(420, avail.height() - 120);
        w = qRound(h * 1.476);
    }
    return QSize(w, h);
}

QMenu *BrowserWindow::createFileMenu(TabWidget *tabWidget)
{
    QMenu *fileMenu = new QMenu(tr("&File"));
    fileMenu->addAction(tr("&New Window"), this, &BrowserWindow::handleNewWindowTriggered, QKeySequence::New);
    fileMenu->addAction(tr("New &Incognito Window"), this, &BrowserWindow::handleNewIncognitoWindowTriggered);

    m_xxriNewTabAction = new QAction(tr("New &Tab"), this);
    // no setShortcut(): installXxriShortcuts() owns the keys, and the same
    // sequence twice is "ambiguous" and fires neither. The \t text is only
    // what the menu shows.
    m_xxriNewTabAction->setText(tr("New &Tab") + QLatin1String("\tCtrl+T"));
    connect(m_xxriNewTabAction, &QAction::triggered, this,
            [this]() { openXxriNewTab(); });
    fileMenu->addAction(m_xxriNewTabAction);

    fileMenu->addAction(tr("&Open File..."), this, &BrowserWindow::handleFileOpenTriggered, QKeySequence::Open);
    fileMenu->addSeparator();

    m_xxriCloseTabAction = new QAction(tr("&Close Tab"), this);
    m_xxriCloseTabAction->setText(tr("&Close Tab") + QLatin1String("\tCtrl+W"));
    connect(m_xxriCloseTabAction, &QAction::triggered, this,
            [this]() { closeXxriTab(-1); });
    fileMenu->addAction(m_xxriCloseTabAction);

    QAction *closeAction = new QAction(tr("&Quit"),this);
    closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(closeAction);

    connect(fileMenu, &QMenu::aboutToShow, [this, closeAction]() {
        if (m_browser->windows().count() == 1)
            closeAction->setText(tr("&Quit"));
        else
            closeAction->setText(tr("&Close Window"));
    });
    return fileMenu;
}

QMenu *BrowserWindow::createEditMenu()
{
    QMenu *editMenu = new QMenu(tr("&Edit"));
    QAction *findAction = editMenu->addAction(tr("&Find"));
    findAction->setShortcuts(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &BrowserWindow::handleFindActionTriggered);

    QAction *findNextAction = editMenu->addAction(tr("Find &Next"));
    findNextAction->setShortcut(QKeySequence::FindNext);
    connect(findNextAction, &QAction::triggered, [this]() {
        if (!currentTab() || m_lastSearch.isEmpty())
            return;
        currentTab()->findText(m_lastSearch);
    });

    QAction *findPreviousAction = editMenu->addAction(tr("Find &Previous"));
    findPreviousAction->setShortcut(QKeySequence::FindPrevious);
    connect(findPreviousAction, &QAction::triggered, [this]() {
        if (!currentTab() || m_lastSearch.isEmpty())
            return;
        currentTab()->findText(m_lastSearch, QWebEnginePage::FindBackward);
    });

    return editMenu;
}

QMenu *BrowserWindow::createViewMenu(QToolBar *toolbar)
{
    QMenu *viewMenu = new QMenu(tr("&View"));
    m_stopAction = viewMenu->addAction(tr("&Stop"));
    QList<QKeySequence> shortcuts;
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Period));
    shortcuts.append(Qt::Key_Escape);
    m_stopAction->setShortcuts(shortcuts);
    connect(m_stopAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Stop);
    });

    m_reloadAction = viewMenu->addAction(tr("Reload Page"));
    m_reloadAction->setShortcuts(QKeySequence::Refresh);
    connect(m_reloadAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Reload);
    });

    QAction *zoomIn = viewMenu->addAction(tr("Zoom &In"));
    zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(zoomIn, &QAction::triggered, [this]() {
        if (currentTab())
            currentTab()->setZoomFactor(currentTab()->zoomFactor() + 0.1);
    });

    QAction *zoomOut = viewMenu->addAction(tr("Zoom &Out"));
    zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(zoomOut, &QAction::triggered, [this]() {
        if (currentTab())
            currentTab()->setZoomFactor(currentTab()->zoomFactor() - 0.1);
    });

    QAction *resetZoom = viewMenu->addAction(tr("Reset &Zoom"));
    resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(resetZoom, &QAction::triggered, [this]() {
        if (currentTab())
            currentTab()->setZoomFactor(1.0);
    });


    viewMenu->addSeparator();
    QAction *viewToolbarAction = new QAction(tr("Hide Toolbar"),this);
    viewToolbarAction->setShortcut(tr("Ctrl+|"));
    connect(viewToolbarAction, &QAction::triggered, [toolbar,viewToolbarAction]() {
        if (toolbar->isVisible()) {
            viewToolbarAction->setText(tr("Show Toolbar"));
            toolbar->close();
        } else {
            viewToolbarAction->setText(tr("Hide Toolbar"));
            toolbar->show();
        }
    });
    viewMenu->addAction(viewToolbarAction);

    QAction *viewStatusbarAction = new QAction(tr("Hide Status Bar"), this);
    viewStatusbarAction->setShortcut(tr("Ctrl+/"));
    connect(viewStatusbarAction, &QAction::triggered, [this, viewStatusbarAction]() {
        if (statusBar()->isVisible()) {
            viewStatusbarAction->setText(tr("Show Status Bar"));
            statusBar()->close();
        } else {
            viewStatusbarAction->setText(tr("Hide Status Bar"));
            statusBar()->show();
        }
    });
    viewMenu->addAction(viewStatusbarAction);
    return viewMenu;
}

QMenu *BrowserWindow::createWindowMenu(TabWidget *tabWidget)
{
    QMenu *menu = new QMenu(tr("&Window"));

    QAction *nextTabAction = new QAction(tr("Show Next Tab"), this);
    QList<QKeySequence> shortcuts;
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BraceRight));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Less));
    nextTabAction->setShortcuts(shortcuts);
    connect(nextTabAction, &QAction::triggered, tabWidget, &TabWidget::nextTab);

    QAction *previousTabAction = new QAction(tr("Show Previous Tab"), this);
    shortcuts.clear();
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BraceLeft));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Greater));
    previousTabAction->setShortcuts(shortcuts);
    connect(previousTabAction, &QAction::triggered, tabWidget, &TabWidget::previousTab);

    connect(menu, &QMenu::aboutToShow, [this, menu, nextTabAction, previousTabAction]() {
        menu->clear();
        menu->addAction(nextTabAction);
        menu->addAction(previousTabAction);
        menu->addSeparator();

        QVector<BrowserWindow*> windows = m_browser->windows();
        int index(-1);
        for (auto window : windows) {
            QAction *action = menu->addAction(window->windowTitle(), this, &BrowserWindow::handleShowWindowTriggered);
            action->setData(++index);
            action->setCheckable(true);
            if (window == this)
                action->setChecked(true);
        }
    });
    return menu;
}

QMenu *BrowserWindow::createHelpMenu()
{
    QMenu *helpMenu = new QMenu(tr("&Help"));
    helpMenu->addAction(tr("About &Qt"), qApp, QApplication::aboutQt);
    return helpMenu;
}

QToolBar *BrowserWindow::createToolBar()
{
    QToolBar *navigationBar = new QToolBar(tr("Navigation"));
    navigationBar->setMovable(false);
    navigationBar->toggleViewAction()->setEnabled(false);

    m_historyBackAction = new QAction(this);
    QList<QKeySequence> backShortcuts = QKeySequence::keyBindings(QKeySequence::Back);
    for (auto it = backShortcuts.begin(); it != backShortcuts.end();) {
        // Chromium already handles navigate on backspace when appropriate.
        if ((*it)[0] == Qt::Key_Backspace)
            it = backShortcuts.erase(it);
        else
            ++it;
    }
    // For some reason Qt doesn't bind the dedicated Back key to Back.
    backShortcuts.append(QKeySequence(Qt::Key_Back));
    m_historyBackAction->setShortcuts(backShortcuts);
    m_historyBackAction->setIconVisibleInMenu(false);
    m_historyBackAction->setIcon(QIcon(QStringLiteral(":go-previous.png")));
    m_historyBackAction->setToolTip(tr("Go back in history"));
    connect(m_historyBackAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Back);
    });
    navigationBar->addAction(m_historyBackAction);

    m_historyForwardAction = new QAction(this);
    QList<QKeySequence> fwdShortcuts = QKeySequence::keyBindings(QKeySequence::Forward);
    for (auto it = fwdShortcuts.begin(); it != fwdShortcuts.end();) {
        if (((*it)[0] & Qt::Key_unknown) == Qt::Key_Backspace)
            it = fwdShortcuts.erase(it);
        else
            ++it;
    }
    fwdShortcuts.append(QKeySequence(Qt::Key_Forward));
    m_historyForwardAction->setShortcuts(fwdShortcuts);
    m_historyForwardAction->setIconVisibleInMenu(false);
    m_historyForwardAction->setIcon(QIcon(QStringLiteral(":go-next.png")));
    m_historyForwardAction->setToolTip(tr("Go forward in history"));
    connect(m_historyForwardAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Forward);
    });
    navigationBar->addAction(m_historyForwardAction);

    m_stopReloadAction = new QAction(this);
    connect(m_stopReloadAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::WebAction(m_stopReloadAction->data().toInt()));
    });
    navigationBar->addAction(m_stopReloadAction);

    m_urlLineEdit = new QLineEdit(this);
    m_favAction = new QAction(this);
    m_urlLineEdit->addAction(m_favAction, QLineEdit::LeadingPosition);
    m_urlLineEdit->setClearButtonEnabled(true);
    navigationBar->addWidget(m_urlLineEdit);

    auto downloadsAction = new QAction(this);
    downloadsAction->setIcon(QIcon(QStringLiteral(":go-bottom.png")));
    downloadsAction->setToolTip(tr("Show downloads"));
    navigationBar->addAction(downloadsAction);
    connect(downloadsAction, &QAction::triggered, [this]() {
        m_browser->downloadManagerWidget().show();
    });

    return navigationBar;
}

void BrowserWindow::handleWebActionEnabledChanged(QWebEnginePage::WebAction action, bool enabled)
{
    switch (action) {
    case QWebEnginePage::Back:
        m_historyBackAction->setEnabled(enabled);
        break;
    case QWebEnginePage::Forward:
        m_historyForwardAction->setEnabled(enabled);
        break;
    case QWebEnginePage::Reload:
        m_reloadAction->setEnabled(enabled);
        break;
    case QWebEnginePage::Stop:
        m_stopAction->setEnabled(enabled);
        break;
    default:
        qWarning("Unhandled webActionChanged signal");
    }
}

void BrowserWindow::handleWebViewTitleChanged(const QString &title)
{
    QString suffix = m_profile->isOffTheRecord()
        ? tr("Qt Simple Browser (Incognito)")
        : tr("Qt Simple Browser");

    if (title.isEmpty())
        setWindowTitle(suffix);
    else
        setWindowTitle(title + " - " + suffix);
}

void BrowserWindow::handleNewWindowTriggered()
{
    BrowserWindow *window = m_browser->createWindow();
    window->m_urlLineEdit->setFocus();
}

void BrowserWindow::handleNewIncognitoWindowTriggered()
{
    BrowserWindow *window = m_browser->createWindow(/* offTheRecord: */ true);
    window->m_urlLineEdit->setFocus();
}

void BrowserWindow::handleFileOpenTriggered()
{
    QUrl url = QFileDialog::getOpenFileUrl(this, tr("Open Web Resource"), QString(),
                                                tr("Web Resources (*.html *.htm *.svg *.png *.gif *.svgz);;All files (*.*)"));
    if (url.isEmpty())
        return;
    currentTab()->setUrl(url);
}

void BrowserWindow::handleFindActionTriggered()
{
    if (!currentTab())
        return;
    bool ok = false;
    QString search = QInputDialog::getText(this, tr("Find"),
                                           tr("Find:"), QLineEdit::Normal,
                                           m_lastSearch, &ok);
    if (ok && !search.isEmpty()) {
        m_lastSearch = search;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        currentTab()->findText(m_lastSearch);
#else
        currentTab()->findText(m_lastSearch, 0, [this](bool found) {
            if (!found)
                statusBar()->showMessage(tr("\"%1\" not found.").arg(m_lastSearch));
        });
#endif
    }
}

// yes/no question as a frameless #xxriDialog, not a stock framed QMessageBox
bool BrowserWindow::xxriConfirm(const QString &title, const QString &text,
                                const QString &detail, const QString &accept)
{
    QMessageBox box(this);
    box.setObjectName(QStringLiteral("xxriDialog"));
    box.setIcon(QMessageBox::NoIcon);
    box.setWindowFlag(Qt::FramelessWindowHint, true);
    // styled, not translucent: QMessageBox doesn't paint itself, so a
    // translucent one is just text floating over the page
    box.setAttribute(Qt::WA_StyledBackground, true);
    box.setWindowTitle(title);
    box.setText(text);
    if (!detail.isEmpty())
        box.setInformativeText(detail);

    QPushButton *go = box.addButton(accept, QMessageBox::AcceptRole);
    // Named and re-polished: QMessageBox styles its buttons as they are added,
    // so an object name set afterwards needs the style told about it or the
    // primary button comes out looking like the secondary one.
    go->setObjectName(QStringLiteral("xxriPrimary"));
    go->style()->unpolish(go);
    go->style()->polish(go);
    QPushButton *keep = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(keep);

    box.adjustSize();
    const QRect g = geometry();
    box.move(g.center().x() - box.width() / 2,
             g.center().y() - box.height() / 2);
    box.exec();
    return box.clickedButton() == go;
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    if (m_tabWidget->count() > 1) {
        // an XXRI dialog, not the example's stock QMessageBox::warning()
        if (!xxriConfirm(tr("Close XXRI Browser"), tr("Close this window?"),
                         tr("%1 tabs are open.").arg(m_tabWidget->count()),
                         tr("Close"))) {
            event->ignore();
            return;
        }
    }
    event->accept();
    deleteLater();
}

TabWidget *BrowserWindow::tabWidget() const
{
    return m_tabWidget;
}

WebView *BrowserWindow::currentTab() const
{
    return m_tabWidget->currentWebView();
}

void BrowserWindow::handleWebViewLoadProgress(int progress)
{
    static QIcon stopIcon(QStringLiteral(":process-stop.png"));
    static QIcon reloadIcon(QStringLiteral(":view-refresh.png"));

    const bool loading = 0 < progress && progress < 100;
    if (loading) {
        m_stopReloadAction->setData(QWebEnginePage::Stop);
        m_stopReloadAction->setIcon(stopIcon);
        m_stopReloadAction->setToolTip(tr("Stop loading the current page"));
        m_progressBar->setValue(progress);
    } else {
        m_stopReloadAction->setData(QWebEnginePage::Reload);
        m_stopReloadAction->setIcon(reloadIcon);
        m_stopReloadAction->setToolTip(tr("Reload the current page"));
        m_progressBar->setValue(0);
    }
    if (m_xxriProgressHost) {
        const int h = qMax(2, qRound(3 * m_xxri.s));
        m_progressBar->setGeometry(0, m_xxriProgressHost->height() - h,
                                   m_xxriProgressHost->width(), h);
        m_progressBar->setVisible(loading);
        if (loading)
            m_progressBar->raise();
    }
}

void BrowserWindow::handleShowWindowTriggered()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int offset = action->data().toInt();
        QVector<BrowserWindow*> windows = m_browser->windows();
        windows.at(offset)->activateWindow();
        windows.at(offset)->currentTab()->setFocus();
    }
}

void BrowserWindow::handleDevToolsRequested(QWebEnginePage *source)
{
    source->setDevToolsPage(m_browser->createDevToolsWindow()->currentTab()->page());
    source->triggerAction(QWebEnginePage::InspectElement);
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
void BrowserWindow::handleFindTextFinished(const QWebEngineFindTextResult &result)
{
    if (result.numberOfMatches() == 0) {
        statusBar()->showMessage(tr("\"%1\" not found.").arg(m_lastSearch));
    } else {
        statusBar()->showMessage(tr("\"%1\" found: %2/%3").arg(m_lastSearch,
                                                                QString::number(result.activeMatch()),
                                                                QString::number(result.numberOfMatches())));
    }
}
#endif

/*
 * The omnibox: URL or search? Not QUrl::fromUserInput(), which turns anything
 * into a URL ("browser" -> http://browser/). In this order:
 *
 *   1. a leading "?" is always a search
 *   2. a scheme we navigate (http, https, file, ftp, data, about, view-source,
 *      xxri) is a URL; other "word:" prefixes ("define: foo") are searches
 *   3. no whitespace and a reachable authority - localhost, an IP literal, or
 *      a dotted name with a plausible TLD - is a URL, over https (http for
 *      literals and localhost)
 *   4. everything else is a search ("browser", "how to install linux")
 */
namespace {

/* Google unless the user has chosen otherwise - see XxriStore::searchUrl(). */
QString xxriSearchEndpoint()
{
    return XxriStore::instance()->searchUrl();
}

bool xxriPlausibleTld(const QString &label)
{
    if (label.size() < 2)
        return false;
    for (const QChar &c : label)
        if (!c.isLetter())
            return false;
    return true;
}

bool xxriIsIpv4(const QString &host)
{
    const QStringList parts = host.split(QLatin1Char('.'));
    if (parts.size() != 4)
        return false;
    for (const QString &p : parts) {
        if (p.isEmpty() || p.size() > 3)
            return false;
        bool ok = false;
        const int v = p.toInt(&ok);
        if (!ok || v < 0 || v > 255)
            return false;
    }
    return true;
}

/* The authority part of a typed string: everything before the first / ? # */
QString xxriAuthority(const QString &text)
{
    int end = text.size();
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('/') || c == QLatin1Char('?') || c == QLatin1Char('#')) {
            end = i;
            break;
        }
    }
    return text.left(end);
}

QUrl xxriSearchUrl(const QString &terms)
{
    QUrl u(xxriSearchEndpoint());
    // by hand, not QUrlQuery: that leaves "+" alone and engines read it as a
    // space ("c++ vector" became "c vector")
    u.setQuery(QStringLiteral("q=")
               + QString::fromLatin1(QUrl::toPercentEncoding(terms))
               + QStringLiteral("&ie=UTF-8"), QUrl::StrictMode);
    return u;
}

} // namespace

QUrl BrowserWindow::xxriResolveInput(const QString &raw, bool *upgradedToHttps)
{
    if (upgradedToHttps)
        *upgradedToHttps = false;
    const QString text = raw.trimmed();
    if (text.isEmpty())
        return QUrl();

    // 1. an explicit search
    if (text.startsWith(QLatin1Char('?')))
        return xxriSearchUrl(text.mid(1).trimmed());

    // 2. an explicit scheme
    static const QRegularExpression schemeRe(
        QStringLiteral("^([a-zA-Z][a-zA-Z0-9+.-]*):"));
    const QRegularExpressionMatch sm = schemeRe.match(text);
    if (sm.hasMatch()) {
        const QString scheme = sm.captured(1).toLower();
        static const QStringList kNavigable = {
            QStringLiteral("http"), QStringLiteral("https"),
            QStringLiteral("file"), QStringLiteral("ftp"),
            QStringLiteral("data"), QStringLiteral("about"),
            QStringLiteral("view-source"), QStringLiteral("xxri"),
            QStringLiteral("chrome"), QStringLiteral("blob")
        };
        if (kNavigable.contains(scheme)) {
            const QUrl u(text, QUrl::TolerantMode);
            if (u.isValid())
                return u;
        }
        // "localhost:8080/x" looks scheme-shaped but is a host and a port;
        // anything else with a colon and no navigable scheme is a search.
        const QString host = scheme;
        const QString rest = text.mid(sm.capturedLength());
        bool portOk = false;
        const QString portPart = xxriAuthority(rest);
        portPart.toInt(&portOk);
        const bool hostLike = host == QLatin1String("localhost")
                || xxriIsIpv4(host)
                || (host.contains(QLatin1Char('.'))
                    && xxriPlausibleTld(host.section(QLatin1Char('.'), -1)));
        if (!(portOk && !portPart.isEmpty() && hostLike))
            return xxriSearchUrl(text);
        // fall through: host:port is a URL
        if (upgradedToHttps)
            *upgradedToHttps = true;
        return QUrl(QStringLiteral("http://") + text, QUrl::TolerantMode);
    }

    // 3. a bare authority
    static const QRegularExpression spaceRe(QStringLiteral("\\s"));
    if (!text.contains(spaceRe)) {
        QString host = xxriAuthority(text);
        const int at = host.lastIndexOf(QLatin1Char('@'));
        if (at >= 0)
            host = host.mid(at + 1);
        const bool literal = host == QLatin1String("localhost")
                || xxriIsIpv4(host)
                || (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']')));
        const bool named = host.contains(QLatin1Char('.'))
                && !host.endsWith(QLatin1Char('.'))
                && xxriPlausibleTld(host.section(QLatin1Char('.'), -1));
        if (literal || named) {
            const QString scheme = literal ? QStringLiteral("http://")
                                           : QStringLiteral("https://");
            if (upgradedToHttps && !literal)
                *upgradedToHttps = true;
            const QUrl u(scheme + text, QUrl::TolerantMode);
            if (u.isValid() && !u.host().isEmpty())
                return u;
        }
    }

    // 4. a search
    return xxriSearchUrl(text);
}

void BrowserWindow::handleXxriAddressEntered()
{
    if (!m_urlLineEdit)
        return;
    const QString text = m_urlLineEdit->text().trimmed();
    if (text.isEmpty())
        return;

    bool upgraded = false;
    const QUrl url = xxriResolveInput(text, &upgraded);
    if (!url.isValid() || url.isEmpty())
        return;

    // remember our own https upgrade; if it doesn't connect, retry that one
    // URL over http, once (port-80-only sites)
    m_xxriHttpsUpgrade = upgraded ? url : QUrl();
    m_tabWidget->setUrl(url);
}

// ===========================================================================
// The XXRI shell. All sizes come from XxriMetrics (see xxriui.h).
// ===========================================================================

int BrowserWindow::xxriSidebarWidth() const
{
    // The artwork's sidebar is 236 of a 1325px window.  Clamped so a very
    // small window keeps a usable rail and a very large one does not turn the
    // sidebar into a panel.
    return qBound(132, qRound(width() * (236.0 / 1325.0)), 260);
}

QWidget *BrowserWindow::createXxriSidebar()
{
    const XxriMetrics &m = m_xxri;

    XxriSidebar *side = new XxriSidebar(this, m, this);
    // XxriSidebar paints its own background in paintEvent

    QVBoxLayout *v = new QVBoxLayout(side);
    v->setContentsMargins(m.pad, 0, m.pad, m.pad);
    v->setSpacing(0);

    // each element at the mockup's distance from the window top; the spacer
    // before it takes whatever the element above left
    int y = 0;
    auto placeAt = [&](int target, QWidget *w, Qt::Alignment align) {
        v->addSpacing(qMax(0, target - y));
        y = qMax(y, target);
        v->addWidget(w, 0, align);
        // not sizeHint() alone: the tile grid's is still empty here
        y += qMax(w->sizeHint().height(), w->minimumSize().height());
    };

    XxriWindowControls *controls = new XxriWindowControls(this, m, side);
    placeAt(m.yControls, controls, Qt::AlignLeft);

    XxriWordmark *mark = new XxriWordmark(m.fWordmark, side);
    placeAt(m.yWordmark, mark, Qt::AlignLeft);

    // bookmark section: heading + edit control, then the tile grid (the
    // artwork's three empty pills under the label are gone)
    QWidget *bmHead = new QWidget(side);
    bmHead->setFixedHeight(qMax(m.fLabel + 4, qRound(20 * m.s)));
    QHBoxLayout *bh = new QHBoxLayout(bmHead);
    bh->setContentsMargins(0, 0, 0, 0);
    bh->setSpacing(0);
    QLabel *bmLabel = new QLabel(tr("Bookmark"), bmHead);
    bmLabel->setObjectName(QStringLiteral("xxriSectionLabel"));
    bh->addWidget(bmLabel, 0, Qt::AlignVCenter);
    bh->addStretch(1);

    const int editBox = qMax(14, qRound(20 * m.s));
    m_xxriEditBookmarks = new QToolButton(bmHead);
    m_xxriEditBookmarks->setObjectName(QStringLiteral("xxriEditBookmarks"));
    m_xxriEditBookmarks->setFixedSize(editBox, editBox);
    m_xxriEditBookmarks->setIconSize(QSize(editBox - 6, editBox - 6));
    XxriUi::setGlyph(m_xxriEditBookmarks, XxriUi::Pencil, editBox - 6, true);
    m_xxriEditBookmarks->setCursor(Qt::PointingHandCursor);
    m_xxriEditBookmarks->setToolTip(tr("Edit bookmarks"));
    m_xxriEditBookmarks->setProperty("on", false);
    connect(m_xxriEditBookmarks, &QToolButton::clicked, this, [this]() {
        m_xxriBookmarkEdit = !m_xxriBookmarkEdit;
        m_xxriEditBookmarks->setProperty("on", m_xxriBookmarkEdit);
        m_xxriEditBookmarks->setToolTip(m_xxriBookmarkEdit
                                        ? tr("Done editing bookmarks")
                                        : tr("Edit bookmarks"));
        m_xxriEditBookmarks->style()->unpolish(m_xxriEditBookmarks);
        m_xxriEditBookmarks->style()->polish(m_xxriEditBookmarks);
        rebuildXxriBookmarks();
    });
    bh->addWidget(m_xxriEditBookmarks, 0, Qt::AlignVCenter);
    placeAt(m.yLabel, bmHead, Qt::Alignment());
    bmHead->setFixedWidth(m.sidebar - 2 * m.pad);

    // --- the bookmark grid ---
    m_xxriTileGrid = new QWidget(side);
    QGridLayout *grid = new QGridLayout(m_xxriTileGrid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(m.tileGap);
    placeAt(m.yTiles, m_xxriTileGrid, Qt::AlignLeft);

    // footer: the dotted rule with "Clear" on its right end, one row
    v->addSpacing(m.gridGap);

    const int ruleH = qMax(12, qRound(16 * m.s));
    QWidget *ruleRow = new QWidget(side);
    ruleRow->setFixedHeight(ruleH);
    ruleRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout *rr = new QHBoxLayout(ruleRow);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(qMax(3, qRound(9 * m.s)));
    rr->addWidget(new XxriDottedRule(ruleRow), 1);

    m_xxriClear = new QToolButton(ruleRow);
    m_xxriClear->setObjectName(QStringLiteral("xxriClear"));
    m_xxriClear->setText(tr("Clear"));
    XxriUi::setGlyph(m_xxriClear, XxriUi::ArrowDown, m.fClear);
    m_xxriClear->setIconSize(QSize(m.fClear, m.fClear));
    m_xxriClear->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_xxriClear->setCursor(Qt::PointingHandCursor);
    m_xxriClear->setToolTip(tr("Remove every bookmark"));
    // Sized to its type, not to a button height: the artwork's row is a
    // hairline with a small label beside it.
    m_xxriClear->setFixedHeight(ruleH);
    connect(m_xxriClear, &QToolButton::clicked, this,
            &BrowserWindow::clearXxriBookmarks);
    rr->addWidget(m_xxriClear, 0, Qt::AlignVCenter);
    v->addWidget(ruleRow);
    v->addSpacing(m.ruleGap);

    // --- navigation rows ---

    auto addRow = [&](const QString &text, XxriUi::Glyph icon, const QString &tip,
                      const std::function<void()> &fn) {
        QToolButton *b = new QToolButton(side);
        b->setObjectName(QStringLiteral("xxriSideItem"));
        b->setText(text);
        XxriUi::setGlyph(b, icon, m.itemIcon);
        b->setIconSize(QSize(m.itemIcon, m.itemIcon));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFixedHeight(m.itemH);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, fn);
        v->addWidget(b);
        return b;
    };

    addRow(tr("New Tab"), XxriUi::Plus,
           tr("Open a new tab (Ctrl+T)"), [this]() { openXxriNewTab(); });
    addRow(tr("History"), XxriUi::Clock,
           tr("Everything this browser has visited"),
           [this]() { openXxriHistory(); });

    // the open tabs (the artwork's rows under History; its tab counter says 3),
    // rebuilt whenever the tab set changes
    m_xxriTabList = new QWidget(side);
    QVBoxLayout *tabRows = new QVBoxLayout(m_xxriTabList);
    tabRows->setContentsMargins(0, 0, 0, 0);
    tabRows->setSpacing(0);
    m_xxriTabList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    v->addWidget(m_xxriTabList);

    v->addStretch(1);

    connect(XxriIcons::instance(), &XxriIcons::changed, this,
            &BrowserWindow::refreshXxriIcons);
    connect(XxriIcons::instance(), &XxriIcons::changed, this,
            &BrowserWindow::refreshXxriPage);
    connect(XxriStore::instance(), &XxriStore::bookmarksChanged, this,
            &BrowserWindow::rebuildXxriBookmarks);
    // A start page that is open when the engine changes has the old one in its
    // form; redraw it so the field it shows is the field it will use.
    connect(XxriStore::instance(), &XxriStore::searchEngineChanged, this,
            [this]() {
        m_xxriRedrawUrl = QUrl();
        m_xxriRedrawCount = 0;
        refreshXxriPage();
    });

    rebuildXxriBookmarks();
    return side;
}

QWidget *BrowserWindow::createXxriToolBar()
{
    const XxriMetrics &m = m_xxri;

    XxriToolBar *bar = new XxriToolBar(this, m, this);
    // XxriToolBar paints its own background in paintEvent

    QHBoxLayout *h = new QHBoxLayout(bar);
    h->setContentsMargins(m.barLead, 0, m.barEnd, 0);
    h->setSpacing(0);

    auto navButton = [&](XxriUi::Glyph g, const QString &tip) {
        QToolButton *b = new QToolButton(bar);
        b->setObjectName(QStringLiteral("xxriNav"));
        XxriUi::setGlyph(b, g, m.navIcon);
        b->setIconSize(QSize(m.navIcon, m.navIcon));
        b->setFixedSize(m.navBox, m.navBox);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        h->addWidget(b);
        return b;
    };

    QToolButton *back = navButton(XxriUi::ChevronLeft, tr("Back (Alt+Left)"));
    QToolButton *fwd  = navButton(XxriUi::ChevronRight, tr("Forward (Alt+Right)"));
    QToolButton *home = navButton(XxriUi::House, tr("Home"));

    auto bind = [](QToolButton *b, QAction *a) {
        if (!a)
            return;
        QObject::connect(b, &QToolButton::clicked, a, &QAction::trigger);
        QObject::connect(a, &QAction::changed, b,
                         [b, a]() { b->setEnabled(a->isEnabled()); });
        b->setEnabled(a->isEnabled());
    };
    bind(back, m_historyBackAction);
    bind(fwd, m_historyForwardAction);
    connect(home, &QToolButton::clicked, this, [this]() {
        m_tabWidget->setUrl(QUrl(QStringLiteral("https://xxri.flows.best")));
    });

    h->addSpacing(m.barPillGap);

    // The address pill runs from the home button to the right-hand controls,
    // exactly as the artwork draws it.
    if (m_urlLineEdit) {
        m_urlLineEdit->setObjectName(QStringLiteral("xxriAddress"));
        m_urlLineEdit->setParent(bar);
        m_urlLineEdit->setAlignment(Qt::AlignCenter);
        m_urlLineEdit->setFixedHeight(m.pillH);
        m_urlLineEdit->setFrame(false);
        m_urlLineEdit->setClearButtonEnabled(false);
        m_urlLineEdit->setPlaceholderText(tr("Search or enter address"));
        m_urlLineEdit->setToolTip(tr("Address bar (Ctrl+L)"));
        if (m_favAction)
            m_urlLineEdit->removeAction(m_favAction);
        h->addWidget(m_urlLineEdit, 1);
        // setParent() hides a widget, and a hidden widget gets no space.
        m_urlLineEdit->show();
    }

    h->addSpacing(m.barRightGap);

    // Right: tabs, new tab, menu.
    m_xxriTabCounter = new XxriTabCounter(bar);
    m_xxriTabCounter->setFixedSize(m.navBox, m.navBox);
    connect(m_xxriTabCounter, &QToolButton::clicked, this,
            [this]() { showXxriTabMenu(m_xxriTabCounter); });
    h->addWidget(m_xxriTabCounter);
    h->addSpacing(m.barBtnGap);

    QToolButton *newTab = new QToolButton(bar);
    newTab->setObjectName(QStringLiteral("xxriNav"));
    newTab->setFixedSize(m.navBox, m.navBox);
    XxriUi::setGlyph(newTab, XxriUi::NewTabMark, m.navIcon);
    newTab->setIconSize(QSize(m.navIcon, m.navIcon));
    newTab->setToolTip(tr("New tab (Ctrl+T)"));
    newTab->setCursor(Qt::PointingHandCursor);
    connect(newTab, &QToolButton::clicked, this, [this]() { openXxriNewTab(); });
    h->addWidget(newTab);
    h->addSpacing(m.barBtnGap);

    QToolButton *menuBtn = new QToolButton(bar);
    menuBtn->setObjectName(QStringLiteral("xxriNav"));
    menuBtn->setFixedSize(m.navBox, m.navBox);
    XxriUi::setGlyph(menuBtn, XxriUi::MenuMark, m.navIcon);
    menuBtn->setIconSize(QSize(m.navIcon, m.navIcon));
    menuBtn->setToolTip(tr("Browser menu"));
    menuBtn->setCursor(Qt::PointingHandCursor);
    connect(menuBtn, &QToolButton::clicked, this,
            [this, menuBtn]() { showXxriMenu(menuBtn); });
    h->addWidget(menuBtn);

    return bar;
}

// --- shortcut tiles ---------------------------------------------------------

// one tile per bookmark, then the add tile; columns from the sidebar width
void BrowserWindow::rebuildXxriBookmarks()
{
    if (!m_xxriTileGrid)
        return;
    QGridLayout *grid = qobject_cast<QGridLayout *>(m_xxriTileGrid->layout());
    if (!grid)
        return;
    const XxriMetrics &m = m_xxri;

    while (QLayoutItem *it = grid->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            // Detached before it is scheduled - deleteLater() alone leaves the
            // widget parented and on screen until the event loop runs.
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete it;
    }
    m_xxriTiles.clear();

    const QVector<XxriEntry> bm = XxriStore::instance()->bookmarks();
    const int usable = qMax(m.tile, m.sidebar - 2 * m.pad);
    const int cols   = qMax(1, (usable + m.tileGap) / (m.tile + m.tileGap));
    const int cells  = bm.size() + 1;                 // the bookmarks and "+"
    const int rows   = (cells + cols - 1) / cols;
    const int badge  = qMax(12, qRound(m.tile * 0.44));

    for (int i = 0; i < cells; ++i) {
        const bool isAdd = (i == bm.size());
        QToolButton *tile = new QToolButton(m_xxriTileGrid);
        tile->setFixedSize(m.tile, m.tile);
        tile->setCursor(Qt::PointingHandCursor);

        if (isAdd) {
            // the add tile: dashed, so it doesn't look like a site whose
            // icon failed to load
            tile->setObjectName(QStringLiteral("xxriAddTile"));
            XxriUi::setGlyph(tile, XxriUi::Plus, m.tile / 2, true);
            tile->setIconSize(QSize(m.tile / 2, m.tile / 2));
            tile->setToolTip(tr("Bookmark this page"));
            connect(tile, &QToolButton::clicked, this,
                    [this]() { addBookmarkForCurrentPage(); });
        } else {
            const XxriEntry e = bm.at(i);
            tile->setObjectName(QStringLiteral("xxriTile"));
            const QIcon ic = (e.url.host() == QLatin1String("xxri.flows.best"))
                    ? QIcon(QStringLiteral(":/xxri/logo.png"))
                    : XxriIcons::instance()->iconOrGeneric(e.url, m.tile);
            tile->setIcon(ic);
            tile->setIconSize(QSize(qRound(m.tile * 0.62), qRound(m.tile * 0.62)));
            tile->setToolTip(e.title.isEmpty()
                             ? e.url.toString()
                             : e.title + QLatin1String("\n") + e.url.toString());
            const QUrl u = e.url;
            connect(tile, &QToolButton::clicked, this, [this, u]() {
                if (u.isValid() && !u.isEmpty())
                    m_tabWidget->setUrl(u);
            });
            tile->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(tile, &QToolButton::customContextMenuRequested, this,
                    [this, i, tile](const QPoint &p) {
                showBookmarkMenu(i, tile->mapToGlobal(p));
            });

            if (m_xxriBookmarkEdit) {
                // Edit mode: a remove badge on the tile itself, so the grid
                // keeps its shape instead of turning into a settings list.
                QToolButton *rm = new QToolButton(tile);
                rm->setObjectName(QStringLiteral("xxriTileRemove"));
                rm->setFixedSize(badge, badge);
                rm->setIcon(XxriUi::glyph(XxriUi::Minus, badge - 6,
                                          QColor(0xE0, 0x41, 0x7B)));
                rm->setIconSize(QSize(badge - 6, badge - 6));
                rm->setCursor(Qt::PointingHandCursor);
                rm->setToolTip(tr("Remove this bookmark"));
                rm->move(m.tile - badge, 0);
                rm->raise();
                rm->show();
                connect(rm, &QToolButton::clicked, this, [this, i]() {
                    XxriStore::instance()->removeBookmark(i);
                });
            }
        }
        // The object name decides which style rule applies, so it has to be
        // re-polished when it changes.
        tile->style()->unpolish(tile);
        tile->style()->polish(tile);
        grid->addWidget(tile, i / cols, i % cols);
        m_xxriTiles.append(tile);
    }

    // A short last row stays left-aligned rather than spreading out.
    m_xxriTileGrid->setFixedSize(cols * m.tile + (cols - 1) * m.tileGap,
                                 rows * m.tile + (rows - 1) * m.tileGap);

    if (m_xxriClear)
        m_xxriClear->setEnabled(!bm.isEmpty());
    if (m_xxriBookmarkAction && m_tabWidget->currentWebView()) {
        m_xxriBookmarkAction->setChecked(XxriStore::instance()->isBookmarked(
                                             m_tabWidget->currentWebView()->url()));
    }
}

// "Clear" removes every bookmark, so it asks first
void BrowserWindow::clearXxriBookmarks()
{
    const int n = XxriStore::instance()->bookmarks().size();
    if (n == 0)
        return;

    if (xxriConfirm(tr("Clear Bookmarks"), tr("Remove every bookmark?"),
                    n == 1 ? tr("One bookmark will be removed.")
                           : tr("%1 bookmarks will be removed.").arg(n),
                    tr("Remove All")))
        XxriStore::instance()->clearBookmarks();
}

void BrowserWindow::addBookmarkForCurrentPage()
{
    WebView *view = m_tabWidget->currentWebView();
    QString name = view ? view->title() : QString();
    QUrl    url  = view ? view->url() : QUrl();
    if (url.scheme() == QLatin1String("xxri")) {          // a browser page
        name.clear();
        url = QUrl();
    }
    QIcon icon = view ? view->page()->icon() : QIcon();
    if (icon.isNull() && !url.isEmpty())
        icon = XxriIcons::instance()->iconOrGeneric(url, 32);

    XxriBookmarkDialog dlg(tr("Add Bookmark"), name, url, icon, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QUrl chosen = dlg.bookmarkUrl();
    if (!chosen.isValid() || chosen.isEmpty())
        return;

    XxriEntry e;
    e.title = dlg.bookmarkName().isEmpty() ? chosen.host() : dlg.bookmarkName();
    e.url   = chosen;
    XxriStore::instance()->addBookmark(e);

    // fetch the icon now, or the new tile stays blank
    XxriIcons::instance()->iconFor(chosen);
}

void BrowserWindow::showBookmarkMenu(int index, const QPoint &global)
{
    const QVector<XxriEntry> bm = XxriStore::instance()->bookmarks();
    QMenu menu(this);
    if (index >= bm.size()) {
        menu.addAction(tr("Add Bookmark..."), this,
                       [this]() { addBookmarkForCurrentPage(); });
        menu.exec(global);
        return;
    }
    const XxriEntry e = bm.at(index);
    menu.addAction(tr("Open"), this, [this, e]() { m_tabWidget->setUrl(e.url); });
    menu.addAction(tr("Open in New Tab"), this, [this, e]() {
        if (WebView *v = m_tabWidget->createTab())
            v->setUrl(e.url);
    });
    menu.addSeparator();
    menu.addAction(tr("Edit..."), this, [this, index, e]() {
        XxriBookmarkDialog dlg(tr("Edit Bookmark"), e.title, e.url,
                               XxriIcons::instance()->iconOrGeneric(e.url, 32),
                               this);
        if (dlg.exec() == QDialog::Accepted && dlg.bookmarkUrl().isValid()) {
            XxriEntry n;
            n.title = dlg.bookmarkName().isEmpty() ? dlg.bookmarkUrl().host()
                                                   : dlg.bookmarkName();
            n.url   = dlg.bookmarkUrl();
            XxriStore::instance()->replaceBookmark(index, n);
        }
    });
    menu.addAction(tr("Remove"), this, [index]() {
        XxriStore::instance()->removeBookmark(index);
    });
    menu.exec(global);
}

void BrowserWindow::refreshXxriIcons()
{
    // rebuilt, not patched, so a late icon reaches every tile and row
    rebuildXxriBookmarks();
    rebuildXxriTabRows();
}

void BrowserWindow::applyXxriTheme()
{
    // the developer-tools window keeps Qt's own look
    if (m_tabWidget->objectName() != QLatin1String("xxriTabs"))
        return;
    setStyleSheet(XxriUi::styleSheet(m_xxri));
    QPalette pagePal = m_tabWidget->palette();
    pagePal.setColor(QPalette::Window, QColor(XxriUi::kPage));
    m_tabWidget->setPalette(pagePal);
    XxriUi::restyleGlyphs(this);
    rebuildXxriBookmarks();
    rebuildXxriTabRows();
    // the browser's own pages are HTML made with the palette
    WebView *v = m_tabWidget->currentWebView();
    if (v && v->url().scheme() == QLatin1String(XxriPages::kScheme))
        v->page()->triggerAction(QWebEnginePage::Reload);
    const auto kids = findChildren<QWidget *>();
    for (QWidget *w : kids)
        w->update();
    update();
}

/* Our pages embed site icons at render time, so redraw when a late one
 * arrives. Only from the icons-changed signal - from urlChanged it would
 * reload forever - and rate-limited, a cold profile gets several at once. */
void BrowserWindow::refreshXxriPage()
{
    WebView *v = m_tabWidget->currentWebView();
    if (!v || v->url().scheme() != QLatin1String(XxriPages::kScheme))
        return;
    if (m_xxriPageRedrawPending)
        return;
    // three redraws per visit at most, whatever keeps emitting
    if (v->url() != m_xxriRedrawUrl) {
        m_xxriRedrawUrl = v->url();
        m_xxriRedrawCount = 0;
    }
    if (m_xxriRedrawCount >= 3)
        return;
    ++m_xxriRedrawCount;
    m_xxriPageRedrawPending = true;
    QTimer::singleShot(700, this, [this]() {
        m_xxriPageRedrawPending = false;
        WebView *w = m_tabWidget->currentWebView();
        if (w && w->url().scheme() == QLatin1String(XxriPages::kScheme))
            w->page()->triggerAction(QWebEnginePage::Reload);
    });
}

// --- the browser's own pages ------------------------------------------------

void BrowserWindow::openXxriNewTab()
{
    m_xxriRedrawUrl = QUrl();
    m_xxriRedrawCount = 0;
    WebView *v = m_tabWidget->createTab();
    if (v)
        v->setUrl(QUrl(QLatin1String(XxriPages::kNewTab)));
    if (m_urlLineEdit) {
        m_urlLineEdit->clear();
        m_urlLineEdit->setFocus(Qt::ShortcutFocusReason);
    }
    syncXxriTabs();
}

void BrowserWindow::openXxriHistory()
{
    m_xxriRedrawUrl = QUrl();
    m_xxriRedrawCount = 0;
    if (WebView *v = m_tabWidget->currentWebView())
        v->setUrl(QUrl(QLatin1String(XxriPages::kHistory)));
}

// --- tabs -------------------------------------------------------------------
// No tab strip in the artwork: the tab counter's popover is the tab UI.

void BrowserWindow::syncXxriTabs()
{
    if (m_xxriTabCounter) {
        const int n = qMax(1, m_tabWidget->count());
        m_xxriTabCounter->setCount(n);
        m_xxriTabCounter->setToolTip(n == 1 ? tr("1 tab open - click to manage")
                                            : tr("%1 tabs open - click to manage").arg(n));
    }
    rebuildXxriTabRows();
}

/* Sidebar tab rows, rebuilt from the tab set every time (never edited in
 * place). Only as many rows as fit the sidebar; past that the last row says
 * how many more and opens the tab popover. */
void BrowserWindow::rebuildXxriTabRows()
{
    if (!m_xxriTabList)
        return;
    QVBoxLayout *rows = qobject_cast<QVBoxLayout *>(m_xxriTabList->layout());
    if (!rows)
        return;
    while (QLayoutItem *it = rows->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete it;
    }

    const XxriMetrics &m = m_xxri;
    const int total   = m_tabWidget->count();
    const int current = m_tabWidget->currentIndex();

    // What is left below History, in whole rows.  Nothing follows the list.
    const int gridBottom = m_xxriTileGrid
            ? qMax(m.yTiles, m_xxriTileGrid->y()) + m_xxriTileGrid->height()
            : m.yTiles;
    const int ruleH = qMax(12, qRound(16 * m.s));
    const int used = gridBottom + m.gridGap + ruleH + m.ruleGap
                   + 2 * m.itemH + m.pad;          // the rule row, New Tab, History
    const int room = m_xxriSidebar ? m_xxriSidebar->height() - used : 0;
    const int fits = qBound(1, room / qMax(1, m.itemH), 24);
    const bool overflow = total > fits;
    const int shown = overflow ? qMax(1, fits - 1) : total;
    // when they don't all fit, slide the window to keep the current tab shown
    const int start = overflow
            ? qBound(0, current - shown + 1, total - shown) : 0;

    const QFontMetrics fm(font());
    for (int i = start; i < start + shown; ++i) {
        WebView *view = m_tabWidget->viewAt(i);
        const QUrl u = view ? view->url() : QUrl();
        QString title = m_tabWidget->tabText(i).trimmed();
        if (title.isEmpty())
            title = u.host().isEmpty() ? tr("New Tab") : u.host();
        // The engine's own icon first; WebView::favIcon() substitutes Qt's
        // document and loading pixmaps, which are not the site's icon.
        QIcon ic = view ? view->page()->icon() : QIcon();
        if (ic.isNull())
            ic = XxriIcons::instance()->iconOrGeneric(u, m.itemIcon);

        QWidget *row = new QWidget(m_xxriTabList);
        row->setObjectName(QStringLiteral("xxriTabRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setProperty("current", i == current);
        row->setFixedHeight(m.itemH);
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, qMax(2, qRound(4 * m.s)), 0);
        h->setSpacing(0);

        const int closeW = qMax(14, m.itemIcon);
        QToolButton *b = new QToolButton(row);
        b->setObjectName(QStringLiteral("xxriTabItem"));
        b->setProperty("current", i == current);
        b->setIcon(ic);
        b->setIconSize(QSize(m.itemIcon, m.itemIcon));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFixedHeight(m.itemH);
        b->setCursor(Qt::PointingHandCursor);
        b->setText(fm.elidedText(title, Qt::ElideRight,
                                 qMax(40, m.sidebar - 2 * m.pad - m.itemIcon
                                          - closeW - qRound(22 * m.s))));
        b->setToolTip(u.isEmpty() ? title
                                  : title + QLatin1Char('\n') + u.toString());
        connect(b, &QToolButton::clicked, this, [this, i]() {
            if (i >= 0 && i < m_tabWidget->count()) {
                m_tabWidget->setCurrentIndex(i);
                syncXxriTabs();
            }
        });
        h->addWidget(b, 1);

        QToolButton *x = new QToolButton(row);
        x->setObjectName(QStringLiteral("xxriTabItemClose"));
        x->setProperty("current", i == current);
        x->setFixedSize(closeW, closeW);
        XxriUi::setGlyph(x, XxriUi::Cross, closeW - 4, true);
        x->setIconSize(QSize(closeW - 4, closeW - 4));
        x->setCursor(Qt::PointingHandCursor);
        x->setToolTip(tr("Close this tab"));
        connect(x, &QToolButton::clicked, this, [this, i]() {
            closeXxriTab(i);
        });
        h->addWidget(x, 0, Qt::AlignVCenter);

        rows->addWidget(row);
    }

    if (overflow) {
        QToolButton *more = new QToolButton(m_xxriTabList);
        more->setObjectName(QStringLiteral("xxriSideItem"));
        XxriUi::setGlyph(more, XxriUi::TabStack, m.itemIcon, true);
        more->setIconSize(QSize(m.itemIcon, m.itemIcon));
        more->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        more->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        more->setFixedHeight(m.itemH);
        more->setCursor(Qt::PointingHandCursor);
        more->setText(tr("%1 more tabs").arg(total - shown));
        more->setToolTip(tr("Every open tab: switch, close or open another"));
        connect(more, &QToolButton::clicked, this,
                [this, more]() { showXxriTabMenu(more); });
        rows->addWidget(more);
    }

    m_xxriTabList->setFixedHeight(rows->count() * m.itemH);
}

void BrowserWindow::showXxriTabMenu(QWidget *anchor)
{
    XxriTabPopover *pop = new XxriTabPopover(m_tabWidget, m_xxri, this);
    connect(pop, &XxriTabPopover::switchRequested, this, [this](int i) {
        m_tabWidget->setCurrentIndex(i);
        syncXxriTabs();
    });
    connect(pop, &XxriTabPopover::closeRequested, this, [this](int i) {
        closeXxriTab(i);
    });
    connect(pop, &XxriTabPopover::newTabRequested, this,
            [this]() { openXxriNewTab(); });
    connect(pop, &XxriTabPopover::reopenRequested, this,
            &BrowserWindow::reopenXxriClosedTab);
    pop->popupUnder(anchor);
}

void BrowserWindow::closeXxriTab(int index)
{
    if (index < 0)
        index = m_tabWidget->currentIndex();
    if (index < 0 || index >= m_tabWidget->count())
        return;
    // Remember where it was, so Ctrl+Shift+T can bring it back.
    if (WebView *v = m_tabWidget->viewAt(index)) {
        const QUrl u = v->url();
        if (u.isValid() && !u.isEmpty() && u.scheme() != QLatin1String("xxri")) {
            m_xxriClosedTabs.append(u);
            while (m_xxriClosedTabs.size() > 16)
                m_xxriClosedTabs.removeFirst();
        }
    }
    m_tabWidget->closeTab(index);
    // never leave an empty window
    if (m_tabWidget->count() == 0)
        openXxriNewTab();
    syncXxriTabs();
}

void BrowserWindow::reopenXxriClosedTab()
{
    if (m_xxriClosedTabs.isEmpty())
        return;
    const QUrl u = m_xxriClosedTabs.takeLast();
    if (WebView *v = m_tabWidget->createTab())
        v->setUrl(u);
    syncXxriTabs();
}

// --- menu -------------------------------------------------------------------

// the tool bar menu, written out by hand: walking the hidden menu bar's actions
// leaked the example's items ("Hide Toolbar", "About Qt")
void BrowserWindow::showXxriMenu(QWidget *anchor)
{
    QMenu menu(this);
    WebView *view = m_tabWidget->currentWebView();

    auto add = [&menu](const QString &text, const QString &keys,
                       const std::function<void()> &fn, bool enabled = true) {
        QAction *a = menu.addAction(keys.isEmpty()
                                    ? text : text + QLatin1Char('\t') + keys);
        a->setEnabled(enabled);
        QObject::connect(a, &QAction::triggered, &menu, fn);
        return a;
    };

    add(tr("New Tab"), QStringLiteral("Ctrl+T"), [this]() { openXxriNewTab(); });
    add(tr("New Window"), QStringLiteral("Ctrl+N"),
        [this]() { handleNewWindowTriggered(); });
    add(tr("Reopen Closed Tab"), QStringLiteral("Ctrl+Shift+T"),
        [this]() { reopenXxriClosedTab(); }, !m_xxriClosedTabs.isEmpty());
    menu.addSeparator();

    add(tr("Reload"), QStringLiteral("F5"), [this]() { reloadXxriPage(); },
        view != nullptr);
    add(tr("Back"), QStringLiteral("Alt+Left"), [this]() { goXxriBack(); },
        m_historyBackAction && m_historyBackAction->isEnabled());
    add(tr("Forward"), QStringLiteral("Alt+Right"), [this]() { goXxriForward(); },
        m_historyForwardAction && m_historyForwardAction->isEnabled());
    menu.addSeparator();

    const bool realPage = view && view->url().isValid()
            && view->url().scheme() != QLatin1String("xxri")
            && !view->url().isEmpty();
    add(tr("Bookmark This Page"), QStringLiteral("Ctrl+D"),
        [this]() { addBookmarkForCurrentPage(); }, realPage);
    add(tr("History"), QString(), [this]() { openXxriHistory(); });
    add(tr("All Tabs"), QString(),
        [this, anchor]() { showXxriTabMenu(anchor); });
    add(tr("Downloads"), QString(),
        [this]() { m_browser->downloadManagerWidget().show(); });
    menu.addSeparator();

    // search engine for the address bar and start page; saved
    QMenu *engines = menu.addMenu(tr("Search Engine"));
    struct Engine { const char *name; const char *url; };
    static const Engine kEngines[] = {
        { "Google",      "https://www.google.com/search" },
        { "DuckDuckGo",  "https://duckduckgo.com/" },
        { "Bing",        "https://www.bing.com/search" },
    };
    const QString current = XxriStore::instance()->searchUrl();
    for (const Engine &e : kEngines) {
        QAction *a = engines->addAction(tr(e.name));
        const QString url = QString::fromLatin1(e.url);
        a->setCheckable(true);
        a->setChecked(url == current);
        connect(a, &QAction::triggered, &menu, [url]() {
            XxriStore::instance()->setSearchUrl(url);
        });
    }
    menu.addSeparator();

    add(tr("Find on Page"), QStringLiteral("Ctrl+F"),
        [this]() { handleFindActionTriggered(); }, view != nullptr);
    add(tr("Zoom In"), QStringLiteral("Ctrl++"), [this]() {
        if (WebView *v = m_tabWidget->currentWebView())
            v->setZoomFactor(qMin(3.0, v->zoomFactor() + 0.1));
    }, view != nullptr);
    add(tr("Zoom Out"), QStringLiteral("Ctrl+-"), [this]() {
        if (WebView *v = m_tabWidget->currentWebView())
            v->setZoomFactor(qMax(0.3, v->zoomFactor() - 0.1));
    }, view != nullptr);
    add(tr("Reset Zoom"), QStringLiteral("Ctrl+0"), [this]() {
        if (WebView *v = m_tabWidget->currentWebView())
            v->setZoomFactor(1.0);
    }, view != nullptr);
    menu.addSeparator();

    add(tr("Close Tab"), QStringLiteral("Ctrl+W"),
        [this]() { closeXxriTab(-1); }, m_tabWidget->count() > 1);
    add(tr("Quit XXRI Browser"), QStringLiteral("Ctrl+Q"),
        [this]() { close(); });

    menu.exec(anchor->mapToGlobal(QPoint(anchor->width() - menu.sizeHint().width(),
                                         anchor->height() + 2)));
}

// --- frameless window behaviour ---------------------------------------------
// Five XxriResizeGrips (E, W, S and the two bottom corners, sized like
// xxri-chrome.h) above the web view, or the edges can't be reached.

// Qt::ApplicationShortcut so keys work with focus inside the page too: the
// render process has its own input path and window shortcuts never see them
void BrowserWindow::installXxriShortcuts()
{
    struct Binding { const char *seq; void (BrowserWindow::*fn)(); };
    static const Binding kBindings[] = {
        { "Ctrl+T",       &BrowserWindow::openXxriNewTab        },
        { "Ctrl+Shift+T", &BrowserWindow::reopenXxriClosedTab   },
        { "Ctrl+W",       &BrowserWindow::closeCurrentXxriTab   },
        { "Ctrl+D",       &BrowserWindow::bookmarkCurrentPage   },
        { "Ctrl+L",       &BrowserWindow::focusXxriAddressBar   },
        { "Ctrl+R",       &BrowserWindow::reloadXxriPage        },
        { "F5",           &BrowserWindow::reloadXxriPage        },
        { "Alt+Left",     &BrowserWindow::goXxriBack            },
        { "Alt+Right",    &BrowserWindow::goXxriForward         },
    };
    for (const Binding &b : kBindings) {
        QShortcut *sc = new QShortcut(QKeySequence(QString::fromLatin1(b.seq)), this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, this, b.fn);
        m_xxriShortcuts.append(sc);
    }
}

void BrowserWindow::closeCurrentXxriTab() { closeXxriTab(-1); }
void BrowserWindow::bookmarkCurrentPage() { addBookmarkForCurrentPage(); }

void BrowserWindow::focusXxriAddressBar()
{
    if (m_urlLineEdit) {
        m_urlLineEdit->setFocus(Qt::ShortcutFocusReason);
        m_urlLineEdit->selectAll();
    }
}

void BrowserWindow::reloadXxriPage()
{
    if (WebView *v = m_tabWidget->currentWebView())
        v->page()->triggerAction(QWebEnginePage::Reload);
}

void BrowserWindow::goXxriBack()
{
    if (WebView *v = m_tabWidget->currentWebView())
        v->page()->triggerAction(QWebEnginePage::Back);
}

void BrowserWindow::goXxriForward()
{
    if (WebView *v = m_tabWidget->currentWebView())
        v->page()->triggerAction(QWebEnginePage::Forward);
}

void BrowserWindow::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    if (m_xxriSidebar)
        m_xxriSidebar->setFixedWidth(xxriSidebarWidth());
    // how many tab rows fit depends on the height
    rebuildXxriTabRows();

    if (m_xxriGrips.size() == 5) {
        const int T = m_xxri.gripEdge, K = m_xxri.gripCorner;
        const int w = width(), h = height();
        m_xxriGrips[0]->setGeometry(w - T, 0, T, h - K);       // east
        m_xxriGrips[1]->setGeometry(0, 0, T, h - K);           // west
        m_xxriGrips[2]->setGeometry(K, h - T, w - 2 * K, T);   // south
        m_xxriGrips[3]->setGeometry(w - K, h - K, K, K);       // south-east
        m_xxriGrips[4]->setGeometry(0, h - K, K, K);           // south-west
        for (QWidget *g : qAsConst(m_xxriGrips))
            g->raise();
    }

    if (windowFlags().testFlag(Qt::FramelessWindowHint)
            && !testAttribute(Qt::WA_TranslucentBackground)) {
        // no compositor: round the corners with a shape mask. Never with
        // one - shape + ARGB composites against nothing
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, width(), height()),
                            m_xxri.corner, m_xxri.corner);
        setMask(QRegion(path.toFillPolygon().toPolygon()));
    }
}
