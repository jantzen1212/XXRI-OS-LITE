/****************************************************************************
** Meta object code from reading C++ file 'xxridialogs.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.10)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "xxridialogs.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'xxridialogs.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.10. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_XxriBookmarkDialog_t {
    QByteArrayData data[1];
    char stringdata0[19];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_XxriBookmarkDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_XxriBookmarkDialog_t qt_meta_stringdata_XxriBookmarkDialog = {
    {
QT_MOC_LITERAL(0, 0, 18) // "XxriBookmarkDialog"

    },
    "XxriBookmarkDialog"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_XxriBookmarkDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

void XxriBookmarkDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    (void)_o;
    (void)_id;
    (void)_c;
    (void)_a;
}

QT_INIT_METAOBJECT const QMetaObject XxriBookmarkDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_XxriBookmarkDialog.data,
    qt_meta_data_XxriBookmarkDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *XxriBookmarkDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *XxriBookmarkDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_XxriBookmarkDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int XxriBookmarkDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    return _id;
}
struct qt_meta_stringdata_XxriTabPopover_t {
    QByteArrayData data[7];
    char stringdata0[85];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_XxriTabPopover_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_XxriTabPopover_t qt_meta_stringdata_XxriTabPopover = {
    {
QT_MOC_LITERAL(0, 0, 14), // "XxriTabPopover"
QT_MOC_LITERAL(1, 15, 15), // "newTabRequested"
QT_MOC_LITERAL(2, 31, 0), // ""
QT_MOC_LITERAL(3, 32, 15), // "reopenRequested"
QT_MOC_LITERAL(4, 48, 14), // "closeRequested"
QT_MOC_LITERAL(5, 63, 5), // "index"
QT_MOC_LITERAL(6, 69, 15) // "switchRequested"

    },
    "XxriTabPopover\0newTabRequested\0\0"
    "reopenRequested\0closeRequested\0index\0"
    "switchRequested"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_XxriTabPopover[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   34,    2, 0x06 /* Public */,
       3,    0,   35,    2, 0x06 /* Public */,
       4,    1,   36,    2, 0x06 /* Public */,
       6,    1,   39,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Int,    5,

       0        // eod
};

void XxriTabPopover::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<XxriTabPopover *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->newTabRequested(); break;
        case 1: _t->reopenRequested(); break;
        case 2: _t->closeRequested((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->switchRequested((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (XxriTabPopover::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&XxriTabPopover::newTabRequested)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (XxriTabPopover::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&XxriTabPopover::reopenRequested)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (XxriTabPopover::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&XxriTabPopover::closeRequested)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (XxriTabPopover::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&XxriTabPopover::switchRequested)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject XxriTabPopover::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_XxriTabPopover.data,
    qt_meta_data_XxriTabPopover,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *XxriTabPopover::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *XxriTabPopover::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_XxriTabPopover.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int XxriTabPopover::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void XxriTabPopover::newTabRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void XxriTabPopover::reopenRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void XxriTabPopover::closeRequested(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void XxriTabPopover::switchRequested(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
