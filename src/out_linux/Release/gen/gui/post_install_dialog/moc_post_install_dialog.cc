/****************************************************************************
** Meta object code from reading C++ file 'post_install_dialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/post_install_dialog/post_install_dialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'post_install_dialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__PostInstallDialog_t {
    QByteArrayData data[6];
    char stringdata0[78];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__PostInstallDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__PostInstallDialog_t qt_meta_stringdata_mozc__gui__PostInstallDialog = {
    {
QT_MOC_LITERAL(0, 0, 28), // "mozc::gui::PostInstallDialog"
QT_MOC_LITERAL(1, 29, 4), // "OnOk"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 29), // "OnsetAsDefaultCheckBoxToggled"
QT_MOC_LITERAL(4, 65, 5), // "state"
QT_MOC_LITERAL(5, 71, 6) // "reject"

    },
    "mozc::gui::PostInstallDialog\0OnOk\0\0"
    "OnsetAsDefaultCheckBoxToggled\0state\0"
    "reject"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__PostInstallDialog[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   29,    2, 0x09 /* Protected */,
       3,    1,   30,    2, 0x09 /* Protected */,
       5,    0,   33,    2, 0x09 /* Protected */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void,

       0        // eod
};

void mozc::gui::PostInstallDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        PostInstallDialog *_t = static_cast<PostInstallDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->OnOk(); break;
        case 1: _t->OnsetAsDefaultCheckBoxToggled((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->reject(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::PostInstallDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_mozc__gui__PostInstallDialog.data,
      qt_meta_data_mozc__gui__PostInstallDialog,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::PostInstallDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::PostInstallDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__PostInstallDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int mozc::gui::PostInstallDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
