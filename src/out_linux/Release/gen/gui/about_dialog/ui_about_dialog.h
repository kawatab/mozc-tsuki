/********************************************************************************
** Form generated from reading UI file 'about_dialog.ui'
**
** Created by: Qt User Interface Compiler version 5.11.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUT_DIALOG_H
#define UI_ABOUT_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AboutDialog
{
public:
    QFrame *color_frame;
    QPushButton *pushButton;
    QWidget *gridLayoutWidget_2;
    QGridLayout *gridLayout_3;
    QLabel *label_6;
    QLabel *label_credits;
    QSpacerItem *horizontalSpacer_4;
    QSpacerItem *horizontalSpacer_3;
    QLabel *label_terms;
    QSpacerItem *horizontalSpacer_5;
    QWidget *gridLayoutWidget;
    QGridLayout *gridLayout;
    QLabel *label;
    QSpacerItem *horizontalSpacer_2;
    QLabel *version_label;
    QSpacerItem *horizontalSpacer;

    void setupUi(QDialog *AboutDialog)
    {
        if (AboutDialog->objectName().isEmpty())
            AboutDialog->setObjectName(QStringLiteral("AboutDialog"));
        AboutDialog->resize(490, 250);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(AboutDialog->sizePolicy().hasHeightForWidth());
        AboutDialog->setSizePolicy(sizePolicy);
        AboutDialog->setMinimumSize(QSize(490, 250));
        AboutDialog->setMaximumSize(QSize(490, 250));
        color_frame = new QFrame(AboutDialog);
        color_frame->setObjectName(QStringLiteral("color_frame"));
        color_frame->setGeometry(QRect(-10, 120, 511, 161));
        color_frame->setFrameShape(QFrame::StyledPanel);
        color_frame->setFrameShadow(QFrame::Raised);
        pushButton = new QPushButton(color_frame);
        pushButton->setObjectName(QStringLiteral("pushButton"));
        pushButton->setGeometry(QRect(400, 90, 91, 32));
        gridLayoutWidget_2 = new QWidget(color_frame);
        gridLayoutWidget_2->setObjectName(QStringLiteral("gridLayoutWidget_2"));
        gridLayoutWidget_2->setGeometry(QRect(20, 10, 471, 81));
        gridLayout_3 = new QGridLayout(gridLayoutWidget_2);
        gridLayout_3->setObjectName(QStringLiteral("gridLayout_3"));
        gridLayout_3->setContentsMargins(0, 0, 0, 0);
        label_6 = new QLabel(gridLayoutWidget_2);
        label_6->setObjectName(QStringLiteral("label_6"));
        QFont font;
        font.setPointSize(10);
        label_6->setFont(font);
        label_6->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout_3->addWidget(label_6, 0, 0, 1, 1);

        label_credits = new QLabel(gridLayoutWidget_2);
        label_credits->setObjectName(QStringLiteral("label_credits"));
        label_credits->setFont(font);

        gridLayout_3->addWidget(label_credits, 1, 0, 1, 1);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_4, 1, 1, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_3, 0, 1, 1, 1);

        label_terms = new QLabel(gridLayoutWidget_2);
        label_terms->setObjectName(QStringLiteral("label_terms"));
        label_terms->setFont(font);

        gridLayout_3->addWidget(label_terms, 2, 0, 1, 1);

        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_5, 2, 1, 1, 1);

        gridLayoutWidget = new QWidget(AboutDialog);
        gridLayoutWidget->setObjectName(QStringLiteral("gridLayoutWidget"));
        gridLayoutWidget->setGeometry(QRect(10, 20, 471, 99));
        gridLayout = new QGridLayout(gridLayoutWidget);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(gridLayoutWidget);
        label->setObjectName(QStringLiteral("label"));
        QFont font1;
        font1.setFamily(QString::fromUtf8("\343\203\241\343\202\244\343\203\252\343\202\252"));
        font1.setPointSize(24);
        font1.setBold(false);
        font1.setWeight(50);
        label->setFont(font1);
        label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout->addWidget(label, 0, 0, 1, 2);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 0, 2, 1, 1);

        version_label = new QLabel(gridLayoutWidget);
        version_label->setObjectName(QStringLiteral("version_label"));
        version_label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);

        gridLayout->addWidget(version_label, 1, 0, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 1, 1, 1, 2);


        retranslateUi(AboutDialog);
        QObject::connect(pushButton, SIGNAL(clicked()), AboutDialog, SLOT(accept()));
        QObject::connect(label_credits, SIGNAL(linkActivated(QString)), AboutDialog, SLOT(linkActivated(QString)));
        QObject::connect(label_terms, SIGNAL(linkActivated(QString)), AboutDialog, SLOT(linkActivated(QString)));

        QMetaObject::connectSlotsByName(AboutDialog);
    } // setupUi

    void retranslateUi(QDialog *AboutDialog)
    {
        AboutDialog->setWindowTitle(QApplication::translate("AboutDialog", "About Mozc", nullptr));
        pushButton->setText(QApplication::translate("AboutDialog", "OK", nullptr));
        label_6->setText(QApplication::translate("AboutDialog", "Copyright \302\251 2018 Google Inc. All rights reserved.", nullptr));
        label_credits->setText(QApplication::translate("AboutDialog", "<html><body>Mozc is made possible by <a href=\"file://credits_en.html\"><span style=\" text-decoration: underline; color:#0000ff;\">open source software</span></a>.</body></html>", nullptr));
        label_terms->setText(QApplication::translate("AboutDialog", "<html><body>Mozc <a href=\"https://github.com/google/mozc\"><span style=\" text-decoration: underline; color:#0000ff;\">product information</span></a> <a href=\"https://github.com/google/mozc/issues\"><span style=\" text-decoration: underline; color:#0000ff;\">issues</span></a></body></html>", nullptr));
        label->setText(QApplication::translate("AboutDialog", "Mozc", nullptr));
        version_label->setText(QApplication::translate("AboutDialog", "0.0.0.0", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AboutDialog: public Ui_AboutDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUT_DIALOG_H
