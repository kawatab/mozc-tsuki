/****************************************************************************
** Meta object code from reading C++ file 'dictionary_tool.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/dictionary_tool/dictionary_tool.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dictionary_tool.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__DictionaryTool_t {
    QByteArrayData data[23];
    char stringdata0[396];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__DictionaryTool_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__DictionaryTool_t qt_meta_stringdata_mozc__gui__DictionaryTool = {
    {
QT_MOC_LITERAL(0, 0, 25), // "mozc::gui::DictionaryTool"
QT_MOC_LITERAL(1, 26, 16), // "CreateDictionary"
QT_MOC_LITERAL(2, 43, 0), // ""
QT_MOC_LITERAL(3, 44, 16), // "DeleteDictionary"
QT_MOC_LITERAL(4, 61, 16), // "RenameDictionary"
QT_MOC_LITERAL(5, 78, 25), // "ImportAndCreateDictionary"
QT_MOC_LITERAL(6, 104, 25), // "ImportAndAppendDictionary"
QT_MOC_LITERAL(7, 130, 20), // "ImportFromDefaultIME"
QT_MOC_LITERAL(8, 151, 16), // "ExportDictionary"
QT_MOC_LITERAL(9, 168, 7), // "AddWord"
QT_MOC_LITERAL(10, 176, 10), // "DeleteWord"
QT_MOC_LITERAL(11, 187, 11), // "CloseWindow"
QT_MOC_LITERAL(12, 199, 14), // "UpdateUIStatus"
QT_MOC_LITERAL(13, 214, 28), // "OnDictionarySelectionChanged"
QT_MOC_LITERAL(14, 243, 13), // "OnItemChanged"
QT_MOC_LITERAL(15, 257, 17), // "QTableWidgetItem*"
QT_MOC_LITERAL(16, 275, 11), // "unused_item"
QT_MOC_LITERAL(17, 287, 15), // "OnHeaderClicked"
QT_MOC_LITERAL(18, 303, 12), // "logicalIndex"
QT_MOC_LITERAL(19, 316, 12), // "OnDeactivate"
QT_MOC_LITERAL(20, 329, 32), // "OnContextMenuRequestedForContent"
QT_MOC_LITERAL(21, 362, 3), // "pos"
QT_MOC_LITERAL(22, 366, 29) // "OnContextMenuRequestedForList"

    },
    "mozc::gui::DictionaryTool\0CreateDictionary\0"
    "\0DeleteDictionary\0RenameDictionary\0"
    "ImportAndCreateDictionary\0"
    "ImportAndAppendDictionary\0"
    "ImportFromDefaultIME\0ExportDictionary\0"
    "AddWord\0DeleteWord\0CloseWindow\0"
    "UpdateUIStatus\0OnDictionarySelectionChanged\0"
    "OnItemChanged\0QTableWidgetItem*\0"
    "unused_item\0OnHeaderClicked\0logicalIndex\0"
    "OnDeactivate\0OnContextMenuRequestedForContent\0"
    "pos\0OnContextMenuRequestedForList"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__DictionaryTool[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   99,    2, 0x08 /* Private */,
       3,    0,  100,    2, 0x08 /* Private */,
       4,    0,  101,    2, 0x08 /* Private */,
       5,    0,  102,    2, 0x08 /* Private */,
       6,    0,  103,    2, 0x08 /* Private */,
       7,    0,  104,    2, 0x08 /* Private */,
       8,    0,  105,    2, 0x08 /* Private */,
       9,    0,  106,    2, 0x08 /* Private */,
      10,    0,  107,    2, 0x08 /* Private */,
      11,    0,  108,    2, 0x08 /* Private */,
      12,    0,  109,    2, 0x08 /* Private */,
      13,    0,  110,    2, 0x08 /* Private */,
      14,    1,  111,    2, 0x08 /* Private */,
      17,    1,  114,    2, 0x08 /* Private */,
      19,    0,  117,    2, 0x08 /* Private */,
      20,    1,  118,    2, 0x08 /* Private */,
      22,    1,  121,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 15,   16,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,   21,
    QMetaType::Void, QMetaType::QPoint,   21,

       0        // eod
};

void mozc::gui::DictionaryTool::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        DictionaryTool *_t = static_cast<DictionaryTool *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->CreateDictionary(); break;
        case 1: _t->DeleteDictionary(); break;
        case 2: _t->RenameDictionary(); break;
        case 3: _t->ImportAndCreateDictionary(); break;
        case 4: _t->ImportAndAppendDictionary(); break;
        case 5: _t->ImportFromDefaultIME(); break;
        case 6: _t->ExportDictionary(); break;
        case 7: _t->AddWord(); break;
        case 8: _t->DeleteWord(); break;
        case 9: _t->CloseWindow(); break;
        case 10: _t->UpdateUIStatus(); break;
        case 11: _t->OnDictionarySelectionChanged(); break;
        case 12: _t->OnItemChanged((*reinterpret_cast< QTableWidgetItem*(*)>(_a[1]))); break;
        case 13: _t->OnHeaderClicked((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 14: _t->OnDeactivate(); break;
        case 15: _t->OnContextMenuRequestedForContent((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 16: _t->OnContextMenuRequestedForList((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::DictionaryTool::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_mozc__gui__DictionaryTool.data,
      qt_meta_data_mozc__gui__DictionaryTool,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::DictionaryTool::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::DictionaryTool::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__DictionaryTool.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int mozc::gui::DictionaryTool::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 17;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
