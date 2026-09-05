/****************************************************************************
** Meta object code from reading C++ file 'webpage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.10)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "webpage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'webpage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.10. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_WebPage_t {
    QByteArrayData data[20];
    char stringdata0[377];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_WebPage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_WebPage_t qt_meta_stringdata_WebPage = {
    {
QT_MOC_LITERAL(0, 0, 7), // "WebPage"
QT_MOC_LITERAL(1, 8, 19), // "xxriActionRequested"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 3), // "url"
QT_MOC_LITERAL(4, 33, 28), // "handleAuthenticationRequired"
QT_MOC_LITERAL(5, 62, 10), // "requestUrl"
QT_MOC_LITERAL(6, 73, 15), // "QAuthenticator*"
QT_MOC_LITERAL(7, 89, 4), // "auth"
QT_MOC_LITERAL(8, 94, 32), // "handleFeaturePermissionRequested"
QT_MOC_LITERAL(9, 127, 14), // "securityOrigin"
QT_MOC_LITERAL(10, 142, 7), // "Feature"
QT_MOC_LITERAL(11, 150, 7), // "feature"
QT_MOC_LITERAL(12, 158, 33), // "handleProxyAuthenticationRequ..."
QT_MOC_LITERAL(13, 192, 9), // "proxyHost"
QT_MOC_LITERAL(14, 202, 38), // "handleRegisterProtocolHandler..."
QT_MOC_LITERAL(15, 241, 40), // "QWebEngineRegisterProtocolHan..."
QT_MOC_LITERAL(16, 282, 7), // "request"
QT_MOC_LITERAL(17, 290, 29), // "handleSelectClientCertificate"
QT_MOC_LITERAL(18, 320, 36), // "QWebEngineClientCertificateSe..."
QT_MOC_LITERAL(19, 357, 19) // "clientCertSelection"

    },
    "WebPage\0xxriActionRequested\0\0url\0"
    "handleAuthenticationRequired\0requestUrl\0"
    "QAuthenticator*\0auth\0"
    "handleFeaturePermissionRequested\0"
    "securityOrigin\0Feature\0feature\0"
    "handleProxyAuthenticationRequired\0"
    "proxyHost\0handleRegisterProtocolHandlerRequested\0"
    "QWebEngineRegisterProtocolHandlerRequest\0"
    "request\0handleSelectClientCertificate\0"
    "QWebEngineClientCertificateSelection\0"
    "clientCertSelection"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_WebPage[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   44,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    2,   47,    2, 0x08 /* Private */,
       8,    2,   52,    2, 0x08 /* Private */,
      12,    3,   57,    2, 0x08 /* Private */,
      14,    1,   64,    2, 0x08 /* Private */,
      17,    1,   67,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QUrl,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::QUrl, 0x80000000 | 6,    5,    7,
    QMetaType::Void, QMetaType::QUrl, 0x80000000 | 10,    9,   11,
    QMetaType::Void, QMetaType::QUrl, 0x80000000 | 6, QMetaType::QString,    5,    7,   13,
    QMetaType::Void, 0x80000000 | 15,   16,
    QMetaType::Void, 0x80000000 | 18,   19,

       0        // eod
};

void WebPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<WebPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->xxriActionRequested((*reinterpret_cast< const QUrl(*)>(_a[1]))); break;
        case 1: _t->handleAuthenticationRequired((*reinterpret_cast< const QUrl(*)>(_a[1])),(*reinterpret_cast< QAuthenticator*(*)>(_a[2]))); break;
        case 2: _t->handleFeaturePermissionRequested((*reinterpret_cast< const QUrl(*)>(_a[1])),(*reinterpret_cast< Feature(*)>(_a[2]))); break;
        case 3: _t->handleProxyAuthenticationRequired((*reinterpret_cast< const QUrl(*)>(_a[1])),(*reinterpret_cast< QAuthenticator*(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 4: _t->handleRegisterProtocolHandlerRequested((*reinterpret_cast< QWebEngineRegisterProtocolHandlerRequest(*)>(_a[1]))); break;
        case 5: _t->handleSelectClientCertificate((*reinterpret_cast< QWebEngineClientCertificateSelection(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (WebPage::*)(const QUrl & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&WebPage::xxriActionRequested)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject WebPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWebEnginePage::staticMetaObject>(),
    qt_meta_stringdata_WebPage.data,
    qt_meta_data_WebPage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *WebPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WebPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_WebPage.stringdata0))
        return static_cast<void*>(this);
    return QWebEnginePage::qt_metacast(_clname);
}

int WebPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWebEnginePage::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void WebPage::xxriActionRequested(const QUrl & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
