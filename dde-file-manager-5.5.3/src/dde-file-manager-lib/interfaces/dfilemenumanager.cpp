/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dfilemenumanager.h"
#include "dfmglobal.h"
#include "app/define.h"
#include "dfmevent.h"
#include "dfilemenu.h"
#include "dfileservices.h"
#include "dfmeventdispatcher.h"
#include "dfmapplication.h"
#include "dfmsettings.h"
#include "controllers/vaultcontroller.h"
#include "controllers/appcontroller.h"
#include "controllers/trashmanager.h"
#include "models/desktopfileinfo.h"
#include "shutil/mimetypedisplaymanager.h"
#include "singleton.h"
#include "views/windowmanager.h"
#include "shutil/fileutils.h"
#include "shutil/mimesappsmanager.h"
#include "shutil/danythingmonitorfilter.h"
#include "controllers/pathmanager.h"
#include "plugins/pluginmanager.h"
#include "dde-file-manager-plugins/plugininterfaces/menu/menuinterface.h"
#include "dfmstandardpaths.h"
#include "deviceinfo/udisklistener.h"
#include "ddiskmanager.h"
#include "dblockdevice.h"
#include "ddiskdevice.h"
#include "views/dtagactionwidget.h"
#include "plugins/dfmadditionalmenu.h"
#include "models/dfmrootfileinfo.h"
#include "bluetooth/bluetoothmanager.h"
#include "bluetooth/bluetoothmodel.h"
#include "io/dstorageinfo.h"
#include "vault/vaultlockmanager.h"
#include "vault/vaulthelper.h"
#include "app/filesignalmanager.h"
#include "views/dfilemanagerwindow.h"
#include "customization/dcustomactionbuilder.h"
#include "customization/dcustomactionparser.h"
#include "gvfs/gvfsmountmanager.h"

#include <DSysInfo>

#include <QMetaObject>
#include <QMetaEnum>
#include <QMenu>
#include <QTextCodec>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QQueue>
#include <QDebug>
#include <QPushButton>
#include <QWidgetAction>

#include <dgiosettings.h>
#include <unistd.h>


//fix:???????????????????????????????????????????????????????????????????????????????????????

namespace DFileMenuData {
static QMap<MenuAction, QString> actionKeys;
static QMap<MenuAction, QIcon> actionIcons;
static QMap<MenuAction, QAction *> actions;
static QMap<const QAction *, MenuAction> actionToMenuAction;
static QMap<MenuAction, QString> actionIDs;
static QVector<MenuAction> sortActionTypes;
static QSet<MenuAction> whitelist;
static QSet<MenuAction> blacklist;
static QQueue<MenuAction> availableUserActionQueue;
static DFMAdditionalMenu *additionalMenu;
static DCustomActionParser *customMenuParser = nullptr;

void initData();
void initActions();
void clearActions();

MenuAction takeAvailableUserActionType()
{
    if (availableUserActionQueue.isEmpty()) {
        availableUserActionQueue.append(MenuAction(MenuAction::UserMenuAction + 1));

        return MenuAction::UserMenuAction;
    }

    MenuAction type = availableUserActionQueue.dequeue();

    if (availableUserActionQueue.isEmpty()) {
        availableUserActionQueue.append(MenuAction(type + 1));
    }

    return type;
}
void recycleUserActionType(MenuAction type)
{
    availableUserActionQueue.prepend(type);
    QAction *action = actions.take(type);

    if (action) {
        actionToMenuAction.remove(action);
    }
    if (DFileMenuManager::needDeleteAction())
        delete action;
}
}

DFileMenu *DFileMenuManager::createDefaultBookMarkMenu(const QSet<MenuAction> &disableList)
{
    QVector<MenuAction> actionKeys;

    actionKeys.reserve(2);

    actionKeys << MenuAction::OpenInNewWindow
               << MenuAction::OpenInNewTab
               << MenuAction::Property;

    return genereteMenuByKeys(actionKeys, disableList);
}

DFileMenu *DFileMenuManager::createUserShareMarkMenu(const QSet<MenuAction> &disableList)
{
    QVector<MenuAction> actionKeys;

    actionKeys.reserve(1);

    actionKeys << MenuAction::OpenInNewWindow
               << MenuAction::OpenInNewTab;

    DFileMenu *menu = genereteMenuByKeys(actionKeys, disableList);
    menu->setAccessibleInfo(AC_FILE_MENU_USER_SHARE);
    return menu;
}

DFileMenu *DFileMenuManager::createToolBarSettingsMenu(const QSet<MenuAction> &disableList)
{
    QVector<MenuAction> actionKeys;
    QMap<MenuAction, QVector<MenuAction> >  subMenuKeys;

    actionKeys.reserve(5);

    actionKeys << MenuAction::NewWindow
               << MenuAction::Separator
               << MenuAction::ConnectToServer
               << MenuAction::SetUserSharePassword
               << MenuAction::Settings;

    DFileMenu *menu = genereteMenuByKeys(actionKeys, disableList, false, subMenuKeys, false);
    menu->setAccessibleInfo(AC_FILE_MENU_TOOLBAR_SEETINGS);
    return menu;
}

DFileMenu *DFileMenuManager::createNormalMenu(const DUrl &currentUrl, const DUrlList &urlList, QSet<MenuAction> disableList, QSet<MenuAction> unusedList, int windowId, bool onDesktop)
{
    // remove compress/decompress action
    unusedList << MenuAction::Compress << MenuAction::Decompress << MenuAction::DecompressHere;

    DAbstractFileInfoPointer info = fileService->createFileInfo(Q_NULLPTR, currentUrl);
    DFileMenu *menu = Q_NULLPTR;
    if (!info) {
        return menu;
    }

    //! urlList??????????????????DUrl?????????????????????????????????????????????????????????
    DUrlList urls = urlList;
    for (int i = 0; i < urlList.size(); ++i) {
        if (urlList[i].isVaultFile())
            urls[i] = VaultController::vaultToLocalUrl(urlList[i]);
    }

    auto dirInUrls = [](const DUrlList &urls) {
        for (const auto &url: urls) {
            auto fileInfo = fileService->createFileInfo(nullptr, url);
            if (fileInfo && fileInfo->isDir())
                return true;
        }
        return false;
    };

    DUrlList redirectedUrlList;
    if (urls.length() == 1) {
        QVector<MenuAction> actions = info->menuActionList(DAbstractFileInfo::SingleFile);
        //?????????????????????????????????????????????????????????????????????
        //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????MenuAction::CompleteDeletion
        //??????????????????canrename?????????false???????????????????????????????????????MenuAction::Cut???MenuAction::Rename
        // MenuAction::delete
        //???????????????????????????canrename?????????true???????????????false???????????????
        //??????????????????MenuAction::CompleteDeletion???????????????
        if (FileUtils::isGvfsMountFile(info->absoluteFilePath()) && !info->canRename()) {
            disableList << MenuAction::CompleteDeletion;
        }
        if (info->isDir()) { //??????????????????
            if (info->ownerId() != getuid() && !DFMGlobal::isRootUser()) { //????????????????????????????????????????????????????????????????????????????????????
                disableList << MenuAction::UnShare << MenuAction::Share; //??????????????????????????????????????????
            }
        }
        foreach (MenuAction action, unusedList) {
            if (actions.contains(action)) {
                actions.remove(actions.indexOf(action));
            }
        }

        if (actions.isEmpty()) {
            return menu;
        }

        const QMap<MenuAction, QVector<MenuAction> > &subActions = info->subMenuActionList();
        disableList += DFileMenuManager::getDisableActionList(urls);
        const bool &tabAddable = WindowManager::tabAddableByWinId(windowId);
        if (!tabAddable) {
            disableList << MenuAction::OpenInNewTab;
        }

        ///###: tag protocol.
        if (!DFileMenuManager::whetherShowTagActions(urls)) {
            actions.removeAll(MenuAction::TagInfo);
            actions.removeAll(MenuAction::TagFilesUseColor);
        }

        // sp3 feature: root??????, ?????????????????????, ???????????????????????????????????????????????????????????????
        if (DFMGlobal::isRootUser() || DFMGlobal::isServerSys() || !DFMGlobal::isDeveloperMode()) {
            actions.removeAll(MenuAction::OpenAsAdmin);
        }

        menu = DFileMenuManager::genereteMenuByKeys(actions, disableList, true, subActions);
    } else {
        bool isSystemPathIncluded = false;
        bool isAllCompressedFiles = true;
//        QMimeType fileMimeType;
        QStringList supportedMimeTypes;
        bool mime_displayOpenWith = true;
        //fix bug 35546 ????????????????????????5.1.2.2-1??????sp2??????sp1?????????????????????????????????????????????????????????
        //?????????????????????????????????????????????????????????
        if (!info->isWritable() && !info->isFile() && !info->isSymLink()) {
            disableList << MenuAction::Delete;
        }

#if 1   //!fix bug#29264.?????????????????????????????????MimesAppsManager::getDefaultAppDesktopFileByMimeType
        //!?????????????????????app?????????????????????????????????????????????app???supportedMimeTypes????????????????????????mimeTypeList??????
        //!????????????????????????matched???false????????????????????????????????????????????????????????????????????????????????????app??????????????????????????????????????????

        //?????????????????????app
        if (supportedMimeTypes.isEmpty()) {
            QMimeType fileMimeType = info->mimeType();
            QString defaultAppDesktopFile = MimesAppsManager::getDefaultAppDesktopFileByMimeType(fileMimeType.name());
            QSettings desktopFile(defaultAppDesktopFile, QSettings::IniFormat);
            desktopFile.setIniCodec("UTF-8");
            Properties mimeTypeList(defaultAppDesktopFile, "Desktop Entry");
            supportedMimeTypes = mimeTypeList.value("MimeType").toString().split(';');
            supportedMimeTypes.removeAll("");
        }

        redirectedUrlList.clear();
        foreach (DUrl url, urls) {
            const DAbstractFileInfoPointer &file_info = fileService->createFileInfo(Q_NULLPTR, url);
            // fix bug202007010011 ?????????????????????????????????????????????????????????
            auto redirectedUrl = file_info->redirectedFileUrl();
            if (redirectedUrl.isValid()) {
                redirectedUrlList << redirectedUrl;
            }

            if (!FileUtils::isArchive(url.path())) {
                isAllCompressedFiles = false;
            }

            if (systemPathManager->isSystemPath(file_info->fileUrl().toLocalFile())) {
                isSystemPathIncluded = true;
            }

            if (!mime_displayOpenWith) {
                continue;
            }

            QStringList mimeTypeList = { file_info->mimeType().name() };
            mimeTypeList.append(file_info->mimeType().parentMimeTypes());
            bool matched = false;

            //???????????????????????????
            if (file_info->suffix() == info->suffix()) {
                matched = true;
            } else {
                for (const QString &oneMimeType : mimeTypeList) {
                    if (supportedMimeTypes.contains(oneMimeType)) {
                        matched = true;
                        break;
                    }
                }
            }

            if (!matched) {
                mime_displayOpenWith = false;
                disableList << MenuAction::Open << MenuAction::OpenWith;
                break;
            }
        }
#else  //???????????????
        foreach (DUrl url, urls) {
            const DAbstractFileInfoPointer &fileInfo = fileService->createFileInfo(Q_NULLPTR, url);

            if (!FileUtils::isArchive(url.path())) {
                isAllCompressedFiles = false;
            }

            if (systemPathManager->isSystemPath(fileInfo->fileUrl().toLocalFile())) {
                isSystemPathIncluded = true;
            }

            if (!mime_displayOpenWith) {
                continue;
            }

            if (supportedMimeTypes.isEmpty()) {
                QMimeType fileMimeType = fileInfo->mimeType();
                QString defaultAppDesktopFile = MimesAppsManager::getDefaultAppDesktopFileByMimeType(fileMimeType.name());
                QSettings desktopFile(defaultAppDesktopFile, QSettings::IniFormat);
                desktopFile.setIniCodec("UTF-8");
                Properties mimeTypeList(defaultAppDesktopFile, "Desktop Entry");
                supportedMimeTypes = mimeTypeList.value("MimeType").toString().split(';');
                supportedMimeTypes.removeAll({});
            } else {
                QStringList mimeTypeList = { fileInfo->mimeType().name() };
                mimeTypeList.append(fileInfo->mimeType().parentMimeTypes());
                bool matched = false;
                for (const QString &oneMimeType : mimeTypeList) {
                    if (supportedMimeTypes.contains(oneMimeType)) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    mime_displayOpenWith = false;
                    disableList << MenuAction::Open << MenuAction::OpenWith;
                }
            }
        }
#endif
        QVector<MenuAction> actions;

        if (isSystemPathIncluded) {
            actions = info->menuActionList(DAbstractFileInfo::MultiFilesSystemPathIncluded);
        } else {
            actions = info->menuActionList(DAbstractFileInfo::MultiFiles);
        }

        if (actions.isEmpty()) {
            return menu;
        }

        if (isAllCompressedFiles) {
            int index = actions.indexOf(MenuAction::Compress);
            actions.insert(index + 1, MenuAction::Decompress);
            actions.insert(index + 2, MenuAction::DecompressHere);
        }

        const QMap<MenuAction, QVector<MenuAction> > &subActions  = info->subMenuActionList();
        disableList += DFileMenuManager::getDisableActionList(urls);
        const bool &tabAddable = WindowManager::tabAddableByWinId(windowId);
        if (!tabAddable) {
            disableList << MenuAction::OpenInNewTab;
        }

        foreach (MenuAction action, unusedList) {
            if (actions.contains(action)) {
                actions.remove(actions.indexOf(action));
            }
        }

        ///###: tag protocol.
        if (!DFileMenuManager::whetherShowTagActions(urls)) {
            actions.removeAll(MenuAction::TagInfo);
            actions.removeAll(MenuAction::TagFilesUseColor);
        }

        menu = DFileMenuManager::genereteMenuByKeys(actions, disableList, true, subActions);
    }

    QAction *openWithAction = menu->actionAt(DFileMenuManager::getActionString(DFMGlobal::OpenWith));
    DFileMenu *openWithMenu = openWithAction ? qobject_cast<DFileMenu *>(openWithAction->menu()) : Q_NULLPTR;

    if (openWithMenu && openWithMenu->isEnabled()) {
        QStringList recommendApps = mimeAppsManager->getRecommendedApps(info->redirectedFileUrl());
        recommendApps.removeOne("/usr/share/applications/dde-open.desktop"); //????????????????????????????????????/usr/share/applications/dde-open.desktop???????????????????????????????????????
//        bug 20275 ?????????????????????V20??????beta??????DDE?????? ????????????????????????????????????????????????ImageMagick????????????
        recommendApps.removeOne("/usr/share/applications/display-im6.q16.desktop"); //??????????????????????????????
        recommendApps.removeOne("/usr/share/applications/display-im6.q16hdri.desktop"); //??????????????????????????????
        foreach (QString app, recommendApps) {
//            const DesktopFile& df = mimeAppsManager->DesktopObjs.value(app);
            //ignore no show apps
//            if(df.getNoShow())
//                continue;
            DesktopFile desktopFile(app);
            QAction *action = new QAction(desktopFile.getDisplayName(), openWithMenu);
            action->setIcon(FileUtils::searchAppIcon(desktopFile));
            action->setProperty("app", app);
            if (urls.length() == 1) {
                action->setProperty("url", QVariant::fromValue(info->redirectedFileUrl()));
            } else {
#if 0       // fix bug202007010011 ?????????????????????????????????????????????????????????
                DUrlList redirectedUrlList;
                for (auto url : urls) {
                    DAbstractFileInfoPointer info = fileService->createFileInfo(Q_NULLPTR, url);
                    auto redirectedUrl = info->redirectedFileUrl();
                    if (redirectedUrl.isValid()) {
                        redirectedUrlList << redirectedUrl;
                    }
                }
#endif
                action->setProperty("urls", QVariant::fromValue(redirectedUrlList));
            }
            openWithMenu->addAction(action);
            connect(action, &QAction::triggered, appController, &AppController::actionOpenFileByApp);
        }

        QAction *action = new QAction(fileMenuManger->getActionString(MenuAction::OpenWithCustom), openWithMenu);
        action->setData((int)MenuAction::OpenWithCustom);
        openWithMenu->addAction(action);
        DFileMenuData::actions[MenuAction::OpenWithCustom] = action;
        DFileMenuData::actionToMenuAction[action] = MenuAction::OpenWithCustom;
    }

    if (deviceListener->isMountedRemovableDiskExits()
            || bluetoothManager->hasAdapter()) {
        QAction *sendToMountedRemovableDiskAction = menu->actionAt(DFileMenuManager::getActionString(DFMGlobal::SendToRemovableDisk));
        if (currentUrl.path().contains("/dev/sr")
                || (currentUrl.scheme() == SEARCH_SCHEME && currentUrl.query().contains("/dev/sr"))) // ?????????????????????????????????????????????
            menu->removeAction(sendToMountedRemovableDiskAction);
        else {
            DFileMenu *sendToMountedRemovableDiskMenu = sendToMountedRemovableDiskAction ? qobject_cast<DFileMenu *>(sendToMountedRemovableDiskAction->menu()) : Q_NULLPTR;
            if (sendToMountedRemovableDiskMenu) {
                // ?????????????????????????????????????????????????????????
                if (BluetoothManager::bluetoothSendEnable() // is system disabled "sending via bluetooth"
                        && bluetoothManager->hasAdapter()
                        && !VaultController::isVaultFile(currentUrl.toLocalFile())) {
                    QAction *sendToBluetooth = new QAction(DFileMenuManager::getActionString(DFMGlobal::SendToBluetooth), sendToMountedRemovableDiskMenu);
                    sendToBluetooth->setProperty("urlList", DUrl::toStringList(urls));
                    sendToMountedRemovableDiskMenu->addAction(sendToBluetooth);
                    connect(sendToBluetooth, &QAction::triggered, appController, &AppController::actionSendToBluetooth);
                    if (dirInUrls(urls))
                        sendToBluetooth->setEnabled(false);
                }

                foreach (UDiskDeviceInfoPointer pDeviceinfo, deviceListener->getCanSendDisksByUrl(currentUrl.toLocalFile()).values()) {
                    //fix:??????????????????????????????????????????????????????????????????????????????????????? id="/dev/sr1" -> tempId="sr1"
                    QString tempId = pDeviceinfo->getDiskInfo().id().mid(5);
                    DUrl gvfsmpurl;
                    gvfsmpurl.setScheme(DFMROOT_SCHEME);
                    gvfsmpurl.setPath("/" + QUrl::toPercentEncoding(tempId) + "." SUFFIX_UDISKS);

                    DAbstractFileInfoPointer fp(new DFMRootFileInfo(gvfsmpurl)); // ??????DFMRootFileInfo ?????????????????????????????? ??????

                    qDebug() << "add send action: [ diskinfoname:" << pDeviceinfo->getDiskInfo().name() << " to RootFileInfo: " << fp->fileDisplayName();

                    QAction *action = new QAction(fp->fileDisplayName(), sendToMountedRemovableDiskMenu);
                    action->setProperty("mounted_root_uri", pDeviceinfo->getDiskInfo().mounted_root_uri());
                    action->setProperty("urlList", DUrl::toStringList(urls));
                    action->setProperty("blkDevice", tempId);

                    // ???????????????????????????????????????
                    if (urls.count() > 0) {
                        DUrl durl = urls[0];
                        if (durl.path().contains(pDeviceinfo->getDiskInfo().id()))
                            action->setEnabled(false);
                    }

                    sendToMountedRemovableDiskMenu->addAction(action);
                    connect(action, &QAction::triggered, appController, &AppController::actionSendToRemovableDisk, Qt::QueuedConnection); //?????????????????????exec?????????????????????bug#25613
                }
                // ????????????????????????????????????????????????
                if(sendToMountedRemovableDiskMenu->actions().count() < 1) {
                    menu->removeAction(sendToMountedRemovableDiskAction);
                }
            }

        }
    }
    if (QAction *stageAction = menu->actionAt(DFileMenuManager::getActionString(DFMGlobal::StageFileForBurning))) {

        QMap<QString, DUrl> diskUrlsMap;
        QStringList odrv;
        DDiskManager diskm;
        for (auto &blks : diskm.blockDevices({})) {
            QScopedPointer<DBlockDevice> blk(DDiskManager::createBlockDevice(blks));
            QScopedPointer<DDiskDevice> drv(DDiskManager::createDiskDevice(blk->drive()));
            if (drv->mediaCompatibility().join(' ').contains("_r")) {
                if ((currentUrl.scheme() == BURN_SCHEME && QString(blk->device()) == currentUrl.burnDestDevice()) || odrv.contains(drv->path())
                        || (currentUrl.scheme() == SEARCH_SCHEME && currentUrl.query().contains("/dev/sr"))) { // ???????????????????????????????????????????????????????????????
                    continue;
                }
                DUrl rootUrl(DFMROOT_ROOT + QString(blk->device()).mid(QString("/dev/").length()) + ".localdisk");
                odrv.push_back(drv->path());
                diskUrlsMap[drv->path()] = rootUrl;
            }
        }

        if (odrv.empty()) {
            stageAction->setEnabled(false);
        } else if (odrv.size() == 1) {
            QScopedPointer<DDiskDevice> dev(DDiskManager::createDiskDevice(odrv.front()));
            QString devID = dev->id();
            stageAction->setProperty("dest_drive", odrv.front());
            stageAction->setProperty("urlList", DUrl::toStringList(urls));
            connect(stageAction, &QAction::triggered, appController, &AppController::actionStageFileForBurning, Qt::UniqueConnection);
            if (DFileMenu *stageMenu = qobject_cast<DFileMenu *>(stageAction->menu())) {
                stageAction->setMenu(nullptr);
                delete stageMenu;
            }
        } else {
            if (DFileMenu *stageMenu = qobject_cast<DFileMenu *>(stageAction->menu())) {
                for (auto &devs : odrv) {
                    QScopedPointer<DDiskDevice> dev(DDiskManager::createDiskDevice(devs));
                    //???????????? ????????????????????? ?????????????????????????????????displayName
                    DAbstractFileInfoPointer fi;
                    QString devName(dev->id());
                    if (diskUrlsMap.contains(devs)) {
                        fi = fileService->createFileInfo(nullptr, diskUrlsMap[devs]);
                        if (fi) {
                            if (!fi->fileDisplayName().isEmpty()) {
                                devName = fi->fileDisplayName();
                            }
                        }
                    }
                    QAction *action = new QAction(devName, stageMenu);
                    action->setProperty("dest_drive", devs);
                    action->setProperty("urlList", DUrl::toStringList(urls));
                    stageMenu->addAction(action);
                    connect(action, &QAction::triggered, appController, &AppController::actionStageFileForBurning, Qt::UniqueConnection);
                }
            }
        }
    }

    if (currentUrl == DesktopFileInfo::computerDesktopFileUrl()
            || currentUrl == DesktopFileInfo::trashDesktopFileUrl()
            || currentUrl == DesktopFileInfo::homeDesktopFileUrl()) {
        return menu;
    }
    // ????????????????????????????????????????????????
    if(!VaultController::isVaultFile(currentUrl.toLocalFile())) {
        loadNormalPluginMenu(menu, urls, currentUrl, onDesktop);
    }
    // stop loading Extension menus from json files
    //loadNormalExtensionMenu(menu, urlList, currentUrl);

    return menu;
}

DFileMenu *DFileMenuManager::createVaultMenu(QWidget *topWidget, const QObject *sender)
{
    DFileMenu *menu = nullptr;

    DFileManagerWindow *wnd = qobject_cast<DFileManagerWindow *>(topWidget);
    VaultController *controller = VaultController::ins();

    VaultController::VaultState vaultState = controller->state();

    DUrl durl = controller->vaultToLocalUrl(controller->makeVaultUrl());
    durl.setScheme(DFMVAULT_SCHEME);
    const DAbstractFileInfoPointer infoPointer = DFileService::instance()->createFileInfo(nullptr, durl);

    QSet<MenuAction> disableList;
    if (!VaultLockManager::getInstance().isValid()) {
        disableList << MenuAction::FiveMinutes
                    << MenuAction::TenMinutes
                    << MenuAction::TwentyMinutes;
    }

    menu = DFileMenuManager::genereteMenuByKeys(infoPointer->menuActionList(), disableList, true, infoPointer->subMenuActionList(), false);
    menu->setEventData(DUrl(), {durl}, WindowManager::getWindowId(wnd), sender);
    menu->setAccessibleInfo(AC_FILE_MENU_VAULT);

    auto lockNow = [](DFileManagerWindow * wnd)->bool {
        //! Is there a vault task, top it if exist.
        if (!VaultHelper::topVaultTasks())
        {
            emit fileSignalManager->requestCloseAllTabOfVault(wnd->windowId());
            VaultController::ins()->lockVault();
        }

        return true;
    };

    auto autoLock = [](int lockState)->bool {
        return VaultLockManager::getInstance().autoLock(static_cast<VaultLockManager::AutoLockState>(lockState));
    };

    auto showView = [&](QWidget * wndPtr, QString host) {
        DFileManagerWindow *file_mng_wnd = qobject_cast<DFileManagerWindow *>(wndPtr);
        file_mng_wnd->cd(VaultController::makeVaultUrl("/", host));
    };

    if (vaultState == VaultController::Unlocked) {

        //! ????????????
        QAction *action = DFileMenuManager::getAction(MenuAction::LockNow);
        QObject::connect(action, &QAction::triggered, action, [ &, wnd]() {
            lockNow(wnd);
        });

        //! ????????????
        VaultLockManager::AutoLockState lockState = VaultLockManager::getInstance().autoLockState();

        QAction *actionNever = DFileMenuManager::getAction(MenuAction::Never);
        QObject::connect(actionNever, &QAction::triggered, actionNever, [&]() {
            autoLock(VaultLockManager::Never);
        });
        actionNever->setCheckable(true);
        actionNever->setChecked(lockState == VaultLockManager::Never ? true : false);

        QAction *actionFiveMins = DFileMenuManager::getAction(MenuAction::FiveMinutes);
        QObject::connect(actionFiveMins, &QAction::triggered, actionFiveMins, [&]() {
            autoLock(VaultLockManager::FiveMinutes);
        });
        actionFiveMins->setCheckable(true);
        actionFiveMins->setChecked(lockState == VaultLockManager::FiveMinutes ? true : false);

        QAction *actionTenMins = DFileMenuManager::getAction(MenuAction::TenMinutes);
        QObject::connect(actionTenMins, &QAction::triggered, actionTenMins, [&]() {
            autoLock(VaultLockManager::TenMinutes);
        });
        actionTenMins->setCheckable(true);
        actionTenMins->setChecked(lockState == VaultLockManager::TenMinutes ? true : false);

        QAction *actionTwentyMins = DFileMenuManager::getAction(MenuAction::TwentyMinutes);
        QObject::connect(actionTwentyMins, &QAction::triggered, actionTwentyMins, [&]() {
            autoLock(VaultLockManager::TwentyMinutes);
        });
        actionTwentyMins->setCheckable(true);
        actionTwentyMins->setChecked(lockState == VaultLockManager::TwentyMinutes ? true : false);

        //! ???????????????
        action = DFileMenuManager::getAction(MenuAction::DeleteVault);
        QObject::connect(action, &QAction::triggered, action, [ &, topWidget]() {
            showView(topWidget, "delete");
        });
    } else if (vaultState == VaultController::Encrypted) {

        //! ??????
        QAction *action = DFileMenuManager::getAction(MenuAction::UnLock);
        QObject::connect(action, &QAction::triggered, action, [ &, topWidget]() {
            showView(topWidget, "unlock");
        });

        //! ??????????????????
        action = DFileMenuManager::getAction(MenuAction::UnLockByKey);
        QObject::connect(action, &QAction::triggered, action, [ &, topWidget]() {
            showView(topWidget, "certificate");
        });
    }

    return menu;
}

QList<QAction *> DFileMenuManager::loadNormalPluginMenu(DFileMenu *menu, const DUrlList &urlList, const DUrl &currentUrl, bool onDesktop)
{
    qDebug() << "load normal plugin menu";
    QStringList files;
    foreach (DUrl url, urlList) {
        files << url.toString();
    }

    // ???menu->actions()?????????????????????ut??????(????????????????????????????????????????????????????????????????????????????????????)
    QList<QAction *> actions;
    if (menu->actions().isEmpty())
        return actions;

    QAction *lastAction = menu->actions().last();
    if (lastAction->isSeparator()) {
        lastAction = menu->actionAt(menu->actions().count() - 2);
    }

    if (DFileMenuData::additionalMenu) {
        actions = DFileMenuData::additionalMenu->actions(files, currentUrl.toString(), onDesktop);
    }
    foreach (QAction *action, actions) {
        menu->insertAction(lastAction, action);
    }

    menu->insertSeparator(lastAction);
    return actions;
}

QList<QAction *> DFileMenuManager::loadEmptyAreaPluginMenu(DFileMenu *menu, const DUrl &currentUrl, bool onDesktop)
{
    qDebug() << "load empty area plugin menu";
    QList<QAction *> actions;
    // ???menu->actions()??????????????????(????????????????????????????????????????????????????????????????????????????????????)
    if (menu->actions().isEmpty())
        return actions;
    QAction *lastAction = menu->actions().last();
    if (lastAction->isSeparator()) {
        lastAction = menu->actionAt(menu->actions().count() - 2);
    }

    if (DFileMenuData::additionalMenu) {
        actions = DFileMenuData::additionalMenu->actions({}, currentUrl.toString(), onDesktop);
    }

    for (QAction *action : actions) {
        menu->insertAction(lastAction, action);
    }

    menu->insertSeparator(lastAction);
    return actions;
}

QAction *DFileMenuManager::getAction(MenuAction action)
{
    return DFileMenuData::actions.value(action);
}

QString DFileMenuManager::getActionText(MenuAction action)
{
    return DFileMenuData::actionKeys.value(action);
}

QSet<MenuAction> DFileMenuManager::getDisableActionList(const DUrl &fileUrl)
{
    DUrlList list;

    list << fileUrl;

    return getDisableActionList(list);
}

QSet<MenuAction> DFileMenuManager::getDisableActionList(const DUrlList &urlList)
{
    QSet<MenuAction> disableList;

    for (const DUrl &file_url : urlList) {
        DUrl durl = file_url;
        if (VaultController::isVaultFile(durl.path())) {
            durl = VaultController::localUrlToVault(file_url);
        }
        const DAbstractFileInfoPointer &file_info = fileService->createFileInfo(Q_NULLPTR, durl);

        if (file_info) {
            disableList += file_info->disableMenuActionList();
        }
    }

    if (DFMGlobal::instance()->clipboardAction() == DFMGlobal::UnknowAction) {
        disableList << MenuAction::Paste;
    }

    return disableList;
}

DFileMenuManager::DFileMenuManager()
{
    qRegisterMetaType<QMap<QString, QString>>(QT_STRINGIFY(QMap<QString, QString>));
    qRegisterMetaType<QList<QUrl>>(QT_STRINGIFY(QList<QUrl>));
}

DFileMenuManager::~DFileMenuManager()
{
    if (DFileMenuData::additionalMenu) {
        DFileMenuData::additionalMenu->deleteLater();
        DFileMenuData::additionalMenu = nullptr;
    }

    if (DFileMenuData::customMenuParser) {
        DFileMenuData::customMenuParser->deleteLater();
        DFileMenuData::customMenuParser = nullptr;
    }
}

void DFileMenuData::initData()
{
    actionKeys[MenuAction::Open] = QObject::tr("Open");
    actionKeys[MenuAction::OpenInNewWindow] = QObject::tr("Open in new window");
    actionKeys[MenuAction::OpenInNewTab] = QObject::tr("Open in new tab");
    actionKeys[MenuAction::OpenDisk] = QObject::tr("Open");
    actionKeys[MenuAction::OpenDiskInNewWindow] = QObject::tr("Open in new window");
    actionKeys[MenuAction::OpenDiskInNewTab] = QObject::tr("Open in new tab");
    actionKeys[MenuAction::OpenAsAdmin] = QObject::tr("Open as administrator");
    actionKeys[MenuAction::OpenWith] = QObject::tr("Open with");
    actionKeys[MenuAction::OpenWithCustom] = QObject::tr("Select default program");
    actionKeys[MenuAction::OpenFileLocation] = QObject::tr("Open file location");
    actionKeys[MenuAction::Compress] = QObject::tr("Compress");
    actionKeys[MenuAction::Decompress] = QObject::tr("Extract");
    actionKeys[MenuAction::DecompressHere] = QObject::tr("Extract here");
    actionKeys[MenuAction::Cut] = QObject::tr("Cut");
    actionKeys[MenuAction::Copy] = QObject::tr("Copy");
    actionKeys[MenuAction::Paste] = QObject::tr("Paste");
    actionKeys[MenuAction::Rename] = QObject::tr("Rename");
    actionKeys[MenuAction::BookmarkRename] = QObject::tr("Rename");
    actionKeys[MenuAction::BookmarkRemove] = QObject::tr("Remove bookmark");
    actionKeys[MenuAction::CreateSymlink] = QObject::tr("Create link");
    actionKeys[MenuAction::SendToDesktop] = QObject::tr("Send to desktop");
    actionKeys[MenuAction::SendToRemovableDisk] = QObject::tr("Send to");
    actionKeys[MenuAction::SendToBluetooth] = QObject::tr("Bluetooth");
    actionKeys[MenuAction::AddToBookMark] = QObject::tr("Add to bookmark");
    actionKeys[MenuAction::Delete] = QObject::tr("Delete");
    actionKeys[MenuAction::CompleteDeletion] = QObject::tr("Delete");
    actionKeys[MenuAction::Property] = QObject::tr("Properties");

    actionKeys[MenuAction::NewFolder] = QObject::tr("New folder");
    actionKeys[MenuAction::NewWindow] = QObject::tr("New window");
    actionKeys[MenuAction::SelectAll] = QObject::tr("Select all");
    actionKeys[MenuAction::ClearRecent] = QObject::tr("Clear recent history");
    actionKeys[MenuAction::ClearTrash] = QObject::tr("Empty Trash");
    actionKeys[MenuAction::DisplayAs] = QObject::tr("Display as");
    actionKeys[MenuAction::SortBy] = QObject::tr("Sort by");
    actionKeys[MenuAction::NewDocument] = QObject::tr("New document");
    actionKeys[MenuAction::NewWord] = QObject::tr("Office Text");
    actionKeys[MenuAction::NewExcel] = QObject::tr("Spreadsheets");
    actionKeys[MenuAction::NewPowerpoint] = QObject::tr("Presentation");
    actionKeys[MenuAction::NewText] = QObject::tr("Plain Text");
    actionKeys[MenuAction::OpenInTerminal] = QObject::tr("Open in terminal");
    actionKeys[MenuAction::Restore] = QObject::tr("Restore");
    actionKeys[MenuAction::RestoreAll] = QObject::tr("Restore all");
    actionKeys[MenuAction::Mount] = QObject::tr("Mount");
    actionKeys[MenuAction::Unmount] = QObject::tr("Unmount");
    actionKeys[MenuAction::Eject] = QObject::tr("Eject");
    actionKeys[MenuAction::SafelyRemoveDrive] = QObject::tr("Safely Remove");
    actionKeys[MenuAction::RemoveFromRecent] = QObject::tr("Remove");
    actionKeys[MenuAction::Name] = QObject::tr("Name");
    actionKeys[MenuAction::Size] = QObject::tr("Size");
    actionKeys[MenuAction::Type] = QObject::tr("Type");
    actionKeys[MenuAction::CreatedDate] = QObject::tr("Time created");
    actionKeys[MenuAction::LastModifiedDate] = QObject::tr("Time modified");
    actionKeys[MenuAction::LastRead] = qApp->translate("DFileSystemModel", "Last access");
    actionKeys[MenuAction::Settings] = QObject::tr("Settings");
    actionKeys[MenuAction::Exit] = QObject::tr("Exit");
    actionKeys[MenuAction::IconView] = QObject::tr("Icon");
    actionKeys[MenuAction::ListView] = QObject::tr("List");
    actionKeys[MenuAction::ExtendView] = QObject::tr("Extend");
    actionKeys[MenuAction::SetAsWallpaper] = QObject::tr("Set as wallpaper");
    actionKeys[MenuAction::ForgetPassword] = QObject::tr("Clear saved password and unmount");
    actionKeys[MenuAction::DeletionDate] = QObject::tr("Time deleted");
    actionKeys[MenuAction::SourcePath] = QObject::tr("Source path");
    actionKeys[MenuAction::AbsolutePath] = QObject::tr("Path");
    actionKeys[MenuAction::Share] = QObject::tr("Share folder");
    actionKeys[MenuAction::UnShare] = QObject::tr("Cancel sharing");
    actionKeys[MenuAction::Vault] = QObject::tr("File Vault");
    actionKeys[MenuAction::ConnectToServer] = QObject::tr("Connect to Server");
    actionKeys[MenuAction::SetUserSharePassword] = QObject::tr("Set share password");
    actionKeys[MenuAction::FormatDevice] = QObject::tr("Format");
    actionKeys[MenuAction::OpticalBlank] = QObject::tr("Erase");

    ///###: tag protocol.
    actionKeys[MenuAction::TagInfo] = QObject::tr("Tag information");
    actionKeys[MenuAction::TagFilesUseColor] = QString{"Add color tags"};
    actionKeys[MenuAction::DeleteTags] = QObject::tr("Delete");
    actionKeys[MenuAction::ChangeTagColor] = QString{"Change color of present tag"};
    actionKeys[MenuAction::RenameTag] = QObject::tr("Rename");

    actionKeys[MenuAction::MountImage] = QObject::tr("Mount");
    //fix: ????????????????????????"??????"???"?????????????????????"
    //actionKeys[MenuAction::StageFileForBurning] = QObject::tr("Burn");
    actionKeys[MenuAction::StageFileForBurning] = QObject::tr("Add to disc");

    // Vault
    actionKeys[MenuAction::LockNow] = QObject::tr("Lock");
    actionKeys[MenuAction::AutoLock] = QObject::tr("Auto lock");
    actionKeys[MenuAction::Never] = QObject::tr("Never");
    actionKeys[MenuAction::FiveMinutes] = QObject::tr("5 minutes");
    actionKeys[MenuAction::TenMinutes] = QObject::tr("10 minutes");
    actionKeys[MenuAction::TwentyMinutes] = QObject::tr("20 minutes");
    actionKeys[MenuAction::DeleteVault] = QObject::tr("Delete File Vault");
    actionKeys[MenuAction::UnLock] = QObject::tr("Unlock");
    actionKeys[MenuAction::UnLockByKey] = QObject::tr("Unlock by key");

    // Action Icons:
    DGioSettings settings("com.deepin.dde.filemanager.general", "/com/deepin/dde/filemanager/general/");
    if (settings.value("context-menu-icons").toBool()) {
        actionIcons[MenuAction::NewFolder] = QIcon::fromTheme("folder-new");
        actionIcons[MenuAction::NewDocument] = QIcon::fromTheme("document-new");
        actionIcons[MenuAction::OpenInNewWindow] = QIcon::fromTheme("window-new");
        actionIcons[MenuAction::OpenInNewTab] = QIcon::fromTheme("tab-new");
        actionIcons[MenuAction::OpenInTerminal] = QIcon::fromTheme("utilities-terminal");
        actionIcons[MenuAction::AddToBookMark] = QIcon::fromTheme("bookmark-new");
        actionIcons[MenuAction::BookmarkRemove] = QIcon::fromTheme("bookmark-remove");
        actionIcons[MenuAction::Copy] = QIcon::fromTheme("edit-copy");
        actionIcons[MenuAction::Paste] = QIcon::fromTheme("edit-paste");
        actionIcons[MenuAction::Cut] = QIcon::fromTheme("edit-cut");
        actionIcons[MenuAction::Rename] = QIcon::fromTheme("edit-rename");
        actionIcons[MenuAction::Delete] = QIcon::fromTheme("edit-delete");
        actionIcons[MenuAction::CompleteDeletion] = QIcon::fromTheme("edit-delete-shred");
        actionIcons[MenuAction::Share] = QIcon::fromTheme("document-share");
        actionIcons[MenuAction::SelectAll] = QIcon::fromTheme("edit-select-all");
        actionIcons[MenuAction::CreateSymlink] = QIcon::fromTheme("insert-link");
        actionIcons[MenuAction::Property] = QIcon::fromTheme("document-properties");
    }

    actionKeys[MenuAction::RemoveStashedRemoteConn] = QObject::tr("Remove");
    actionKeys[MenuAction::RefreshView] = QObject::tr("Refresh");
}

void DFileMenuData::initActions()
{
    QList<MenuAction> unCachedActions;
    unCachedActions << MenuAction::NewWindow << MenuAction::RefreshView;
    foreach (MenuAction key, actionKeys.keys()) {
        if (unCachedActions.contains(key)) {
            continue;
        }

        ///###: MenuAction::TagFilesUseColor represents the button for tagging files.
        ///###: MenuAction::ChangeTagColor represents that you change the color of a present tag.
        ///###: They are different event.
        if (key == MenuAction::TagFilesUseColor || key == MenuAction::ChangeTagColor) {
//????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
//??????????????????????????????????????????????????????????????????????????????????????????
//?????????color????????????????????????????????????????????????
//            DTagActionWidget *tagWidget{ new DTagActionWidget };
//            QWidgetAction *tagAction{ new QWidgetAction{ nullptr } };

//            tagAction->setDefaultWidget(tagWidget);

//            switch (key) {
//            case MenuAction::TagFilesUseColor: {
//                tagAction->setText("Add color tags");
//                break;
//            }
//            case MenuAction::ChangeTagColor: {
//                tagAction->setText("Change color of present tag");
//                tagWidget->setExclusive(true);
//                tagWidget->setToolTipVisible(false);
//                break;
//            }
//            default:
//                break;
//            }

//            tagAction->setData(key);
//            actions.insert(key, tagAction);
//            actionToMenuAction.insert(tagAction, key);
            continue;
        }

        QAction *action = new QAction(actionIcons.value(key), actionKeys.value(key), nullptr);
        action->setData(key);
        actions.insert(key, action);
        actionToMenuAction.insert(action, key);
    }

    additionalMenu = new DFMAdditionalMenu();
}

/**
 * @brief ????????????!!! ???????????????ut??????action???, ??????????????????????????????
 * @param
 * @return
 */
void DFileMenuData::clearActions()
{
    for (const auto &menu : actions.keys()) {
        QAction *action = actions.take(menu);
        actionToMenuAction.remove(action);
        delete action;
    }
    actions.clear();
    actionToMenuAction.clear();
}

bool DFileMenuManager::needDeleteAction()
{
    return false;
}

DFileMenu *DFileMenuManager::genereteMenuByKeys(const QVector<MenuAction> &keys,
                                                const QSet<MenuAction> &disableList,
                                                bool checkable,
                                                const QMap<MenuAction, QVector<MenuAction> > &subMenuList, bool isUseCachedAction, bool isRecursiveCall)
{
    static bool actions_initialized = false;

    if (!actions_initialized) {
        actions_initialized = true;
        DFileMenuData::initData();
        DFileMenuData::initActions();
    }

    if (!isUseCachedAction) {
        foreach (MenuAction actionKey, keys) {
            QAction *action = DFileMenuData::actions.take(actionKey);

            if (action) {
                DFileMenuData::actionToMenuAction.remove(action);
            }
            if (needDeleteAction())
                delete action;
        }
    }

    DFileMenu *menu = new DFileMenu();

    if (!isRecursiveCall) {
        connect(menu, &DFileMenu::triggered, fileMenuManger, &DFileMenuManager::actionTriggered);
    }

    foreach (MenuAction key, keys) {
        if (!isAvailableAction(key)) {
            continue;
        }
        /****************************************************************************/
        //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
        //??????????????????????????????????????????????????????????????????????????????????????????
        //?????????color????????????????????????????????????????????????
        if (key == MenuAction::TagFilesUseColor || key == MenuAction::ChangeTagColor) {
            DTagActionWidget *tagWidget{ new DTagActionWidget };
            QWidgetAction *tagAction{ new QWidgetAction{ nullptr } };

            tagAction->setDefaultWidget(tagWidget);

            switch (key) {
            case MenuAction::TagFilesUseColor: {
                tagAction->setText("Add color tags");
                break;
            }
            case MenuAction::ChangeTagColor: {
                tagAction->setText("Change color of present tag");
                tagWidget->setExclusive(true);
                tagWidget->setToolTipVisible(false);
                break;
            }
            default:
                break;
            }

            tagAction->setData(key);
            auto keyAction = DFileMenuData::actions.take(key);
            if (keyAction) {
                QWidgetAction *widAction = dynamic_cast<QWidgetAction *>(keyAction);
                if (widAction && widAction->defaultWidget()) {
                    widAction->defaultWidget()->deleteLater();
                }

                DFileMenuData::actionToMenuAction.remove(keyAction);
                keyAction->deleteLater();
            }
            DFileMenuData::actions.insert(key, tagAction);
            DFileMenuData::actionToMenuAction.insert(tagAction, key);
        }
        /****************************************************************************/
        if (key == MenuAction::Separator) {
            menu->addSeparator();
        } else {
            QAction *action = DFileMenuData::actions.value(key);

            if (!action) {
                action = new QAction(DFileMenuData::actionKeys.value(key), nullptr);
                action->setData(key);
                DFileMenuData::actions[key] = action;
                DFileMenuData::actionToMenuAction[action] = key;
            }

            action->setDisabled(disableList.contains(key));
            action->setProperty("_dfm_menu", QVariant::fromValue(menu));

            menu->addAction(action);

            if (!subMenuList.contains(key)) {
                continue;
            }

            DFileMenu *subMenu = genereteMenuByKeys(subMenuList.value(key), disableList, checkable, QMap<MenuAction, QVector<MenuAction> >(), true, true);

            subMenu->QObject::setParent(menu);
            action->setMenu(subMenu);
        }
    }

    return menu;
}

QString DFileMenuManager::getActionString(MenuAction type)
{
    return DFileMenuData::actionKeys.value(type);
}

//?????????????????????
void DFileMenuManager::extendCustomMenu(DFileMenu *menu, bool isNormal, const DUrl &dir, const DUrl &focusFile, const DUrlList &selected, bool onDesktop)
{
    if (!DFileMenuData::customMenuParser) {
        DFileMenuData::customMenuParser = new DCustomActionParser;
    }

    const QList<DCustomActionEntry> &rootEntry = DFileMenuData::customMenuParser->getActionFiles(onDesktop);
    qDebug() << "extendCustomMenu " << isNormal << dir << focusFile << "files" << selected.size() << "entrys" << rootEntry.size();

    if (menu == nullptr || rootEntry.isEmpty())
        return;

    DCustomActionBuilder builder;
    //????????????????????????
    builder.setActiveDir(dir);

    //???????????????????????????
    DCustomActionDefines::ComboType fileCombo = DCustomActionDefines::BlankSpace;
    if (isNormal) {
        fileCombo = builder.checkFileCombo(selected);
        if (fileCombo == DCustomActionDefines::BlankSpace)
            return;

        //???????????????????????????
        builder.setFocusFile(focusFile);
    }

    //????????????????????????
    auto usedEntrys = builder.matchFileCombo(rootEntry, fileCombo);

    //??????????????????
    usedEntrys = builder.matchActions(selected, usedEntrys);
    qDebug() << "selected combo" << fileCombo << "entry count" << usedEntrys.size();

    if (usedEntrys.isEmpty())
        return;

    //?????????????????????????????????
    {
        QVariant var;
        var.setValue(dir);
        menu->setProperty(DCustomActionDefines::kCustomActionDataDir, var);

        var.setValue(focusFile);
        menu->setProperty(DCustomActionDefines::kCustomActionDataFoucsFile, var);

        var.setValue(selected);
        menu->setProperty(DCustomActionDefines::kCustomActionDataSelectedFiles, var);
    }

    //??????tooltips
    menu->setToolTipsVisible(true);

    //?????????????????????
    auto systemActions = menu->actions();
    for (auto it = systemActions.begin(); it != systemActions.end(); ++it)
        menu->removeAction(*it);
    Q_ASSERT(menu->actions().isEmpty());

    QMap<int, QList<QAction*>> locate;
    QMap<QAction*, DCustomActionDefines::Separator> actionsSeparator;
    //?????????????????????????????????
    for (auto it = usedEntrys.begin(); it != usedEntrys.end(); ++it) {
        const DCustomActionData &actionData = it->data();
        auto *action = builder.buildAciton(actionData, menu);
        if (action == nullptr)
            continue;

        //????????????
        action->setParent(menu);

        //???????????????
        if (actionData.separator() != DCustomActionDefines::None)
            actionsSeparator.insert(action, actionData.separator());

        //????????????????????????????????????
        auto pos = actionData.position(fileCombo);

        //??????????????????
        if (pos > 0) {
            auto temp = locate.find(pos);
            if (temp == locate.end()) {
                locate.insert(pos, {action});
            }
            else { //????????????????????????
                temp->append(action);
            }
        }
        else {  //????????????????????????????????????
            systemActions.append(action);
        }
    }

    //???????????????????????????
    DCustomActionDefines::sortFunc(locate, systemActions, [menu](const QList<QAction *> &acs){
        menu->addActions(acs);
    },[](QAction *ac) ->bool {
        return ac && !ac->isSeparator();
    });

    Q_ASSERT(systemActions.isEmpty());

    //???????????????
    for (auto it = actionsSeparator.begin(); it != actionsSeparator.end(); ++it) {
        //????????????
        if (it.value() & DCustomActionDefines::Top) {
            menu->insertSeparator(it.key());
        }

        //????????????
        if ((it.value() & DCustomActionDefines::Bottom)) {
            const QList<QAction*> &actionList = menu->actions();
            int nextIndex = actionList.indexOf(it.key()) + 1;

            //?????????action
            if (nextIndex < actionList.size()) {
                auto nextAction = menu->actionAt(nextIndex);

                //????????????????????????
                if (!nextAction->isSeparator()) {
                    menu->insertSeparator(nextAction);
                }
            }
        }

    }
}

/*
 * ?????????????????????????????????????????????
 * U???????????????????????????????????????????????????
 *
 * ????????????????????????????????????????????????
 * ???????????????????????????????????????????????????????????????????????????
 * ???????????????????????????????????????????????????????????????????????????????????????
 * ??????????????????????????????????????????????????????????????????????????????????????????
 */
bool DFileMenuManager::isCustomMenuSupported(const DUrl &viewRootUrl)
{
    const QString &path = viewRootUrl.toLocalFile();
    //U????????????????????????
    if (deviceListener->isBlockFile(path))
        return true;

    DStorageInfo st(path);
    return st.isLocalDevice() //?????? ?????? ?????? smb ftp
            && !viewRootUrl.isUserShareFile() //??????????????????
            && !deviceListener->isFileFromDisc(path) //????????????
            && !viewRootUrl.isVaultFile() //???????????????
            && !viewRootUrl.isTrashFile();//???????????????

}

void DFileMenuManager::addActionWhitelist(MenuAction action)
{
    DFileMenuData::whitelist << action;
}

void DFileMenuManager::setActionWhitelist(const QSet<MenuAction> &actionList)
{
    DFileMenuData::whitelist = actionList;
}

QSet<MenuAction> DFileMenuManager::actionWhitelist()
{
    return DFileMenuData::whitelist;
}

void DFileMenuManager::addActionBlacklist(MenuAction action)
{
    DFileMenuData::blacklist << action;
}

void DFileMenuManager::setActionBlacklist(const QSet<MenuAction> &actionList)
{
    DFileMenuData::blacklist = actionList;
}

QSet<MenuAction> DFileMenuManager::actionBlacklist()
{
    return DFileMenuData::blacklist;
}

bool DFileMenuManager::isAvailableAction(MenuAction action)
{
    const QString &group_name = QStringLiteral("MenuActions");

    // init menu action black list
    const QMetaEnum &action_enum = QMetaEnum::fromType<MenuAction>();

    for (const QString &action_name : DFMApplication::genericObtuselySetting()->value(group_name, "disable").toStringList()) {
        bool ok = false;
        int key = action_enum.keyToValue(action_name.toUtf8(), &ok);

        if (ok && key == action) {
            return false;
        }
    }

    if (DFileMenuData::whitelist.isEmpty()) {
        return !DFileMenuData::blacklist.contains(action);
    }

    return DFileMenuData::whitelist.contains(action) && !DFileMenuData::blacklist.contains(action);
}

void DFileMenuManager::setActionString(MenuAction type, QString actionString)
{
    DFileMenuData::actionKeys.insert(type, actionString);

    QAction *action = new QAction(actionString, nullptr);
    action->setData(type);
    DFileMenuData::actions.insert(type, action);
    DFileMenuData::actionToMenuAction[action] = type;
}

void DFileMenuManager::setActionID(MenuAction type, QString id)
{
    DFileMenuData::actionIDs.insert(type, id);
}

MenuAction DFileMenuManager::registerMenuActionType(QAction *action)
{
    Q_ASSERT(action);

    MenuAction type = DFileMenuData::actionToMenuAction.value(action, MenuAction::Unknow);

    if (type >= MenuAction::UserMenuAction) {
        return type;
    }

    type = DFileMenuData::takeAvailableUserActionType();
    DFileMenuData::actions[type] = action;
    DFileMenuData::actionToMenuAction[action] = type;

    QObject::connect(action, &QAction::destroyed, action, [type] {
        DFileMenuData::recycleUserActionType(type);
    });

    return type;
}

bool DFileMenuManager::whetherShowTagActions(const QList<DUrl> &urls)
{
#ifdef DISABLE_TAG_SUPPORT
    return false;
#endif // DISABLE_TAG_SUPPORT

    for (const DUrl &durl : urls) {
        const DAbstractFileInfoPointer &info = DFileService::instance()->createFileInfo(nullptr, durl);

        if (!info)
            return false;

//        bool temp{ DAnythingMonitorFilter::instance()->whetherFilterCurrentPath(info->toLocalFile().toLocal8Bit()) };

        if (!info->canTag()) {
            return false;
        }

        //???????????????????????????????????? ?????????????????????????????????
        if (info->fileUrl() == DesktopFileInfo::computerDesktopFileUrl()
                || info->fileUrl() == DesktopFileInfo::trashDesktopFileUrl()
                || info->fileUrl() == DesktopFileInfo::homeDesktopFileUrl()) {
            return false;
        }
    }

    return true;
}

void DFileMenuManager::clearActions()
{
    DFileMenuData::clearActions();
}

void DFileMenuManager::actionTriggered(QAction *action)
{
    qDebug() << action << action->data().isValid();
    DFileMenu *menu = qobject_cast<DFileMenu *>(sender());
    if (!(menu->property("ToolBarSettingsMenu").isValid() && menu->property("ToolBarSettingsMenu").toBool())) {
        disconnect(menu, &DFileMenu::triggered, fileMenuManger, &DFileMenuManager::actionTriggered);
    }

    //????????????
    if (action->property(DCustomActionDefines::kCustomActionFlag).isValid()) {
        QString cmd = action->property(DCustomActionDefines::kCustomActionCommand).toString();
        DCustomActionDefines::ActionArg argFlag = static_cast<DCustomActionDefines::ActionArg>
                    (action->property(DCustomActionDefines::kCustomActionCommandArgFlag).toInt());
        DUrl dir = menu->property(DCustomActionDefines::kCustomActionDataDir).value<DUrl>();
        DUrl foucs = menu->property(DCustomActionDefines::kCustomActionDataFoucsFile).value<DUrl>();
        DUrlList selected = menu->property(DCustomActionDefines::kCustomActionDataSelectedFiles).value<DUrlList>();

        qDebug() << "argflag" << argFlag << "dir" << dir << "foucs" << foucs << "selected" << selected;
        qInfo() << "extend" << action->text() << cmd;

        QPair<QString, QStringList> runable = DCustomActionBuilder::makeCommand(cmd,argFlag,dir,foucs,selected);
        qInfo () << "exec:" << runable.first << runable.second;

        if (!runable.first.isEmpty())
            FileUtils::runCommand(runable.first, runable.second);
        return;
    }

    if (action->data().isValid()) {
        bool flag = false;
        int _type = action->data().toInt(&flag);
        MenuAction type;
        if (flag) {
            type = (MenuAction)_type;
        } else {
            qDebug() << action->data().toString();;
            return;
        }

        if (type >= MenuAction::UserMenuAction) {
            return;
        }

        if (menu->ignoreMenuActions().contains(type)) {
            return;
        }

        QAction *typeAction = DFileMenuData::actions.value(type);
        qDebug() << typeAction << action;
        if (typeAction) {
            qDebug() << typeAction->text() << action->text();
            if (typeAction->text() == action->text()) {
                const QSharedPointer<DFMMenuActionEvent> &event = menu->makeEvent(type);

                // fix bug 39754 ???sp3????????????????????????????????????5.2.0.8-1?????????????????????????????????????????????????????????????????????????????????-?????????????????????????????????
                // ?????????????????????????????? url ??????????????????????????????????????????????????????????????????????????????
                const DUrlList &selUrls = event->selectedUrls();
                if (selUrls.count() > 0) {
                    const DUrl &u = selUrls.first();
                    // fix bug 60949 ??????url?????????????????????????????????rootpath?????????????????????,???????????????isLowSpeedDevice??????true
                    // ??????????????????????????????
                    QString path  = u.path();
                    if (u.scheme() == DFMROOT_SCHEME && path.endsWith(SUFFIX_GVFSMP)) {
                        path = QUrl::fromPercentEncoding(u.path().toUtf8());
                        path = path.startsWith("//") ? path.mid(1) : path;
                    }
                    if (u.isValid() && !DStorageInfo::isLowSpeedDevice(path)) { // ??????????????????????????????????????????????????????????????????????????????????????????????????????
                        DAbstractFileInfoPointer info = fileService->createFileInfo(nullptr, u);
                        if (info && !info->exists())
                            return;
                    }
                }

                //?????????????????????????????????????????????????????????menu.exec?????????????????????????????????????????????????????????
                //?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
                if (type == MenuAction::TagFilesUseColor) {
                    //????????????????????????menu??????????????????????????????????????????????????????????????????????????????event???
                    //fileeventprocessor????????????event????????????
                    QAction *action{ menu->actionAt("Add color tags") };
                    if (QWidgetAction *widgetAction = qobject_cast<QWidgetAction *>(action)) {
                        if (DTagActionWidget *tagWidget = qobject_cast<DTagActionWidget *>(widgetAction->defaultWidget())) {
                            QList<QColor> colors{ tagWidget->checkedColorList() };
                            event->setTagColors(colors);
                            DFMEventDispatcher::instance()->processEventAsync(event);
                        }
                    }
                } else {
                    DFMEventDispatcher::instance()->processEvent(event);
                }
            }
        }

#ifdef SW_LABEL
        if (DFileMenuData::actionIDs.contains(type)) {
            const QSharedPointer<DFMMenuActionEvent> &menuActionEvent = menu->makeEvent(type);
            DFMEvent event;
            event.setWindowId(menuActionEvent->windowId());
            event.setData(menuActionEvent->fileUrl());
            QMetaObject::invokeMethod(appController,
                                      "actionByIds",
                                      Qt::DirectConnection,
                                      Q_ARG(DFMEvent, event), Q_ARG(QString, DFileMenuData::actionIDs.value(type)));
        }
#endif
    }
}
