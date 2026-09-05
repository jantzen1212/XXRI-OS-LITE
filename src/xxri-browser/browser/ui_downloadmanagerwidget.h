/********************************************************************************
** Form generated from reading UI file 'downloadmanagerwidget.ui'
**
** Created by: Qt User Interface Compiler version 5.15.10
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DOWNLOADMANAGERWIDGET_H
#define UI_DOWNLOADMANAGERWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DownloadManagerWidget
{
public:
    QVBoxLayout *m_topLevelLayout;
    QScrollArea *m_scrollArea;
    QWidget *m_items;
    QVBoxLayout *m_itemsLayout;
    QLabel *m_zeroItemsLabel;

    void setupUi(QWidget *DownloadManagerWidget)
    {
        if (DownloadManagerWidget->objectName().isEmpty())
            DownloadManagerWidget->setObjectName(QString::fromUtf8("DownloadManagerWidget"));
        DownloadManagerWidget->resize(400, 212);
        DownloadManagerWidget->setStyleSheet(QString::fromUtf8("#DownloadManagerWidget {\n"
"    background: palette(button)\n"
"}"));
        m_topLevelLayout = new QVBoxLayout(DownloadManagerWidget);
        m_topLevelLayout->setObjectName(QString::fromUtf8("m_topLevelLayout"));
        m_topLevelLayout->setSizeConstraint(QLayout::SetNoConstraint);
        m_topLevelLayout->setContentsMargins(0, 0, 0, 0);
        m_scrollArea = new QScrollArea(DownloadManagerWidget);
        m_scrollArea->setObjectName(QString::fromUtf8("m_scrollArea"));
        m_scrollArea->setStyleSheet(QString::fromUtf8("#m_scrollArea {\n"
"  margin: 2px;\n"
"  border: none;\n"
"}"));
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        m_items = new QWidget();
        m_items->setObjectName(QString::fromUtf8("m_items"));
        m_items->setStyleSheet(QString::fromUtf8("#m_items {background: palette(mid)}"));
        m_itemsLayout = new QVBoxLayout(m_items);
        m_itemsLayout->setSpacing(2);
        m_itemsLayout->setObjectName(QString::fromUtf8("m_itemsLayout"));
        m_itemsLayout->setContentsMargins(3, 3, 3, 3);
        m_zeroItemsLabel = new QLabel(m_items);
        m_zeroItemsLabel->setObjectName(QString::fromUtf8("m_zeroItemsLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(m_zeroItemsLabel->sizePolicy().hasHeightForWidth());
        m_zeroItemsLabel->setSizePolicy(sizePolicy);
        m_zeroItemsLabel->setStyleSheet(QString::fromUtf8("color: palette(shadow)"));
        m_zeroItemsLabel->setAlignment(Qt::AlignCenter);

        m_itemsLayout->addWidget(m_zeroItemsLabel);

        m_scrollArea->setWidget(m_items);

        m_topLevelLayout->addWidget(m_scrollArea);


        retranslateUi(DownloadManagerWidget);

        QMetaObject::connectSlotsByName(DownloadManagerWidget);
    } // setupUi

    void retranslateUi(QWidget *DownloadManagerWidget)
    {
        DownloadManagerWidget->setWindowTitle(QCoreApplication::translate("DownloadManagerWidget", "Downloads", nullptr));
        m_zeroItemsLabel->setText(QCoreApplication::translate("DownloadManagerWidget", "No downloads", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DownloadManagerWidget: public Ui_DownloadManagerWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DOWNLOADMANAGERWIDGET_H
