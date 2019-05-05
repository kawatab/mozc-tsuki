/********************************************************************************
** Form generated from reading UI file 'hand_writing.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HAND_WRITING_H
#define UI_HAND_WRITING_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>
#include "gui/character_pad/hand_writing_canvas.h"
#include "gui/character_pad/result_list.h"

QT_BEGIN_NAMESPACE

class Ui_HandWriting
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QFontComboBox *fontComboBox;
    QComboBox *sizeComboBox;
    HandWritingCanvas *handWritingCanvas;
    ResultList *resultListWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *clearButton;
    QPushButton *revertButton;
    QSpacerItem *verticalSpacer;
    QComboBox *handwritingSourceComboBox;

    void setupUi(QMainWindow *HandWriting)
    {
        if (HandWriting->objectName().isEmpty())
            HandWriting->setObjectName(QStringLiteral("HandWriting"));
        HandWriting->resize(500, 249);
        centralwidget = new QWidget(HandWriting);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        gridLayout_2 = new QGridLayout(centralwidget);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        gridLayout_2->setHorizontalSpacing(0);
        gridLayout_2->setContentsMargins(0, 2, 0, 0);
        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        gridLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 1, 1, 1);

        fontComboBox = new QFontComboBox(centralwidget);
        fontComboBox->setObjectName(QStringLiteral("fontComboBox"));

        gridLayout->addWidget(fontComboBox, 0, 2, 1, 1);

        sizeComboBox = new QComboBox(centralwidget);
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->setObjectName(QStringLiteral("sizeComboBox"));

        gridLayout->addWidget(sizeComboBox, 0, 3, 1, 1);

        handWritingCanvas = new HandWritingCanvas(centralwidget);
        handWritingCanvas->setObjectName(QStringLiteral("handWritingCanvas"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(handWritingCanvas->sizePolicy().hasHeightForWidth());
        handWritingCanvas->setSizePolicy(sizePolicy);
        handWritingCanvas->setMinimumSize(QSize(170, 170));
        handWritingCanvas->setMaximumSize(QSize(170, 170));

        gridLayout->addWidget(handWritingCanvas, 1, 0, 1, 1);

        resultListWidget = new ResultList(centralwidget);
        resultListWidget->setObjectName(QStringLiteral("resultListWidget"));

        gridLayout->addWidget(resultListWidget, 1, 1, 3, 3);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        clearButton = new QPushButton(centralwidget);
        clearButton->setObjectName(QStringLiteral("clearButton"));

        horizontalLayout->addWidget(clearButton);

        revertButton = new QPushButton(centralwidget);
        revertButton->setObjectName(QStringLiteral("revertButton"));

        horizontalLayout->addWidget(revertButton);


        gridLayout->addLayout(horizontalLayout, 2, 0, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(verticalSpacer, 3, 0, 1, 1);

        handwritingSourceComboBox = new QComboBox(centralwidget);
        handwritingSourceComboBox->addItem(QString());
        handwritingSourceComboBox->addItem(QString());
        handwritingSourceComboBox->setObjectName(QStringLiteral("handwritingSourceComboBox"));

        gridLayout->addWidget(handwritingSourceComboBox, 0, 0, 1, 1);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);

        HandWriting->setCentralWidget(centralwidget);

        retranslateUi(HandWriting);

        QMetaObject::connectSlotsByName(HandWriting);
    } // setupUi

    void retranslateUi(QMainWindow *HandWriting)
    {
        HandWriting->setWindowTitle(QApplication::translate("HandWriting", "Mozc Hand Writing", nullptr));
        sizeComboBox->setItemText(0, QApplication::translate("HandWriting", "Largest", nullptr));
        sizeComboBox->setItemText(1, QApplication::translate("HandWriting", "Larger", nullptr));
        sizeComboBox->setItemText(2, QApplication::translate("HandWriting", "Medium", nullptr));
        sizeComboBox->setItemText(3, QApplication::translate("HandWriting", "Smaller", nullptr));
        sizeComboBox->setItemText(4, QApplication::translate("HandWriting", "Smallest", nullptr));

        clearButton->setText(QApplication::translate("HandWriting", "clear", nullptr));
        revertButton->setText(QApplication::translate("HandWriting", "revert", nullptr));
        handwritingSourceComboBox->setItemText(0, QApplication::translate("HandWriting", "Local", nullptr));
        handwritingSourceComboBox->setItemText(1, QApplication::translate("HandWriting", "Cloud", nullptr));

    } // retranslateUi

};

namespace Ui {
    class HandWriting: public Ui_HandWriting {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HAND_WRITING_H
