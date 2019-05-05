/****************************************************************************
** Meta object code from reading C++ file 'config_dialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/config_dialog/config_dialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'config_dialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__ConfigDialog_t {
    QByteArrayData data[19];
    char stringdata0[313];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__ConfigDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__ConfigDialog_t qt_meta_stringdata_mozc__gui__ConfigDialog = {
    {
QT_MOC_LITERAL(0, 0, 23), // "mozc::gui::ConfigDialog"
QT_MOC_LITERAL(1, 24, 7), // "clicked"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 16), // "QAbstractButton*"
QT_MOC_LITERAL(4, 50, 6), // "button"
QT_MOC_LITERAL(5, 57, 16), // "ClearUserHistory"
QT_MOC_LITERAL(6, 74, 19), // "ClearUserPrediction"
QT_MOC_LITERAL(7, 94, 25), // "ClearUnusedUserPrediction"
QT_MOC_LITERAL(8, 120, 18), // "EditUserDictionary"
QT_MOC_LITERAL(9, 139, 10), // "EditKeymap"
QT_MOC_LITERAL(10, 150, 14), // "EditRomanTable"
QT_MOC_LITERAL(11, 165, 15), // "ResetToDefaults"
QT_MOC_LITERAL(12, 181, 22), // "SelectInputModeSetting"
QT_MOC_LITERAL(13, 204, 5), // "index"
QT_MOC_LITERAL(14, 210, 27), // "SelectAutoConversionSetting"
QT_MOC_LITERAL(15, 238, 5), // "state"
QT_MOC_LITERAL(16, 244, 23), // "SelectSuggestionSetting"
QT_MOC_LITERAL(17, 268, 26), // "LaunchAdministrationDialog"
QT_MOC_LITERAL(18, 295, 17) // "EnableApplyButton"

    },
    "mozc::gui::ConfigDialog\0clicked\0\0"
    "QAbstractButton*\0button\0ClearUserHistory\0"
    "ClearUserPrediction\0ClearUnusedUserPrediction\0"
    "EditUserDictionary\0EditKeymap\0"
    "EditRomanTable\0ResetToDefaults\0"
    "SelectInputModeSetting\0index\0"
    "SelectAutoConversionSetting\0state\0"
    "SelectSuggestionSetting\0"
    "LaunchAdministrationDialog\0EnableApplyButton"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__ConfigDialog[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   79,    2, 0x09 /* Protected */,
       5,    0,   82,    2, 0x09 /* Protected */,
       6,    0,   83,    2, 0x09 /* Protected */,
       7,    0,   84,    2, 0x09 /* Protected */,
       8,    0,   85,    2, 0x09 /* Protected */,
       9,    0,   86,    2, 0x09 /* Protected */,
      10,    0,   87,    2, 0x09 /* Protected */,
      11,    0,   88,    2, 0x09 /* Protected */,
      12,    1,   89,    2, 0x09 /* Protected */,
      14,    1,   92,    2, 0x09 /* Protected */,
      16,    1,   95,    2, 0x09 /* Protected */,
      17,    0,   98,    2, 0x09 /* Protected */,
      18,    0,   99,    2, 0x09 /* Protected */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   13,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void mozc::gui::ConfigDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        ConfigDialog *_t = static_cast<ConfigDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->clicked((*reinterpret_cast< QAbstractButton*(*)>(_a[1]))); break;
        case 1: _t->ClearUserHistory(); break;
        case 2: _t->ClearUserPrediction(); break;
        case 3: _t->ClearUnusedUserPrediction(); break;
        case 4: _t->EditUserDictionary(); break;
        case 5: _t->EditKeymap(); break;
        case 6: _t->EditRomanTable(); break;
        case 7: _t->ResetToDefaults(); break;
        case 8: _t->SelectInputModeSetting((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 9: _t->SelectAutoConversionSetting((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 10: _t->SelectSuggestionSetting((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 11: _t->LaunchAdministrationDialog(); break;
        case 12: _t->EnableApplyButton(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::ConfigDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_mozc__gui__ConfigDialog.data,
      qt_meta_data_mozc__gui__ConfigDialog,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::ConfigDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::ConfigDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__ConfigDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int mozc::gui::ConfigDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
