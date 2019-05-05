/********************************************************************************
** Form generated from reading UI file 'import_dialog.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_IMPORT_DIALOG_H
#define UI_IMPORT_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ImportDialog
{
public:
    QGridLayout *gridLayout_2;
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer_2;
    QSpacerItem *horizontalSpacer_3;
    QLineEdit *dic_name_lineedit_;
    QLineEdit *file_name_lineedit_;
    QLabel *file_location_label_;
    QLabel *dic_name_label_;
    QLabel *ime_label_;
    QLabel *encoding_label_;
    QPushButton *select_file_pushbutton_;
    QComboBox *encoding_combobox_;
    QComboBox *ime_combobox_;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QDialogButtonBox *buttonbox_;

    void setupUi(QDialog *ImportDialog)
    {
        if (ImportDialog->objectName().isEmpty())
            ImportDialog->setObjectName(QStringLiteral("ImportDialog"));
        ImportDialog->resize(480, 210);
        ImportDialog->setMinimumSize(QSize(480, 210));
        ImportDialog->setMaximumSize(QSize(480, 210));
        ImportDialog->setModal(true);
        gridLayout_2 = new QGridLayout(ImportDialog);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 2, 2, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_3, 3, 2, 1, 1);

        dic_name_lineedit_ = new QLineEdit(ImportDialog);
        dic_name_lineedit_->setObjectName(QStringLiteral("dic_name_lineedit_"));
        dic_name_lineedit_->setMaxLength(32);

        gridLayout->addWidget(dic_name_lineedit_, 1, 1, 1, 2);

        file_name_lineedit_ = new QLineEdit(ImportDialog);
        file_name_lineedit_->setObjectName(QStringLiteral("file_name_lineedit_"));

        gridLayout->addWidget(file_name_lineedit_, 0, 1, 1, 2);

        file_location_label_ = new QLabel(ImportDialog);
        file_location_label_->setObjectName(QStringLiteral("file_location_label_"));

        gridLayout->addWidget(file_location_label_, 0, 0, 1, 1);

        dic_name_label_ = new QLabel(ImportDialog);
        dic_name_label_->setObjectName(QStringLiteral("dic_name_label_"));

        gridLayout->addWidget(dic_name_label_, 1, 0, 1, 1);

        ime_label_ = new QLabel(ImportDialog);
        ime_label_->setObjectName(QStringLiteral("ime_label_"));

        gridLayout->addWidget(ime_label_, 2, 0, 1, 1);

        encoding_label_ = new QLabel(ImportDialog);
        encoding_label_->setObjectName(QStringLiteral("encoding_label_"));

        gridLayout->addWidget(encoding_label_, 3, 0, 1, 1);

        select_file_pushbutton_ = new QPushButton(ImportDialog);
        select_file_pushbutton_->setObjectName(QStringLiteral("select_file_pushbutton_"));

        gridLayout->addWidget(select_file_pushbutton_, 0, 3, 1, 1);

        encoding_combobox_ = new QComboBox(ImportDialog);
        encoding_combobox_->setObjectName(QStringLiteral("encoding_combobox_"));
        encoding_combobox_->setMinimumSize(QSize(120, 0));

        gridLayout->addWidget(encoding_combobox_, 3, 1, 1, 1);

        ime_combobox_ = new QComboBox(ImportDialog);
        ime_combobox_->setObjectName(QStringLiteral("ime_combobox_"));
        ime_combobox_->setMinimumSize(QSize(120, 0));

        gridLayout->addWidget(ime_combobox_, 2, 1, 1, 1);


        verticalLayout->addLayout(gridLayout);

        verticalSpacer = new QSpacerItem(148, 13, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        buttonbox_ = new QDialogButtonBox(ImportDialog);
        buttonbox_->setObjectName(QStringLiteral("buttonbox_"));
        buttonbox_->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        horizontalLayout->addWidget(buttonbox_);


        verticalLayout->addLayout(horizontalLayout);


        gridLayout_2->addLayout(verticalLayout, 0, 0, 1, 1);

        QWidget::setTabOrder(file_name_lineedit_, select_file_pushbutton_);
        QWidget::setTabOrder(select_file_pushbutton_, dic_name_lineedit_);
        QWidget::setTabOrder(dic_name_lineedit_, ime_combobox_);
        QWidget::setTabOrder(ime_combobox_, encoding_combobox_);
        QWidget::setTabOrder(encoding_combobox_, buttonbox_);

        retranslateUi(ImportDialog);

        QMetaObject::connectSlotsByName(ImportDialog);
    } // setupUi

    void retranslateUi(QDialog *ImportDialog)
    {
        ImportDialog->setWindowTitle(QApplication::translate("ImportDialog", "Mozc Dictionary Tool", nullptr));
        file_location_label_->setText(QApplication::translate("ImportDialog", "File Location", nullptr));
        dic_name_label_->setText(QApplication::translate("ImportDialog", "Dictionary Name", nullptr));
        ime_label_->setText(QApplication::translate("ImportDialog", "Format", nullptr));
        encoding_label_->setText(QApplication::translate("ImportDialog", "Encoding", nullptr));
        select_file_pushbutton_->setText(QApplication::translate("ImportDialog", "Select file...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ImportDialog: public Ui_ImportDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_IMPORT_DIALOG_H
