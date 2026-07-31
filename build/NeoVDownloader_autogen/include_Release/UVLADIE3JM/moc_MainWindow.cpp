/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/MainWindow.h"
#include <QtGui/qtextcursor.h>
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSMainWindowENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSMainWindowENDCLASS = QtMocHelpers::stringData(
    "MainWindow",
    "onStartClicked",
    "",
    "onCancelClicked",
    "onBrowseClicked",
    "onOpenFolderClicked",
    "switchPage",
    "index",
    "refreshLibrary",
    "onPlaySelectedMedia",
    "onLibraryDoubleClicked",
    "row",
    "column",
    "onConvertBrowseClicked",
    "onStartConvertClicked",
    "onCancelConvertClicked",
    "onConvertProcessOutput",
    "onConvertProcessFinished",
    "exitCode",
    "checkForUpdates",
    "silent",
    "onUpdateReplyFinished",
    "QNetworkReply*",
    "reply",
    "updateYtdlpEngine",
    "onUpdateDownloadFinished",
    "fileName"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSMainWindowENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      18,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  122,    2, 0x08,    1 /* Private */,
       3,    0,  123,    2, 0x08,    2 /* Private */,
       4,    0,  124,    2, 0x08,    3 /* Private */,
       5,    0,  125,    2, 0x08,    4 /* Private */,
       6,    1,  126,    2, 0x08,    5 /* Private */,
       8,    0,  129,    2, 0x08,    7 /* Private */,
       9,    0,  130,    2, 0x08,    8 /* Private */,
      10,    2,  131,    2, 0x08,    9 /* Private */,
      13,    0,  136,    2, 0x08,   12 /* Private */,
      14,    0,  137,    2, 0x08,   13 /* Private */,
      15,    0,  138,    2, 0x08,   14 /* Private */,
      16,    0,  139,    2, 0x08,   15 /* Private */,
      17,    1,  140,    2, 0x08,   16 /* Private */,
      19,    1,  143,    2, 0x08,   18 /* Private */,
      19,    0,  146,    2, 0x28,   20 /* Private | MethodCloned */,
      21,    2,  147,    2, 0x08,   21 /* Private */,
      24,    0,  152,    2, 0x08,   24 /* Private */,
      25,    2,  153,    2, 0x08,   25 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   11,   12,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void, QMetaType::Bool,   20,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 22, QMetaType::Bool,   23,   20,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 22, QMetaType::QString,   23,   26,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_CLASSMainWindowENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSMainWindowENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSMainWindowENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'onStartClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCancelClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBrowseClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOpenFolderClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'switchPage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'refreshLibrary'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPlaySelectedMedia'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLibraryDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onConvertBrowseClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onStartConvertClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCancelConvertClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onConvertProcessOutput'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onConvertProcessFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'checkForUpdates'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'checkForUpdates'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onUpdateReplyFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QNetworkReply *, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'updateYtdlpEngine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onUpdateDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QNetworkReply *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onStartClicked(); break;
        case 1: _t->onCancelClicked(); break;
        case 2: _t->onBrowseClicked(); break;
        case 3: _t->onOpenFolderClicked(); break;
        case 4: _t->switchPage((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->refreshLibrary(); break;
        case 6: _t->onPlaySelectedMedia(); break;
        case 7: _t->onLibraryDoubleClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->onConvertBrowseClicked(); break;
        case 9: _t->onStartConvertClicked(); break;
        case 10: _t->onCancelConvertClicked(); break;
        case 11: _t->onConvertProcessOutput(); break;
        case 12: _t->onConvertProcessFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->checkForUpdates((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->checkForUpdates(); break;
        case 15: _t->onUpdateReplyFinished((*reinterpret_cast< std::add_pointer_t<QNetworkReply*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 16: _t->updateYtdlpEngine(); break;
        case 17: _t->onUpdateDownloadFinished((*reinterpret_cast< std::add_pointer_t<QNetworkReply*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 15:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QNetworkReply* >(); break;
            }
            break;
        case 17:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QNetworkReply* >(); break;
            }
            break;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSMainWindowENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    return _id;
}
QT_WARNING_POP
