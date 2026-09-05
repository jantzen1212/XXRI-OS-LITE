TEMPLATE = app
TARGET = xxri-browser
QT += webenginewidgets x11extras
# The diagnostic asks the X server what visual this window actually got; that
# is the one fact Qt does not expose and the one that decides transparency.
LIBS += -lX11

HEADERS += \
    xxridialogs.h \
    xxricompat.h \
    xxridata.h \
    xxripages.h \
    xxriui.h \
    browser.h \
    browserwindow.h \
    downloadmanagerwidget.h \
    downloadwidget.h \
    tabwidget.h \
    webpage.h \
    webpopupwindow.h \
    webview.h

SOURCES += \
    xxridialogs.cpp \
    xxricompat.cpp \
    xxridata.cpp \
    xxripages.cpp \
    xxriui.cpp \
    browser.cpp \
    browserwindow.cpp \
    downloadmanagerwidget.cpp \
    downloadwidget.cpp \
    main.cpp \
    tabwidget.cpp \
    webpage.cpp \
    webpopupwindow.cpp \
    webview.cpp

FORMS += \
    certificateerrordialog.ui \
    passworddialog.ui \
    downloadmanagerwidget.ui \
    downloadwidget.ui

RESOURCES += data/simplebrowser.qrc \
              data/xxri.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webenginewidgets/simplebrowser
INSTALLS += target
