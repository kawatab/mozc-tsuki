/****************************************************************************
** Meta object code from reading C++ file 'generic_table_editor.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/config_dialog/generic_table_editor.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'generic_table_editor.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__GenericTableEditorDialog_t {
    QByteArrayData data[20];
    char stringdata0[244];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__GenericTableEditorDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__GenericTableEditorDialog_t qt_meta_stringdata_mozc__gui__GenericTableEditorDialog = {
    {
QT_MOC_LITERAL(0, 0, 35), // "mozc::gui::GenericTableEditor..."
QT_MOC_LITERAL(1, 36, 10), // "AddNewItem"
QT_MOC_LITERAL(2, 47, 0), // ""
QT_MOC_LITERAL(3, 48, 10), // "InsertItem"
QT_MOC_LITERAL(4, 59, 19), // "DeleteSelectedItems"
QT_MOC_LITERAL(5, 79, 22), // "OnContextMenuRequested"
QT_MOC_LITERAL(6, 102, 3), // "pos"
QT_MOC_LITERAL(7, 106, 7), // "Clicked"
QT_MOC_LITERAL(8, 114, 16), // "QAbstractButton*"
QT_MOC_LITERAL(9, 131, 6), // "button"
QT_MOC_LITERAL(10, 138, 15), // "InsertEmptyItem"
QT_MOC_LITERAL(11, 154, 3), // "row"
QT_MOC_LITERAL(12, 158, 14), // "UpdateOKButton"
QT_MOC_LITERAL(13, 173, 6), // "status"
QT_MOC_LITERAL(14, 180, 6), // "Import"
QT_MOC_LITERAL(15, 187, 6), // "Export"
QT_MOC_LITERAL(16, 194, 16), // "UpdateMenuStatus"
QT_MOC_LITERAL(17, 211, 16), // "OnEditMenuAction"
QT_MOC_LITERAL(18, 228, 8), // "QAction*"
QT_MOC_LITERAL(19, 237, 6) // "action"

    },
    "mozc::gui::GenericTableEditorDialog\0"
    "AddNewItem\0\0InsertItem\0DeleteSelectedItems\0"
    "OnContextMenuRequested\0pos\0Clicked\0"
    "QAbstractButton*\0button\0InsertEmptyItem\0"
    "row\0UpdateOKButton\0status\0Import\0"
    "Export\0UpdateMenuStatus\0OnEditMenuAction\0"
    "QAction*\0action"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__GenericTableEditorDialog[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   69,    2, 0x09 /* Protected */,
       3,    0,   70,    2, 0x09 /* Protected */,
       4,    0,   71,    2, 0x09 /* Protected */,
       5,    1,   72,    2, 0x09 /* Protected */,
       7,    1,   75,    2, 0x09 /* Protected */,
      10,    1,   78,    2, 0x09 /* Protected */,
      12,    1,   81,    2, 0x09 /* Protected */,
      14,    0,   84,    2, 0x09 /* Protected */,
      15,    0,   85,    2, 0x09 /* Protected */,
      16,    0,   86,    2, 0x09 /* Protected */,
      17,    1,   87,    2, 0x09 /* Protected */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,    6,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void, QMetaType::Int,   11,
    QMetaType::Void, QMetaType::Bool,   13,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 18,   19,

       0        // eod
};

void mozc::gui::GenericTableEditorDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        GenericTableEditorDialog *_t = static_cast<GenericTableEditorDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->AddNewItem(); break;
        case 1: _t->InsertItem(); break;
        case 2: _t->DeleteSelectedItems(); break;
        case 3: _t->OnContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 4: _t->Clicked((*reinterpret_cast< QAbstractButton*(*)>(_a[1]))); break;
        case 5: _t->InsertEmptyItem((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->UpdateOKButton((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: _t->Import(); break;
        case 8: _t->Export(); break;
        case 9: _t->UpdateMenuStatus(); break;
        case 10: _t->OnEditMenuAction((*reinterpret_cast< QAction*(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::GenericTableEditorDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_mozc__gui__GenericTableEditorDialog.data,
      qt_meta_data_mozc__gui__GenericTableEditorDialog,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::GenericTableEditorDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::GenericTableEditorDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__GenericTableEditorDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int mozc::gui::GenericTableEditorDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 11;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
