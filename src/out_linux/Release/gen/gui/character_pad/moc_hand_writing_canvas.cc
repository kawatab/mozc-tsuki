/****************************************************************************
** Meta object code from reading C++ file 'hand_writing_canvas.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.11.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "gui/character_pad/hand_writing_canvas.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'hand_writing_canvas.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.11.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_mozc__gui__HandWritingCanvas_t {
    QByteArrayData data[11];
    char stringdata0[163];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_mozc__gui__HandWritingCanvas_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_mozc__gui__HandWritingCanvas_t qt_meta_stringdata_mozc__gui__HandWritingCanvas = {
    {
QT_MOC_LITERAL(0, 0, 28), // "mozc::gui::HandWritingCanvas"
QT_MOC_LITERAL(1, 29, 16), // "startRecognition"
QT_MOC_LITERAL(2, 46, 0), // ""
QT_MOC_LITERAL(3, 47, 13), // "canvasUpdated"
QT_MOC_LITERAL(4, 61, 5), // "clear"
QT_MOC_LITERAL(5, 67, 6), // "revert"
QT_MOC_LITERAL(6, 74, 11), // "listUpdated"
QT_MOC_LITERAL(7, 86, 18), // "restartRecognition"
QT_MOC_LITERAL(8, 105, 13), // "statusUpdated"
QT_MOC_LITERAL(9, 119, 36), // "mozc::handwriting::Handwritin..."
QT_MOC_LITERAL(10, 156, 6) // "status"

    },
    "mozc::gui::HandWritingCanvas\0"
    "startRecognition\0\0canvasUpdated\0clear\0"
    "revert\0listUpdated\0restartRecognition\0"
    "statusUpdated\0mozc::handwriting::HandwritingStatus\0"
    "status"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_mozc__gui__HandWritingCanvas[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x06 /* Public */,
       3,    0,   50,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    0,   51,    2, 0x0a /* Public */,
       5,    0,   52,    2, 0x0a /* Public */,
       6,    0,   53,    2, 0x0a /* Public */,
       7,    0,   54,    2, 0x0a /* Public */,
       8,    1,   55,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 9,   10,

       0        // eod
};

void mozc::gui::HandWritingCanvas::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        HandWritingCanvas *_t = static_cast<HandWritingCanvas *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->startRecognition(); break;
        case 1: _t->canvasUpdated(); break;
        case 2: _t->clear(); break;
        case 3: _t->revert(); break;
        case 4: _t->listUpdated(); break;
        case 5: _t->restartRecognition(); break;
        case 6: _t->statusUpdated((*reinterpret_cast< mozc::handwriting::HandwritingStatus(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (HandWritingCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&HandWritingCanvas::startRecognition)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (HandWritingCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&HandWritingCanvas::canvasUpdated)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject mozc::gui::HandWritingCanvas::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_mozc__gui__HandWritingCanvas.data,
      qt_meta_data_mozc__gui__HandWritingCanvas,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *mozc::gui::HandWritingCanvas::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *mozc::gui::HandWritingCanvas::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_mozc__gui__HandWritingCanvas.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int mozc::gui::HandWritingCanvas::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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

// SIGNAL 0
void mozc::gui::HandWritingCanvas::startRecognition()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void mozc::gui::HandWritingCanvas::canvasUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
