/********************************************************************************
** Form generated from reading UI file 'dictionary_tool.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DICTIONARY_TOOL_H
#define UI_DICTIONARY_TOOL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include "gui/dictionary_tool/dictionary_content_table_widget.h"
#include "gui/dictionary_tool/zero_width_splitter.h"

QT_BEGIN_NAMESPACE

class Ui_DictionaryTool
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout;
    ZeroWidthSplitter *splitter_;
    QListWidget *dic_list_;
    DictionaryContentTableWidget *dic_content_;
    QToolBar *toolbar_;
    QStatusBar *statusbar_;

    void setupUi(QMainWindow *DictionaryTool)
    {
        if (DictionaryTool->objectName().isEmpty())
            DictionaryTool->setObjectName(QStringLiteral("DictionaryTool"));
        DictionaryTool->resize(700, 420);
        centralwidget = new QWidget(DictionaryTool);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        gridLayout = new QGridLayout(centralwidget);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        splitter_ = new ZeroWidthSplitter(centralwidget);
        splitter_->setObjectName(QStringLiteral("splitter_"));
        splitter_->setOrientation(Qt::Horizontal);
        dic_list_ = new QListWidget(splitter_);
        dic_list_->setObjectName(QStringLiteral("dic_list_"));
        splitter_->addWidget(dic_list_);
        dic_content_ = new DictionaryContentTableWidget(splitter_);
        dic_content_->setObjectName(QStringLiteral("dic_content_"));
        splitter_->addWidget(dic_content_);

        gridLayout->addWidget(splitter_, 0, 0, 1, 1);

        DictionaryTool->setCentralWidget(centralwidget);
        toolbar_ = new QToolBar(DictionaryTool);
        toolbar_->setObjectName(QStringLiteral("toolbar_"));
        DictionaryTool->addToolBar(Qt::TopToolBarArea, toolbar_);
        statusbar_ = new QStatusBar(DictionaryTool);
        statusbar_->setObjectName(QStringLiteral("statusbar_"));
        DictionaryTool->setStatusBar(statusbar_);

        retranslateUi(DictionaryTool);

        QMetaObject::connectSlotsByName(DictionaryTool);
    } // setupUi

    void retranslateUi(QMainWindow *DictionaryTool)
    {
        DictionaryTool->setWindowTitle(QApplication::translate("DictionaryTool", "Mozc Dictionary Tool", nullptr));
        toolbar_->setWindowTitle(QApplication::translate("DictionaryTool", "toolBar", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DictionaryTool: public Ui_DictionaryTool {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DICTIONARY_TOOL_H
