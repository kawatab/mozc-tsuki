/********************************************************************************
** Form generated from reading UI file 'keybinding_editor.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_KEYBINDING_EDITOR_H
#define UI_KEYBINDING_EDITOR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>

QT_BEGIN_NAMESPACE

class Ui_KeyBindingEditor
{
public:
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QLineEdit *KeyBindingLineEdit;
    QSpacerItem *verticalSpacer;
    QSpacerItem *verticalSpacer_2;
    QDialogButtonBox *KeyBindingEditorbuttonBox;
    QLabel *KeyBindingEditorLabel;
    QSpacerItem *horizontalSpacer;

    void setupUi(QDialog *KeyBindingEditor)
    {
        if (KeyBindingEditor->objectName().isEmpty())
            KeyBindingEditor->setObjectName(QStringLiteral("KeyBindingEditor"));
        KeyBindingEditor->resize(340, 100);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(KeyBindingEditor->sizePolicy().hasHeightForWidth());
        KeyBindingEditor->setSizePolicy(sizePolicy);
        KeyBindingEditor->setMinimumSize(QSize(340, 100));
        KeyBindingEditor->setMaximumSize(QSize(340, 100));
        gridLayout_2 = new QGridLayout(KeyBindingEditor);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        KeyBindingLineEdit = new QLineEdit(KeyBindingEditor);
        KeyBindingLineEdit->setObjectName(QStringLiteral("KeyBindingLineEdit"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(150);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(KeyBindingLineEdit->sizePolicy().hasHeightForWidth());
        KeyBindingLineEdit->setSizePolicy(sizePolicy1);
        KeyBindingLineEdit->setMinimumSize(QSize(150, 0));
        KeyBindingLineEdit->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout->addWidget(KeyBindingLineEdit, 1, 2, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(verticalSpacer, 2, 2, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(verticalSpacer_2, 0, 2, 1, 1);

        KeyBindingEditorbuttonBox = new QDialogButtonBox(KeyBindingEditor);
        KeyBindingEditorbuttonBox->setObjectName(QStringLiteral("KeyBindingEditorbuttonBox"));
        KeyBindingEditorbuttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        gridLayout->addWidget(KeyBindingEditorbuttonBox, 3, 0, 1, 3);

        KeyBindingEditorLabel = new QLabel(KeyBindingEditor);
        KeyBindingEditorLabel->setObjectName(QStringLiteral("KeyBindingEditorLabel"));

        gridLayout->addWidget(KeyBindingEditorLabel, 1, 0, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 1, 1, 1, 1);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);


        retranslateUi(KeyBindingEditor);

        QMetaObject::connectSlotsByName(KeyBindingEditor);
    } // setupUi

    void retranslateUi(QDialog *KeyBindingEditor)
    {
        KeyBindingEditor->setWindowTitle(QApplication::translate("KeyBindingEditor", "Mozc key binding editor", nullptr));
        KeyBindingEditorLabel->setText(QApplication::translate("KeyBindingEditor", "Input key assignments:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class KeyBindingEditor: public Ui_KeyBindingEditor {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_KEYBINDING_EDITOR_H
