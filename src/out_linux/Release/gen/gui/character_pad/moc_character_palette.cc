/****************************************************************************
** Meta object code from reading C++ file 'character_palette.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/character_pad/character_palette.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'character_palette.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__CharacterPalette_t {
    QByteArrayData data[15];
    char stringdata0[181];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__CharacterPalette_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__CharacterPalette_t qt_meta_stringdata_mozc__gui__CharacterPalette = {
    {
QT_MOC_LITERAL(0, 0, 27), // "mozc::gui::CharacterPalette"
QT_MOC_LITERAL(1, 28, 11), // "resizeEvent"
QT_MOC_LITERAL(2, 40, 0), // ""
QT_MOC_LITERAL(3, 41, 13), // "QResizeEvent*"
QT_MOC_LITERAL(4, 55, 5), // "event"
QT_MOC_LITERAL(5, 61, 14), // "updateFontSize"
QT_MOC_LITERAL(6, 76, 5), // "index"
QT_MOC_LITERAL(7, 82, 10), // "updateFont"
QT_MOC_LITERAL(8, 93, 4), // "font"
QT_MOC_LITERAL(9, 98, 16), // "categorySelected"
QT_MOC_LITERAL(10, 115, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(11, 132, 4), // "item"
QT_MOC_LITERAL(12, 137, 6), // "column"
QT_MOC_LITERAL(13, 144, 12), // "itemSelected"
QT_MOC_LITERAL(14, 157, 23) // "const QTableWidgetItem*"

    },
    "mozc::gui::CharacterPalette\0resizeEvent\0"
    "\0QResizeEvent*\0event\0updateFontSize\0"
    "index\0updateFont\0font\0categorySelected\0"
    "QTreeWidgetItem*\0item\0column\0itemSelected\0"
    "const QTableWidgetItem*"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__CharacterPalette[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   39,    2, 0x0a /* Public */,
       5,    1,   42,    2, 0x0a /* Public */,
       7,    1,   45,    2, 0x0a /* Public */,
       9,    2,   48,    2, 0x0a /* Public */,
      13,    1,   53,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::QFont,    8,
    QMetaType::Void, 0x80000000 | 10, QMetaType::Int,   11,   12,
    QMetaType::Void, 0x80000000 | 14,   11,

       0        // eod
};

void mozc::gui::CharacterPalette::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        CharacterPalette *_t = static_cast<CharacterPalette *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->resizeEvent((*reinterpret_cast< QResizeEvent*(*)>(_a[1]))); break;
        case 1: _t->updateFontSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->updateFont((*reinterpret_cast< const QFont(*)>(_a[1]))); break;
        case 3: _t->categorySelected((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 4: _t->itemSelected((*reinterpret_cast< const QTableWidgetItem*(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::CharacterPalette::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_mozc__gui__CharacterPalette.data,
      qt_meta_data_mozc__gui__CharacterPalette,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::CharacterPalette::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::CharacterPalette::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__CharacterPalette.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int mozc::gui::CharacterPalette::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
