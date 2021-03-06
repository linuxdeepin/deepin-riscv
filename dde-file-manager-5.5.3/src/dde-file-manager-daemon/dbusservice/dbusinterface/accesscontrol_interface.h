/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -c AccessControlInterface -p dbusinterface/accesscontrol_interface acesscontrol.xml
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef ACCESSCONTROL_INTERFACE_H
#define ACCESSCONTROL_INTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface com.deepin.filemanager.daemon.AccessControlManager
 */
class AccessControlInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "com.deepin.filemanager.daemon.AccessControlManager"; }

public:
    AccessControlInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr);

    ~AccessControlInterface();

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<QString> FileManagerReply(int policystate)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(policystate);
        return asyncCallWithArgumentList(QStringLiteral("FileManagerReply"), argumentList);
    }

    inline QDBusPendingReply<QVariantList> QueryAccessPolicy()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("QueryAccessPolicy"), argumentList);
    }

    inline QDBusPendingReply<QVariantList> QueryVaultAccessPolicy()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("QueryVaultAccessPolicy"), argumentList);
    }

    inline QDBusPendingReply<int> QueryVaultAccessPolicyVisible()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("QueryVaultAccessPolicyVisible"), argumentList);
    }

    inline QDBusPendingReply<QString> SetAccessPolicy(const QVariantMap &policy)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(policy);
        return asyncCallWithArgumentList(QStringLiteral("SetAccessPolicy"), argumentList);
    }

    inline QDBusPendingReply<QString> SetVaultAccessPolicy(const QVariantMap &policy)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(policy);
        return asyncCallWithArgumentList(QStringLiteral("SetVaultAccessPolicy"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void AccessPolicySetFinished(const QVariantMap &policy);
    void AccessVaultPolicyNotify();
    void DeviceAccessPolicyChanged(const QVariantList &policy);
};

namespace com {
  namespace deepin {
    namespace filemanager {
      namespace daemon {
        typedef ::AccessControlInterface AccessControlManager;
      }
    }
  }
}
#endif
