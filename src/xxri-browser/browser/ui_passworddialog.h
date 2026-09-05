/********************************************************************************
** Form generated from reading UI file 'passworddialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.10
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PASSWORDDIALOG_H
#define UI_PASSWORDDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

QT_BEGIN_NAMESPACE

class Ui_PasswordDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *m_iconLabel;
    QLabel *m_infoLabel;
    QLabel *userLabel;
    QLineEdit *m_userNameLineEdit;
    QLabel *passwordLabel;
    QLineEdit *m_passwordLineEdit;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *PasswordDialog)
    {
        if (PasswordDialog->objectName().isEmpty())
            PasswordDialog->setObjectName(QString::fromUtf8("PasswordDialog"));
        PasswordDialog->resize(399, 148);
        gridLayout = new QGridLayout(PasswordDialog);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        m_iconLabel = new QLabel(PasswordDialog);
        m_iconLabel->setObjectName(QString::fromUtf8("m_iconLabel"));
        m_iconLabel->setAlignment(Qt::AlignCenter);

        gridLayout->addWidget(m_iconLabel, 0, 0, 1, 1);

        m_infoLabel = new QLabel(PasswordDialog);
        m_infoLabel->setObjectName(QString::fromUtf8("m_infoLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(m_infoLabel->sizePolicy().hasHeightForWidth());
        m_infoLabel->setSizePolicy(sizePolicy);
        m_infoLabel->setWordWrap(true);

        gridLayout->addWidget(m_infoLabel, 0, 1, 1, 1);

        userLabel = new QLabel(PasswordDialog);
        userLabel->setObjectName(QString::fromUtf8("userLabel"));

        gridLayout->addWidget(userLabel, 1, 0, 1, 1);

        m_userNameLineEdit = new QLineEdit(PasswordDialog);
        m_userNameLineEdit->setObjectName(QString::fromUtf8("m_userNameLineEdit"));

        gridLayout->addWidget(m_userNameLineEdit, 1, 1, 1, 1);

        passwordLabel = new QLabel(PasswordDialog);
        passwordLabel->setObjectName(QString::fromUtf8("passwordLabel"));

        gridLayout->addWidget(passwordLabel, 2, 0, 1, 1);

        m_passwordLineEdit = new QLineEdit(PasswordDialog);
        m_passwordLineEdit->setObjectName(QString::fromUtf8("m_passwordLineEdit"));
        m_passwordLineEdit->setEchoMode(QLineEdit::Password);

        gridLayout->addWidget(m_passwordLineEdit, 2, 1, 1, 1);

        buttonBox = new QDialogButtonBox(PasswordDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        gridLayout->addWidget(buttonBox, 3, 0, 1, 2);

        userLabel->raise();
        m_userNameLineEdit->raise();
        passwordLabel->raise();
        m_passwordLineEdit->raise();
        buttonBox->raise();
        m_iconLabel->raise();
        m_infoLabel->raise();

        retranslateUi(PasswordDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), PasswordDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), PasswordDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(PasswordDialog);
    } // setupUi

    void retranslateUi(QDialog *PasswordDialog)
    {
        PasswordDialog->setWindowTitle(QCoreApplication::translate("PasswordDialog", "Authentication Required", nullptr));
        m_iconLabel->setText(QCoreApplication::translate("PasswordDialog", "Icon", nullptr));
        m_infoLabel->setText(QCoreApplication::translate("PasswordDialog", "Info", nullptr));
        userLabel->setText(QCoreApplication::translate("PasswordDialog", "Username:", nullptr));
        passwordLabel->setText(QCoreApplication::translate("PasswordDialog", "Password:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PasswordDialog: public Ui_PasswordDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PASSWORDDIALOG_H
