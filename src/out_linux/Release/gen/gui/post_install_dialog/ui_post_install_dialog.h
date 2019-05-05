/********************************************************************************
** Form generated from reading UI file 'post_install_dialog.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POST_INSTALL_DIALOG_H
#define UI_POST_INSTALL_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

QT_BEGIN_NAMESPACE

class Ui_PostInstallDialog
{
public:
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *okButton;
    QCheckBox *migrateDefaultIMEUserDictionaryCheckBox;
    QCheckBox *setAsDefaultCheckBox;
    QCheckBox *IMEHotKeyDisabledCheckBox;

    void setupUi(QDialog *PostInstallDialog)
    {
        if (PostInstallDialog->objectName().isEmpty())
            PostInstallDialog->setObjectName(QStringLiteral("PostInstallDialog"));
        PostInstallDialog->resize(470, 180);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(PostInstallDialog->sizePolicy().hasHeightForWidth());
        PostInstallDialog->setSizePolicy(sizePolicy);
        PostInstallDialog->setMinimumSize(QSize(470, 180));
        PostInstallDialog->setMaximumSize(QSize(470, 180));
        gridLayout_2 = new QGridLayout(PostInstallDialog);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        gridLayout->setHorizontalSpacing(6);
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 6, 0, 1, 1);

        okButton = new QPushButton(PostInstallDialog);
        okButton->setObjectName(QStringLiteral("okButton"));

        gridLayout->addWidget(okButton, 6, 3, 1, 1);

        migrateDefaultIMEUserDictionaryCheckBox = new QCheckBox(PostInstallDialog);
        migrateDefaultIMEUserDictionaryCheckBox->setObjectName(QStringLiteral("migrateDefaultIMEUserDictionaryCheckBox"));
        migrateDefaultIMEUserDictionaryCheckBox->setChecked(true);

        gridLayout->addWidget(migrateDefaultIMEUserDictionaryCheckBox, 5, 0, 1, 4);

        setAsDefaultCheckBox = new QCheckBox(PostInstallDialog);
        setAsDefaultCheckBox->setObjectName(QStringLiteral("setAsDefaultCheckBox"));
        setAsDefaultCheckBox->setChecked(true);

        gridLayout->addWidget(setAsDefaultCheckBox, 3, 0, 1, 4);

        IMEHotKeyDisabledCheckBox = new QCheckBox(PostInstallDialog);
        IMEHotKeyDisabledCheckBox->setObjectName(QStringLiteral("IMEHotKeyDisabledCheckBox"));
        IMEHotKeyDisabledCheckBox->setChecked(true);

        gridLayout->addWidget(IMEHotKeyDisabledCheckBox, 4, 0, 1, 4);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);

        QWidget::setTabOrder(setAsDefaultCheckBox, IMEHotKeyDisabledCheckBox);
        QWidget::setTabOrder(IMEHotKeyDisabledCheckBox, migrateDefaultIMEUserDictionaryCheckBox);
        QWidget::setTabOrder(migrateDefaultIMEUserDictionaryCheckBox, okButton);

        retranslateUi(PostInstallDialog);

        QMetaObject::connectSlotsByName(PostInstallDialog);
    } // setupUi

    void retranslateUi(QDialog *PostInstallDialog)
    {
        PostInstallDialog->setWindowTitle(QApplication::translate("PostInstallDialog", "Mozc", nullptr));
        okButton->setText(QApplication::translate("PostInstallDialog", "&OK", nullptr));
        migrateDefaultIMEUserDictionaryCheckBox->setText(QApplication::translate("PostInstallDialog", "Migrate MSIME's user dictionary to Mozc", nullptr));
        setAsDefaultCheckBox->setText(QApplication::translate("PostInstallDialog", "Set Mozc as the default IME", nullptr));
        IMEHotKeyDisabledCheckBox->setText(QApplication::translate("PostInstallDialog", "Disable Keyboard layout hotkey (Ctrl+Shift)", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PostInstallDialog: public Ui_PostInstallDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POST_INSTALL_DIALOG_H
