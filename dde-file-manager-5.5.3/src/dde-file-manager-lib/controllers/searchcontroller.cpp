/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     yanghao<yanghao@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
 *             hujianzhong<hujianzhong@uniontech.com>
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

#include "searchcontroller.h"
#include "dfileservices.h"
#include "dfmevent.h"
#include "dfileproxywatcher.h"
#include "private/dabstractfilewatcher_p.h"

#include "models/searchfileinfo.h"
#include "ddiriterator.h"
#include "shutil/dfmregularexpression.h"
#include "shutil/dfmfilelistfile.h"
#include "dfmapplication.h"
#include "dfmstandardpaths.h"
#include "vaultcontroller.h"
#include "app/define.h"
#include "app/filesignalmanager.h"
#include "models/vaultfileinfo.h"

#ifndef DISABLE_QUICK_SEARCH
#include "anything_interface.h"
#endif

#include <DDesktopServices>

#include <QDebug>
#include <QRegularExpression>
#include <QQueue>
#ifdef  FULLTEXTSEARCH_ENABLE
#include "fulltextsearch/fulltextsearch.h"
#endif
class SearchFileWatcherPrivate;
class SearchFileWatcher : public DAbstractFileWatcher
{
public:
    explicit SearchFileWatcher(const DUrl &url, QObject *parent = nullptr);
    ~SearchFileWatcher() override;

    void setEnabledSubfileWatcher(const DUrl &subfileUrl, bool enabled = true) override;

private:
    void addWatcher(const DUrl &url);
    void removeWatcher(const DUrl &url);

    void onFileDeleted(const DUrl &url);
    void onFileAttributeChanged(const DUrl &url);
    void onFileMoved(const DUrl &fromUrl, const DUrl &toUrl);
    void onSubfileCreated(const DUrl &url);
    void onFileModified(const DUrl &url);

    Q_DECLARE_PRIVATE(SearchFileWatcher)
};

class SearchFileWatcherPrivate : public DAbstractFileWatcherPrivate
{
public:
    explicit SearchFileWatcherPrivate(SearchFileWatcher *qq)
        : DAbstractFileWatcherPrivate(qq) {}

    bool start() override;
    bool stop() override;

    QMap<DUrl, DAbstractFileWatcher *> urlToWatcherMap;

    Q_DECLARE_PUBLIC(SearchFileWatcher)
};

SearchFileWatcher::SearchFileWatcher(const DUrl &url, QObject *parent)
    : DAbstractFileWatcher(*new SearchFileWatcherPrivate(this), url, parent)
{

}

SearchFileWatcher::~SearchFileWatcher()
{
    Q_D(SearchFileWatcher);
    d->urlToWatcherMap.clear();
}

void SearchFileWatcher::setEnabledSubfileWatcher(const DUrl &subfileUrl, bool enabled)
{
    DUrl url = subfileUrl;

    url.setSearchedFileUrl(DUrl());

    if (url != fileUrl()) {
        return;
    }

    if (enabled) {
        addWatcher(subfileUrl.searchedFileUrl());
    }
    //??????removeWatcher??????????????????????????????????????????????????????????????????
    //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????? ????????????????????????????????????????????????
    //?????????????????????????????????????????????????????????
    //??????????????????????????????
    //else {
    //    removeWatcher(subfileUrl.searchedFileUrl());
    //}
}

void SearchFileWatcher::addWatcher(const DUrl &url)
{
    Q_D(SearchFileWatcher);

    if (!url.isValid() || d->urlToWatcherMap.contains(url)) {
        return;
    }

    DAbstractFileWatcher *watcher = DFileService::instance()->createFileWatcher(this, url);

    if (!watcher) {
        return;
    }

    watcher->moveToThread(this->thread());
    watcher->setParent(this);

    d->urlToWatcherMap[url] = watcher;

    connect(watcher, &DAbstractFileWatcher::fileAttributeChanged, this, &SearchFileWatcher::onFileAttributeChanged);
    connect(watcher, &DAbstractFileWatcher::fileDeleted, this, &SearchFileWatcher::onFileDeleted);
    connect(watcher, &DAbstractFileWatcher::fileModified, this, &SearchFileWatcher::onFileModified);
    connect(watcher, &DAbstractFileWatcher::fileMoved, this, &SearchFileWatcher::onFileMoved);

    if (d->started) {
        watcher->startWatcher();
    }
}

void SearchFileWatcher::removeWatcher(const DUrl &url)
{
    Q_D(SearchFileWatcher);

    DAbstractFileWatcher *watcher = d->urlToWatcherMap.take(url);

    if (!watcher) {
        return;
    }

    watcher->deleteLater();
}

void SearchFileWatcher::onFileDeleted(const DUrl &url)
{
    removeWatcher(url);

    DUrl newUrl = fileUrl();
    newUrl.setSearchedFileUrl(url);

    emit fileDeleted(newUrl);
}

void SearchFileWatcher::onFileAttributeChanged(const DUrl &url)
{
    DUrl newUrl = fileUrl();
    newUrl.setSearchedFileUrl(url);

    emit fileAttributeChanged(newUrl);
}

void SearchFileWatcher::onFileMoved(const DUrl &fromUrl, const DUrl &toUrl)
{
    DUrl newFromUrl = fileUrl();
    newFromUrl.setSearchedFileUrl(fromUrl);

    DUrl newToUrl = toUrl;
    if (fileUrl().searchTargetUrl().scheme() == toUrl.scheme() && toUrl.path().startsWith(fileUrl().searchTargetUrl().path())) {
        QString keywordPattern = DFMRegularExpression::checkWildcardAndToRegularExpression(fileUrl().searchKeyword());
        const DAbstractFileInfoPointer &info = DFileService::instance()->createFileInfo(this, toUrl);

        QRegularExpression regexp(keywordPattern, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = regexp.match(info->fileDisplayName());
        if (match.hasMatch()) {
            newToUrl = fileUrl();
            newToUrl.setSearchedFileUrl(toUrl);

            /*fix bug34957,??????????????????????????????????????????????????????????????????watcher?????????????????????????????????*/
            if (toUrl.path().contains("/.local/share/Trash/files", Qt::CaseSensitive)) {
                return;
            } else {
                /*fix bug 44187 ?????????????????????????????????????????????????????????????????????watcher????????????????????????????????????????????????*/
                addWatcher(newToUrl);
            }
        }
    }

//    removeWatcher(fromUrl); // fix task 21431 ?????????????????????

    emit fileMoved(newFromUrl, newToUrl);
}

void SearchFileWatcher::onFileModified(const DUrl &url)
{
    DUrl newUrl = fileUrl();
    newUrl.setSearchedFileUrl(url);

    emit fileModified(newUrl);
}

bool SearchFileWatcherPrivate::start()
{
    bool ok = true;

    for (DAbstractFileWatcher *watcher : urlToWatcherMap) {
        ok = ok && watcher->startWatcher();
    }

    started = ok;

    return ok;
}

bool SearchFileWatcherPrivate::stop()
{
    bool ok = true;

    for (DAbstractFileWatcher *watcher : urlToWatcherMap) {
        ok = ok && watcher->stopWatcher();
    }

    started = !ok;

    return ok;
}

class SearchDiriterator : public DDirIterator
{
public:
    SearchDiriterator(const DUrl &url, const QStringList &nameFilters, QDir::Filters filter,
                      QDirIterator::IteratorFlags flags, SearchController *parent);
    ~SearchDiriterator() override;

    DUrl next() override;
    bool hasNext() const override;

    QString fileName() const override;
    DUrl fileUrl() const override;
    const DAbstractFileInfoPointer fileInfo() const override;
    DUrl url() const override;
    void close() override;
    void fullTextSearch(const QString &searchPath) const;

    // fix bug23761 ?????????????????????????????????????????????
    // fix bug60961 ????????????????????????????????????????????????
    bool searchFileIsHidden(const QString &fileName) const;

    SearchController *parent;
    DAbstractFileInfoPointer currentFileInfo;
    mutable QQueue<DUrl> childrens;

    DUrl m_fileUrl;
    DUrl targetUrl;
    QString keyword;
    QRegularExpression regex;
    QStringList m_nameFilters;
    QDir::Filters m_filter;
    QDirIterator::IteratorFlags m_flags;
    mutable QList<DUrl> searchPathList;
    mutable DDirIteratorPointer it;
    mutable bool m_hasIteratorByKeywordOfCurrentIt = false;

#ifndef DISABLE_QUICK_SEARCH
    // ????????????????????????????????????(??????????????????????????????)
    QStringList hasLFTSubdirectories;
    QDBusPendingCallWatcher *dbusWatcher = nullptr;
#endif

#ifdef DISABLE_QUICK_SEARCH
    // ????????????????????????????????????(??????????????????????????????)
    QStringList hasLFTSubdirectories;
//    QDBusPendingCallWatcher *dbusWatcher = nullptr;
#endif

    bool closed = false;
    mutable bool hasExecuteFullTextSearch = false;/*???????????????????????????false????????????????????????true???????????????????????????????????????????????????????????????????????????????????????????????????*/
    mutable bool hasUpdateIndex = false;
    mutable QMap<QString, QSet<QString>> hiddenFileMap; // ??????????????????
};

SearchDiriterator::SearchDiriterator(const DUrl &url, const QStringList &nameFilters,
                                     QDir::Filters filter, QDirIterator::IteratorFlags flags,
                                     SearchController *parent)
    : DDirIterator()
    , parent(parent)
    , m_fileUrl(url)
    , targetUrl(url.searchTargetUrl())
    , keyword(DFMRegularExpression::checkWildcardAndToRegularExpression(url.searchKeyword()))
    , m_nameFilters(nameFilters)
    , m_filter(filter)
    , m_flags(flags)
{
    regex = QRegularExpression(keyword, QRegularExpression::CaseInsensitiveOption);
    searchPathList << targetUrl;

#ifndef DISABLE_QUICK_SEARCH
    if (targetUrl.isLocalFile()) {
        QStorageInfo info(targetUrl.toLocalFile());

        if (info.isValid()) {
            ComDeepinAnythingInterface *interface = new ComDeepinAnythingInterface("com.deepin.anything",
                                                                                       "/com/deepin/anything",
                                                                                       QDBusConnection::systemBus());

            dbusWatcher = new QDBusPendingCallWatcher(interface->hasLFTSubdirectories(info.rootPath()));
            interface->setTimeout(3);
            interface->setParent(dbusWatcher);

            // ??????????????????????????????????????????, ???????????????????????????????????????
            hasLFTSubdirectories.append("/");
            QObject::connect(dbusWatcher, &QDBusPendingCallWatcher::finished,
            dbusWatcher, [this](QDBusPendingCallWatcher * call) {
                QDBusPendingReply<QStringList> result = *call;

                hasLFTSubdirectories = result.value();
                dbusWatcher->deleteLater();
                dbusWatcher = nullptr;
            });
        }
    }
#endif

#ifdef DISABLE_QUICK_SEARCH
//    if (targetUrl.isLocalFile()) {
//        QStorageInfo info(targetUrl.toLocalFile());

//        if (info.isValid()) {
//            ComDeepinAnythingInterface *interface = new ComDeepinAnythingInterface("com.deepin.anything",
//                                                                                   "/com/deepin/anything",
//                                                                                   QDBusConnection::systemBus());

//            dbusWatcher = new QDBusPendingCallWatcher(interface->hasLFTSubdirectories(info.rootPath()));
//            interface->setTimeout(3);
//            interface->setParent(dbusWatcher);

    // ??????????????????????????????????????????, ???????????????????????????????????????
    hasLFTSubdirectories.append("/");
//            QObject::connect(dbusWatcher, &QDBusPendingCallWatcher::finished,
//                             dbusWatcher, [this] (QDBusPendingCallWatcher *call) {
//                QDBusPendingReply<QStringList> result = *call;

//                hasLFTSubdirectories = result.value();
//                dbusWatcher->deleteLater();
//                dbusWatcher = nullptr;
//            });
//        }
//    }
#endif
}

SearchDiriterator::~SearchDiriterator()
{
#ifndef DISABLE_QUICK_SEARCH
    if (dbusWatcher) {
        dbusWatcher->deleteLater();
    }
#endif

#ifdef DISABLE_QUICK_SEARCH
    //FSEARCH CLOSE
#endif
}

DUrl SearchDiriterator::next()
{
    if (!childrens.isEmpty()) {
        const DUrl &url = childrens.dequeue();

        currentFileInfo = DFileService::instance()->createFileInfo(parent, url);

        return url;
    }

    return DUrl();
}
#ifdef  FULLTEXTSEARCH_ENABLE
// ????????????
void SearchDiriterator::fullTextSearch(const QString &searchPath) const
{
    QStringList searchResult = DFMFullTextSearchManager::getInstance()->fullTextSearch(m_fileUrl.searchKeyword(), searchPath);
    for (QString res : searchResult) {
        if (DFMFullTextSearchManager::getInstance()->getSearchState() == JobController::Stoped) {
            return;
        }
        // ?????????????????????
        if (searchFileIsHidden(res)) {
            continue;
        }

        DUrl url = m_fileUrl;
        DUrl realUrl = DUrl::fromUserInput(res);
        // ????????????????????????????????????????????????????????????url???????????????????????????URL
        if (targetUrl.isTrashFile()) {
            realUrl = DUrl::fromTrashFile(realUrl.toLocalFile().remove(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath)));
        }
        // ????????????????????????????????????????????????????????????
        if (VaultController::isVaultFile(res)) {
            realUrl = VaultController::localToVault(res);
        }
        url.setSearchedFileUrl(realUrl);

        if (!childrens.contains(url)) {
            // ??????bug-51754 ????????????????????????????????????????????????????????????
            if (!VaultController::isVaultFile(targetUrl.toLocalFile()) && VaultController::isVaultFile(url.fragment())) {
                continue;
            }
            childrens << url;
        }
    }
}

#endif

bool SearchDiriterator::searchFileIsHidden(const QString &fileName) const
{
    if (!targetUrl.isLocalFile())
        return false;

    auto searchPath = targetUrl.toLocalFile();
    if (!fileName.startsWith(searchPath) || fileName == searchPath)
        return false;

    QFileInfo fileInfo(fileName);
    if (fileInfo.isHidden())
        return true;

    QString fileParentPath = fileInfo.absolutePath();
    DFMFileListFile flf(fileParentPath);

    // ??????.hidden??????????????????????????????????????????????????????????????????
    QFileInfo localHiddenFileInfo(flf.filePath());
    if (!localHiddenFileInfo.exists())
        return searchFileIsHidden(fileParentPath);

    if (hiddenFileMap[fileParentPath].isEmpty()) {
        // ??????.hidden???????????????????????????????????????????????????????????????????????????
        auto hiddenFiles = flf.getHiddenFiles();
        if (!hiddenFiles.isEmpty()) {
            hiddenFileMap[fileParentPath] = hiddenFiles;
        } else {
            return searchFileIsHidden(fileParentPath);
        }
    }

    return hiddenFileMap[fileParentPath].contains(fileInfo.fileName())
           ? true
           : searchFileIsHidden(fileParentPath);
}

bool SearchDiriterator::hasNext() const
{
    if (!childrens.isEmpty()) {
        return true;
    }
#ifdef  FULLTEXTSEARCH_ENABLE
    if (!hasExecuteFullTextSearch && DFMApplication::instance()->genericAttribute(DFMApplication::GA_IndexFullTextSearch).toBool()) {
        DAbstractFileInfoPointer fileInfo = fileService->createFileInfo(nullptr, targetUrl);
        if (fileInfo->isVirtualEntry()) {
            hasExecuteFullTextSearch = true;
            return true;
        }

        QString searchPath = fileInfo->filePath();
        fullTextSearch(searchPath);
        hasExecuteFullTextSearch = true;
    }
#endif
    forever {
        if (closed) {
            return false;
        }

        if (!it) {
            if (searchPathList.isEmpty()) {
                break;
            }

            const DUrl &url = searchPathList.takeAt(0);

            it = DFileService::instance()->createDirIterator(parent, url, m_nameFilters, QDir::NoDotAndDotDot | m_filter, m_flags);

            if (!it) {
                continue;
            }

            m_hasIteratorByKeywordOfCurrentIt = false;

#ifndef DISABLE_QUICK_SEARCH
            if (url.isLocalFile()) { // ??????????????????, ???????????????????????????????????????????????????, ?????????????????????????????????, ?????????anything dbus???????????????
                const QString &file = url.toLocalFile().append("/");

                for (const QString &path : hasLFTSubdirectories) {
                    if (path == "/") {
                        m_hasIteratorByKeywordOfCurrentIt = true;
                        break;
                    }

                    if (file.startsWith(path + "/")) {
                        m_hasIteratorByKeywordOfCurrentIt = true;
                        break;
                    }
                }

                if (m_hasIteratorByKeywordOfCurrentIt)
                    m_hasIteratorByKeywordOfCurrentIt = it->enableIteratorByKeyword(m_fileUrl.searchKeyword());
            } else
#endif
            {
                m_hasIteratorByKeywordOfCurrentIt = it->enableIteratorByKeyword(m_fileUrl.searchKeyword());
            }
        }

        while (it->hasNext()) {
            if (closed) {
                return false;
            }

            it->next();

            DAbstractFileInfoPointer fileInfo = it->fileInfo();
            // fix bug58348 ?????????????????????????????????????????????
            if (!fileInfo || !fileInfo->exists()) {
                continue;
            }

            fileInfo->makeAbsolute();

            //????????????????????????????????????
            // fileInfo->isHidden()????????????????????????????????????searchFileIsHidden??????????????????????????????????????????
            if (fileInfo->isHidden() || searchFileIsHidden(fileInfo->absoluteFilePath())) {
                continue;
            }

            if (m_hasIteratorByKeywordOfCurrentIt) {
                DUrl url = m_fileUrl;
                const DUrl &realUrl = fileInfo->fileUrl();

                url.setSearchedFileUrl(realUrl);
                if (!childrens.contains(url)) {
                    // ??????bug-51754 ????????????????????????????????????????????????????????????
                    if (!VaultController::isVaultFile(targetUrl.toLocalFile()) && VaultController::isVaultFile(url.fragment())) {
                        continue;
                    }
                    childrens << url;
                }

                return true;
            }

            if (fileInfo->isDir() && !fileInfo->isSymLink()) {
                const DUrl &url = fileInfo->fileUrl();

                if (!searchPathList.contains(url)) {
                    //????????????????????????????????????????????????????????????????????????????????????????????????
                    if (!url.path().startsWith("/sys/"))
                        searchPathList << url;
                }
            }

            QRegularExpressionMatch match = regex.match(fileInfo->fileDisplayName());
            if (match.hasMatch()) {
                DUrl url = m_fileUrl;
                const DUrl &realUrl = fileInfo->fileUrl();

//                qDebug() << "search matched url = " << realUrl.path() + "/" + realUrl.fileName();
                url.setSearchedFileUrl(realUrl);
                if (!childrens.contains(url)) {/*??????*/
                    // ??????bug-51754 ????????????????????????????????????????????????????????????
                    if (!VaultController::isVaultFile(targetUrl.toLocalFile()) && VaultController::isVaultFile(url.fragment())) {
                        continue;
                    }
                    childrens << url;
                }

                return true;
            }
        }

        it.clear();
    }
#ifdef  FULLTEXTSEARCH_ENABLE
    if (!hasUpdateIndex && DFMApplication::instance()->genericAttribute(DFMApplication::GA_IndexFullTextSearch).toBool()) {
        DAbstractFileInfoPointer fileInfo = fileService->createFileInfo(nullptr, targetUrl);
        if (fileInfo->isVirtualEntry()) {
            hasUpdateIndex = true;
            return true;
        }

        QString searchPath = fileInfo->filePath();
        if (DFMFullTextSearchManager::getInstance()->updateIndex(searchPath)) {
            fullTextSearch(searchPath);
        }
        hasUpdateIndex = true;
        if (childrens.isEmpty()) {
            return false;
        }
        return true;
    }
#endif
    return false;
}

QString SearchDiriterator::fileName() const
{
    return currentFileInfo ? currentFileInfo->fileName() : QString();
}

DUrl SearchDiriterator::fileUrl() const
{
    return currentFileInfo ? currentFileInfo->fileUrl() : DUrl();
}

const DAbstractFileInfoPointer SearchDiriterator::fileInfo() const
{
    return currentFileInfo;
}

DUrl SearchDiriterator::url() const
{
    return m_fileUrl;
}

void SearchDiriterator::close()
{
    closed = true;
    if (it)
        it->close();
}

SearchController::SearchController(QObject *parent)
    : DAbstractFileController(parent)
{

}

const DAbstractFileInfoPointer SearchController::createFileInfo(const QSharedPointer<DFMCreateFileInfoEvent> &event) const
{
    DUrl url = event->url().searchTargetUrl();

    if (url.isSearchFile()) {
        url.setSearchKeyword(event->url().searchKeyword());
    } else {
        url = event->url();
    }

    return DAbstractFileInfoPointer(new SearchFileInfo(url));
}

bool SearchController::openFileLocation(const QSharedPointer<DFMOpenFileLocation> &event) const
{
    return DFileService::instance()->openFileLocation(event->sender(), realUrl(event->url()));
}

bool SearchController::openFile(const QSharedPointer<DFMOpenFileEvent> &event) const
{
    return DFileService::instance()->openFile(event->sender(), realUrl(event->url()));
}

bool SearchController::openFileByApp(const QSharedPointer<DFMOpenFileByAppEvent> &event) const
{
    return DFileService::instance()->openFileByApp(event->sender(), event->appName(), realUrl(event->url()));
}

bool SearchController::openFilesByApp(const QSharedPointer<DFMOpenFilesByAppEvent> &event) const
{
    return DFileService::instance()->openFilesByApp(event->sender(), event->appName(), realUrlList(event->urlList()), event->isEnter());
}

bool SearchController::writeFilesToClipboard(const QSharedPointer<DFMWriteUrlsToClipboardEvent> &event) const
{
    return DFileService::instance()->writeFilesToClipboard(event->sender(), event->action(), realUrlList(event->urlList()));
}

DUrlList SearchController::moveToTrash(const QSharedPointer<DFMMoveToTrashEvent> &event) const
{
    return DFileService::instance()->moveToTrash(event->sender(), realUrlList(event->urlList()));
}

bool SearchController::restoreFile(const QSharedPointer<DFMRestoreFromTrashEvent> &event) const
{
    return DFileService::instance()->restoreFile(event->sender(), realUrlList(event->urlList()));
}

bool SearchController::deleteFiles(const QSharedPointer<DFMDeleteEvent> &event) const
{
    return DFileService::instance()->deleteFiles(event->sender(), realUrlList(event->urlList()), false, event->silent(), event->force());
}

bool SearchController::renameFile(const QSharedPointer<DFMRenameEvent> &event) const
{
    return DFileService::instance()->renameFile(event->sender(), realUrl(event->fromUrl()), realUrl(event->toUrl()));
}

bool SearchController::setPermissions(const QSharedPointer<DFMSetPermissionEvent> &event) const
{
    DUrl url = event->url();
    /*????????????????????????????????????????????????????????????????????????*/
    if (!url.searchTargetUrl().isEmpty()) {
        return DFileService::instance()->setPermissions(event->sender(), DUrl(url.fragment()), event->permissions());
    }

    return false;
}

bool SearchController::compressFiles(const QSharedPointer<DFMCompressEvent> &event) const
{
    return DFileService::instance()->compressFiles(event->sender(), realUrlList(event->urlList()));
}

bool SearchController::decompressFile(const QSharedPointer<DFMDecompressEvent> &event) const
{
    return DFileService::instance()->decompressFile(event->sender(), realUrlList(event->urlList()));
}

bool SearchController::addToBookmark(const QSharedPointer<DFMAddToBookmarkEvent> &event) const
{
    return DFileService::instance()->addToBookmark(event->sender(), realUrl(event->url()));
}

bool SearchController::removeBookmark(const QSharedPointer<DFMRemoveBookmarkEvent> &event) const
{
    return DFileService::instance()->removeBookmark(nullptr, realUrl(event->url()));
}

bool SearchController::createSymlink(const QSharedPointer<DFMCreateSymlinkEvent> &event) const
{
    return DFileService::instance()->createSymlink(event->sender(), realUrl(event->fileUrl()), event->toUrl(), event->force());
}

bool SearchController::shareFolder(const QSharedPointer<DFMFileShareEvent> &event) const
{
    return DFileService::instance()->shareFolder(event->sender(), realUrl(event->url()),
                                                 event->name(), event->isWritable(), event->allowGuest());
}

bool SearchController::unShareFolder(const QSharedPointer<DFMCancelFileShareEvent> &event) const
{
    return DFileService::instance()->unShareFolder(event->sender(), realUrl(event->url()));
}

bool SearchController::openInTerminal(const QSharedPointer<DFMOpenInTerminalEvent> &event) const
{
    return DFileService::instance()->openInTerminal(event->sender(), realUrl(event->url()));
}

const DDirIteratorPointer SearchController::createDirIterator(const QSharedPointer<DFMCreateDiriterator> &event) const
{
    SearchDiriterator *diriterator = new SearchDiriterator(event->url(), event->nameFilters(),
                                                           event->filters(), event->flags(),
                                                           const_cast<SearchController *>(this));

    return DDirIteratorPointer(diriterator);
}

DAbstractFileWatcher *SearchController::createFileWatcher(const QSharedPointer<DFMCreateFileWatcherEvent> &event) const
{
    if (event->url().searchedFileUrl().isValid()) {
        return nullptr;
    }

    return new SearchFileWatcher(event->url());
}

bool SearchController::setFileTags(const QSharedPointer<DFMSetFileTagsEvent> &event) const
{
    if (!event->url().searchedFileUrl().isValid()) {
        return false;
    }

    QList<QString> tags = event->tags();
    return fileService->setFileTags(this, event->url().searchedFileUrl(), tags);
}

bool SearchController::removeTagsOfFile(const QSharedPointer<DFMRemoveTagsOfFileEvent> &event) const
{
    if (!event->url().searchedFileUrl().isValid()) {
        return false;
    }

    return fileService->removeTagsOfFile(this, event->url().searchedFileUrl(), event->tags());
}

QList<QString> SearchController::getTagsThroughFiles(const QSharedPointer<DFMGetTagsThroughFilesEvent> &event) const
{
    QList<DUrl> list = event->urlList();
    for (DUrl &item : list) {
        item = item.searchedFileUrl();
    }
    return fileService->getTagsThroughFiles(this, list);
}

DUrl SearchController::realUrl(const DUrl &searchUrl)
{
    return searchUrl.searchedFileUrl();
}

DUrlList SearchController::realUrlList(const DUrlList &searchUrls)
{
    DUrlList list;

    for (const DUrl &url : searchUrls) {
        list << realUrl(url);
    }

    return list;
}
