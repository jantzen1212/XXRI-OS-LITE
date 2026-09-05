/********************************************************************************
** Form generated from reading UI file 'downloadwidget.ui'
**
** Created by: Qt User Interface Compiler version 5.15.10
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DOWNLOADWIDGET_H
#define UI_DOWNLOADWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DownloadWidget
{
public:
    QGridLayout *m_topLevelLayout;
    QLabel *m_dstName;
    QPushButton *m_cancelButton;
    QLabel *m_srcUrl;
    QProgressBar *m_progressBar;

    void setupUi(QFrame *DownloadWidget)
    {
        if (DownloadWidget->objectName().isEmpty())
            DownloadWidget->setObjectName(QString::fromUtf8("DownloadWidget"));
        DownloadWidget->setStyleSheet(QString::fromUtf8("#DownloadWidget {\n"
"  background: palette(button);\n"
"  border: 1px solid palette(dark);\n"
"  margin: 0px;\n"
"}"));
        m_topLevelLayout = new QGridLayout(DownloadWidget);
        m_topLevelLayout->setObjectName(QString::fromUtf8("m_topLevelLayout"));
        m_topLevelLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        m_dstName = new QLabel(DownloadWidget);
        m_dstName->setObjectName(QString::fromUtf8("m_dstName"));
        m_dstName->setStyleSheet(QString::fromUtf8("font-weight: bold\n"
""));

        m_topLevelLayout->addWidget(m_dstName, 0, 0, 1, 1);

        m_cancelButton = new QPushButton(DownloadWidget);
        m_cancelButton->setObjectName(QString::fromUtf8("m_cancelButton"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(m_cancelButton->sizePolicy().hasHeightForWidth());
        m_cancelButton->setSizePolicy(sizePolicy);
        m_cancelButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"  margin: 1px;\n"
"  border: none;\n"
"}\n"
"QPushButton:pressed {\n"
"  margin: none;\n"
"  border: 1px solid palette(shadow);\n"
"  background: palette(midlight);\n"
"}"));
        m_cancelButton->setFlat(false);

        m_topLevelLayout->addWidget(m_cancelButton, 0, 1, 1, 1);

        m_srcUrl = new QLabel(DownloadWidget);
        m_srcUrl->setObjectName(QString::fromUtf8("m_srcUrl"));
        m_srcUrl->setMaximumSize(QSize(350, 16777215));
        m_srcUrl->setStyleSheet(QString::fromUtf8(""));

        m_topLevelLayout->addWidget(m_srcUrl, 1, 0, 1, 2);

        m_progressBar = new QProgressBar(DownloadWidget);
        m_progressBar->setObjectName(QString::fromUtf8("m_progressBar"));
        m_progressBar->setStyleSheet(QString::fromUtf8("font-size: 12px"));
        m_progressBar->setValue(24);

        m_topLevelLayout->addWidget(m_progressBar, 2, 0, 1, 2);


        retranslateUi(DownloadWidget);

        QMetaObject::connectSlotsByName(DownloadWidget);
    } // setupUi

    void retranslateUi(QFrame *DownloadWidget)
    {
        m_dstName->setText(QCoreApplication::translate("DownloadWidget", "TextLabel", nullptr));
        m_srcUrl->setText(QCoreApplication::translate("DownloadWidget", "TextLabel", nullptr));
        (void)DownloadWidget;
    } // retranslateUi

};

namespace Ui {
    class DownloadWidget: public Ui_DownloadWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DOWNLOADWIDGET_H
