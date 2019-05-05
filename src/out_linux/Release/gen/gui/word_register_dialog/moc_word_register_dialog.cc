/****************************************************************************
** Meta object code from reading C++ file 'word_register_dialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/word_register_dialog/word_register_dialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'word_register_dialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__WordRegisterDialog_t {
    QByteArrayData data[9];
    char stringdata0[120];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__WordRegisterDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__WordRegisterDialog_t qt_meta_stringdata_mozc__gui__WordRegisterDialog = {
    {
QT_MOC_LITERAL(0, 0, 29), // "mozc::gui::WordRegisterDialog"
QT_MOC_LITERAL(1, 30, 7), // "Clicked"
QT_MOC_LITERAL(2, 38, 0), // ""
QT_MOC_LITERAL(3, 39, 16), // "QAbstractButton*"
QT_MOC_LITERAL(4, 56, 6), // "button"
QT_MOC_LITERAL(5, 63, 15), // "LineEditChanged"
QT_MOC_LITERAL(6, 79, 3), // "str"
QT_MOC_LITERAL(7, 83, 15), // "CompleteReading"
QT_MOC_LITERAL(8, 99, 20) // "LaunchDictionaryTool"

    },
    "mozc::gui::WordRegisterDialog\0Clicked\0"
    "\0QAbstractButton*\0button\0LineEditChanged\0"
    "str\0CompleteReading\0LaunchDictionaryTool"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__WordRegisterDialog[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x09 /* Protected */,
       5,    1,   37,    2, 0x09 /* Protected */,
       7,    0,   40,    2, 0x09 /* Protected */,
       8,    0,   41,    2, 0x09 /* Protected */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void mozc::gui::WordRegisterDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        WordRegisterDialog *_t = static_cast<WordRegisterDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->Clicked((*reinterpret_cast< QAbstractButton*(*)>(_a[1]))); break;
        case 1: _t->LineEditChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->CompleteReading(); break;
        case 3: _t->LaunchDictionaryTool(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::WordRegisterDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_mozc__gui__WordRegisterDialog.data,
      qt_meta_data_mozc__gui__WordRegisterDialog,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::WordRegisterDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::WordRegisterDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__WordRegisterDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int mozc::gui::WordRegisterDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
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
QT_WARNING_POP
QT_END_MOC_NAMESPACE
