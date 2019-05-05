/****************************************************************************
** Meta object code from reading C++ file 'hand_writing_thread.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/character_pad/hand_writing_thread.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'hand_writing_thread.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__HandWritingThread_t {
    QByteArrayData data[10];
    char stringdata0[164];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__HandWritingThread_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__HandWritingThread_t qt_meta_stringdata_mozc__gui__HandWritingThread = {
    {
QT_MOC_LITERAL(0, 0, 28), // "mozc::gui::HandWritingThread"
QT_MOC_LITERAL(1, 29, 17), // "candidatesUpdated"
QT_MOC_LITERAL(2, 47, 0), // ""
QT_MOC_LITERAL(3, 48, 13), // "statusUpdated"
QT_MOC_LITERAL(4, 62, 36), // "mozc::handwriting::Handwritin..."
QT_MOC_LITERAL(5, 99, 6), // "status"
QT_MOC_LITERAL(6, 106, 16), // "startRecognition"
QT_MOC_LITERAL(7, 123, 12), // "itemSelected"
QT_MOC_LITERAL(8, 136, 22), // "const QListWidgetItem*"
QT_MOC_LITERAL(9, 159, 4) // "item"

    },
    "mozc::gui::HandWritingThread\0"
    "candidatesUpdated\0\0statusUpdated\0"
    "mozc::handwriting::HandwritingStatus\0"
    "status\0startRecognition\0itemSelected\0"
    "const QListWidgetItem*\0item"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__HandWritingThread[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   34,    2, 0x06 /* Public */,
       3,    1,   35,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       6,    0,   38,    2, 0x0a /* Public */,
       7,    1,   39,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8,    9,

       0        // eod
};

void mozc::gui::HandWritingThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        HandWritingThread *_t = static_cast<HandWritingThread *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->candidatesUpdated(); break;
        case 1: _t->statusUpdated((*reinterpret_cast< mozc::handwriting::HandwritingStatus(*)>(_a[1]))); break;
        case 2: _t->startRecognition(); break;
        case 3: _t->itemSelected((*reinterpret_cast< const QListWidgetItem*(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (HandWritingThread::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&HandWritingThread::candidatesUpdated)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (HandWritingThread::*)(mozc::handwriting::HandwritingStatus );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&HandWritingThread::statusUpdated)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::HandWritingThread::staticMetaObject = {
    { &QThread::staticMetaObject, qt_meta_stringdata_mozc__gui__HandWritingThread.data,
      qt_meta_data_mozc__gui__HandWritingThread,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::HandWritingThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::HandWritingThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__HandWritingThread.stringdata0))
        return static_cast<void*>(this);
    return QThread::qt_metacast(_clname);
}

int mozc::gui::HandWritingThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
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
void mozc::gui::HandWritingThread::candidatesUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void mozc::gui::HandWritingThread::statusUpdated(mozc::handwriting::HandwritingStatus _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
