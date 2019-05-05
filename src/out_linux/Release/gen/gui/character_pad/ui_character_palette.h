/********************************************************************************
** Form generated from reading UI file 'character_palette.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CHARACTER_PALETTE_H
#define UI_CHARACTER_PALETTE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QWidget>
#include "gui/character_pad/character_palette_table_widget.h"
#include "gui/dictionary_tool/zero_width_splitter.h"

QT_BEGIN_NAMESPACE

class Ui_CharacterPalette
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QFontComboBox *fontComboBox;
    QComboBox *sizeComboBox;
    ZeroWidthSplitter *splitter;
    QTreeWidget *categoryTreeWidget;
    CharacterPaletteTableWidget *tableWidget;

    void setupUi(QMainWindow *CharacterPalette)
    {
        if (CharacterPalette->objectName().isEmpty())
            CharacterPalette->setObjectName(QStringLiteral("CharacterPalette"));
        CharacterPalette->resize(670, 250);
        centralwidget = new QWidget(CharacterPalette);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        gridLayout_2 = new QGridLayout(centralwidget);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        gridLayout_2->setContentsMargins(0, 2, 0, 0);
        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        gridLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 0, 1, 1);

        fontComboBox = new QFontComboBox(centralwidget);
        fontComboBox->setObjectName(QStringLiteral("fontComboBox"));

        gridLayout->addWidget(fontComboBox, 0, 1, 1, 1);

        sizeComboBox = new QComboBox(centralwidget);
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->addItem(QString());
        sizeComboBox->setObjectName(QStringLiteral("sizeComboBox"));
        sizeComboBox->setMinimumSize(QSize(70, 0));

        gridLayout->addWidget(sizeComboBox, 0, 2, 1, 1);

        splitter = new ZeroWidthSplitter(centralwidget);
        splitter->setObjectName(QStringLiteral("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        categoryTreeWidget = new QTreeWidget(splitter);
        categoryTreeWidget->headerItem()->setText(0, QString());
        categoryTreeWidget->setObjectName(QStringLiteral("categoryTreeWidget"));
        categoryTreeWidget->setMaximumSize(QSize(16777215, 16777215));
        categoryTreeWidget->setBaseSize(QSize(300, 0));
        categoryTreeWidget->setIndentation(10);
        categoryTreeWidget->setUniformRowHeights(false);
        categoryTreeWidget->setAnimated(true);
        categoryTreeWidget->setHeaderHidden(true);
        splitter->addWidget(categoryTreeWidget);
        tableWidget = new CharacterPaletteTableWidget(splitter);
        tableWidget->setObjectName(QStringLiteral("tableWidget"));
        splitter->addWidget(tableWidget);

        gridLayout->addWidget(splitter, 1, 0, 1, 3);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);

        CharacterPalette->setCentralWidget(centralwidget);

        retranslateUi(CharacterPalette);

        QMetaObject::connectSlotsByName(CharacterPalette);
    } // setupUi

    void retranslateUi(QMainWindow *CharacterPalette)
    {
        CharacterPalette->setWindowTitle(QApplication::translate("CharacterPalette", "Mozc Character Palette", nullptr));
        sizeComboBox->setItemText(0, QApplication::translate("CharacterPalette", "Largest", nullptr));
        sizeComboBox->setItemText(1, QApplication::translate("CharacterPalette", "Larger", nullptr));
        sizeComboBox->setItemText(2, QApplication::translate("CharacterPalette", "Medium", nullptr));
        sizeComboBox->setItemText(3, QApplication::translate("CharacterPalette", "Smaller", nullptr));
        sizeComboBox->setItemText(4, QApplication::translate("CharacterPalette", "Smallest", nullptr));

    } // retranslateUi

};

namespace Ui {
    class CharacterPalette: public Ui_CharacterPalette {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CHARACTER_PALETTE_H
