/********************************************************************************
** Form generated from reading UI file 'certificateerrordialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.10
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CERTIFICATEERRORDIALOG_H
#define UI_CERTIFICATEERRORDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_CertificateErrorDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *m_iconLabel;
    QLabel *m_errorLabel;
    QLabel *m_infoLabel;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CertificateErrorDialog)
    {
        if (CertificateErrorDialog->objectName().isEmpty())
            CertificateErrorDialog->setObjectName(QString::fromUtf8("CertificateErrorDialog"));
        CertificateErrorDialog->resize(370, 141);
        verticalLayout = new QVBoxLayout(CertificateErrorDialog);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(20, -1, 20, -1);
        m_iconLabel = new QLabel(CertificateErrorDialog);
        m_iconLabel->setObjectName(QString::fromUtf8("m_iconLabel"));
        m_iconLabel->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(m_iconLabel);

        m_errorLabel = new QLabel(CertificateErrorDialog);
        m_errorLabel->setObjectName(QString::fromUtf8("m_errorLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(m_errorLabel->sizePolicy().hasHeightForWidth());
        m_errorLabel->setSizePolicy(sizePolicy);
        m_errorLabel->setAlignment(Qt::AlignCenter);
        m_errorLabel->setWordWrap(true);

        verticalLayout->addWidget(m_errorLabel);

        m_infoLabel = new QLabel(CertificateErrorDialog);
        m_infoLabel->setObjectName(QString::fromUtf8("m_infoLabel"));
        QSizePolicy sizePolicy1(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(m_infoLabel->sizePolicy().hasHeightForWidth());
        m_infoLabel->setSizePolicy(sizePolicy1);
        m_infoLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        m_infoLabel->setWordWrap(true);

        verticalLayout->addWidget(m_infoLabel);

        verticalSpacer = new QSpacerItem(20, 16, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        buttonBox = new QDialogButtonBox(CertificateErrorDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(CertificateErrorDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), CertificateErrorDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), CertificateErrorDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(CertificateErrorDialog);
    } // setupUi

    void retranslateUi(QDialog *CertificateErrorDialog)
    {
        CertificateErrorDialog->setWindowTitle(QCoreApplication::translate("CertificateErrorDialog", "Dialog", nullptr));
        m_iconLabel->setText(QCoreApplication::translate("CertificateErrorDialog", "Icon", nullptr));
        m_errorLabel->setText(QCoreApplication::translate("CertificateErrorDialog", "Error", nullptr));
        m_infoLabel->setText(QCoreApplication::translate("CertificateErrorDialog", "If you wish so, you may continue with an unverified certificate. Accepting an unverified certificate mean you may not be connected with the host you tried to connect to.\n"
"\n"
"Do you wish to override the security check and continue ?   ", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CertificateErrorDialog: public Ui_CertificateErrorDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CERTIFICATEERRORDIALOG_H
