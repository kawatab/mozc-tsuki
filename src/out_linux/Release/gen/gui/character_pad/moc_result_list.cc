/****************************************************************************
** Meta object code from reading C++ file 'result_list.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/character_pad/result_list.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'result_list.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__ResultList_t {
    QByteArrayData data[10];
    char stringdata0[108];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__ResultList_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__ResultList_t qt_meta_stringdata_mozc__gui__ResultList = {
    {
QT_MOC_LITERAL(0, 0, 21), // "mozc::gui::ResultList"
QT_MOC_LITERAL(1, 22, 12), // "itemSelected"
QT_MOC_LITERAL(2, 35, 0), // ""
QT_MOC_LITERAL(3, 36, 22), // "const QListWidgetItem*"
QT_MOC_LITERAL(4, 59, 4), // "item"
QT_MOC_LITERAL(5, 64, 6), // "update"
QT_MOC_LITERAL(6, 71, 14), // "updateFontSize"
QT_MOC_LITERAL(7, 86, 5), // "index"
QT_MOC_LITERAL(8, 92, 10), // "updateFont"
QT_MOC_LITERAL(9, 103, 4) // "font"

    },
    "mozc::gui::ResultList\0itemSelected\0\0"
    "const QListWidgetItem*\0item\0update\0"
    "updateFontSize\0index\0updateFont\0font"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__ResultList[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   37,    2, 0x0a /* Public */,
       6,    1,   38,    2, 0x0a /* Public */,
       8,    1,   41,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void, QMetaType::QFont,    9,

       0        // eod
};

void mozc::gui::ResultList::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        ResultList *_t = static_cast<ResultList *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->itemSelected((*reinterpret_cast< const QListWidgetItem*(*)>(_a[1]))); break;
        case 1: _t->update(); break;
        case 2: _t->updateFontSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->updateFont((*reinterpret_cast< const QFont(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ResultList::*)(const QListWidgetItem * );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ResultList::itemSelected)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::ResultList::staticMetaObject = {
    { &QListWidget::staticMetaObject, qt_meta_stringdata_mozc__gui__ResultList.data,
      qt_meta_data_mozc__gui__ResultList,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::ResultList::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::ResultList::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__ResultList.stringdata0))
        return static_cast<void*>(this);
    return QListWidget::qt_metacast(_clname);
}

int mozc::gui::ResultList::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QListWidget::qt_metacall(_c, _id, _a);
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
void mozc::gui::ResultList::itemSelected(const QListWidgetItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
