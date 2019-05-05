/****************************************************************************
** Meta object code from reading C++ file 'hand_writing.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/character_pad/hand_writing.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'hand_writing.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__HandWriting_t {
    QByteArrayData data[13];
    char stringdata0[159];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__HandWriting_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__HandWriting_t qt_meta_stringdata_mozc__gui__HandWriting = {
    {
QT_MOC_LITERAL(0, 0, 22), // "mozc::gui::HandWriting"
QT_MOC_LITERAL(1, 23, 14), // "updateFontSize"
QT_MOC_LITERAL(2, 38, 0), // ""
QT_MOC_LITERAL(3, 39, 5), // "index"
QT_MOC_LITERAL(4, 45, 10), // "updateFont"
QT_MOC_LITERAL(5, 56, 4), // "font"
QT_MOC_LITERAL(6, 61, 5), // "clear"
QT_MOC_LITERAL(7, 67, 6), // "revert"
QT_MOC_LITERAL(8, 74, 14), // "updateUIStatus"
QT_MOC_LITERAL(9, 89, 28), // "tryToUpdateHandwritingSource"
QT_MOC_LITERAL(10, 118, 12), // "itemSelected"
QT_MOC_LITERAL(11, 131, 22), // "const QListWidgetItem*"
QT_MOC_LITERAL(12, 154, 4) // "item"

    },
    "mozc::gui::HandWriting\0updateFontSize\0"
    "\0index\0updateFont\0font\0clear\0revert\0"
    "updateUIStatus\0tryToUpdateHandwritingSource\0"
    "itemSelected\0const QListWidgetItem*\0"
    "item"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__HandWriting[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   49,    2, 0x0a /* Public */,
       4,    1,   52,    2, 0x0a /* Public */,
       6,    0,   55,    2, 0x0a /* Public */,
       7,    0,   56,    2, 0x0a /* Public */,
       8,    0,   57,    2, 0x0a /* Public */,
       9,    1,   58,    2, 0x0a /* Public */,
      10,    1,   61,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::QFont,    5,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, 0x80000000 | 11,   12,

       0        // eod
};

void mozc::gui::HandWriting::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        HandWriting *_t = static_cast<HandWriting *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->updateFontSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->updateFont((*reinterpret_cast< const QFont(*)>(_a[1]))); break;
        case 2: _t->clear(); break;
        case 3: _t->revert(); break;
        case 4: _t->updateUIStatus(); break;
        case 5: _t->tryToUpdateHandwritingSource((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->itemSelected((*reinterpret_cast< const QListWidgetItem*(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::HandWriting::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_mozc__gui__HandWriting.data,
      qt_meta_data_mozc__gui__HandWriting,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::HandWriting::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::HandWriting::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__HandWriting.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int mozc::gui::HandWriting::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
