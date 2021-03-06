#include "filesystemmanager.h"

#include <QDirIterator>
#include <QtConcurrent/QtConcurrent>

FileSystemManager::FileSystemManager(QObject *parent) : QObject{parent} {}

FileSystemManager::~FileSystemManager() {
    delete m_GetInnerFilesWatcher;
    delete m_GetRootElementSizeWatcher;
    delete m_CancelationWatcher;
    delete m_GetInnerFilesFuture;
    delete m_GetRootElementSizeFuture;
    delete m_CancelationFuture;
}

void FileSystemManager::generateFileTreeAsync(const QString &rootPath,
                                              uint recursionDepth) {
    QString normalizedRootPath = QDir::cleanPath(rootPath);
    if (!QFile::exists(normalizedRootPath))
        return;

    QDir currentDir(normalizedRootPath);
    m_FileTreeRoot = new FileTreeElement("", 0, QDate(), nullptr);
    m_FilesystemRootElement =
            new FileTreeElement(normalizedRootPath, 0, QDate(), m_FileTreeRoot);
    m_FileTreeRoot->appendChild(m_FilesystemRootElement);
    m_FilesystemRootElement->setAbsoluteFilePath(normalizedRootPath);

    // getting inner files of root
    if (m_GetInnerFilesFuture != nullptr) {
        delete m_GetInnerFilesFuture;
        m_GetInnerFilesFuture = nullptr;
    }
    if (m_GetInnerFilesWatcher != nullptr) {
        delete m_GetInnerFilesWatcher;
        m_GetInnerFilesWatcher = nullptr;
    }
    m_GetInnerFilesFuture = new QFuture<FileTreeElement *>();
    m_GetInnerFilesWatcher = new QFutureWatcher<FileTreeElement *>();
    *m_GetInnerFilesFuture =
            QtConcurrent::run(&FileSystemManager::getInnerFilesAsync, this,
                              currentDir, m_FilesystemRootElement, recursionDepth);
    QObject::connect(m_GetInnerFilesWatcher,
                     &QFutureWatcher<FileTreeElement *>::finished, this,
                     &FileSystemManager::handleGetInnerFilesFinished);
    m_GetInnerFilesWatcher->setFuture(*m_GetInnerFilesFuture);

    // getting root directory size
    if (m_GetRootElementSizeFuture != nullptr) {
        delete m_GetRootElementSizeFuture;
        m_GetRootElementSizeFuture = nullptr;
    }
    if (m_GetRootElementSizeWatcher != nullptr) {
        delete m_GetRootElementSizeWatcher;
        m_GetRootElementSizeWatcher = nullptr;
    }
    m_GetRootElementSizeFuture = new QFuture<quint64>();
    m_GetRootElementSizeWatcher = new QFutureWatcher<quint64>();
    *m_GetRootElementSizeFuture =
            QtConcurrent::run(&FileSystemManager::getDirectorySizeAsync, this,
                              currentDir.absolutePath());
    connect(m_GetRootElementSizeWatcher,
            &QFutureWatcher<FileTreeElement *>::finished, this,
            &FileSystemManager::handleGetRootElementSizeFinished);
    m_GetRootElementSizeWatcher->setFuture(*m_GetRootElementSizeFuture);
}

QList<FileTreeElement *>
FileSystemManager::getInnerFiles(QPromise<FileTreeElement *> &promise,
                                 bool &outWasCanceled, const QDir &currenDir,
                                 FileTreeElement *parent, uint recursionDepth,
                                 uint currentRecursionDepth) {
    qDebug() << "FileSystemManager: current recursion depth:"
             << currentRecursionDepth;
    QList<FileTreeElement *> innerFiles;
    bool wasCanceled = false;
    if (currentRecursionDepth <= recursionDepth) {
        ++currentRecursionDepth;
        for (auto &fileElement : currenDir.entryInfoList(
                 QDir::NoDot | QDir::NoDotDot | QDir::AllEntries)) {
            if (promise.isCanceled()) {
                wasCanceled = true;
                break;
            }
            FileTreeElement *fileTreeElement =
                    new FileTreeElement(fileElement.fileName(), fileElement.size(),
                                        fileElement.lastModified().date(), parent);

            if (fileElement.isDir()) {
                QList<FileTreeElement *> innerFiles = getInnerFiles(
                            promise, wasCanceled, QDir(fileElement.absoluteFilePath()),
                            fileTreeElement, recursionDepth, currentRecursionDepth);
                if (wasCanceled) {
                    break;
                }
                fileTreeElement->setChildElements(innerFiles);
                fileTreeElement->setFileSize(
                            getDirectorySize(fileElement.absoluteFilePath()));
            }
            fileTreeElement->setAbsoluteFilePath(fileElement.absoluteFilePath());
            innerFiles.append(fileTreeElement);
        }
    }
    if (wasCanceled) {
        innerFiles.clear();
    }
    outWasCanceled = wasCanceled;
    return innerFiles;
}

void FileSystemManager::getInnerFilesAsync(QPromise<FileTreeElement *> &promise,
                                           const QDir &currenDir,
                                           FileTreeElement *parent,
                                           uint recursionDepth) {
    qDebug() << "Getting inner files in thread:" << QThread::currentThread();
    int counter = 0;
    for (auto &fileElement : currenDir.entryInfoList(
             QDir::NoDot | QDir::NoDotDot | QDir::AllEntries)) {
        if (promise.isCanceled())
            return;
        FileTreeElement *fileTreeElement =
                new FileTreeElement(fileElement.fileName(), fileElement.size(),
                                    fileElement.lastModified().date(), parent);
        qDebug() << "currently reading:" << fileTreeElement->fileName();

        if (fileElement.isDir()) {
            QList<FileTreeElement *> innerFiles;
            if (recursionDepth > 0) {
                bool wasCanceled;
                innerFiles = getInnerFiles(promise, wasCanceled,
                                           QDir(fileElement.absoluteFilePath()),
                                           fileTreeElement, recursionDepth, 1);
                if (wasCanceled) {
                    break;
                }
                fileTreeElement->setChildElements(innerFiles);
            }
            fileTreeElement->setFileSize(
                        getDirectorySize(fileElement.absoluteFilePath()));
            qDebug() << fileElement.absoluteFilePath();
        }
        fileTreeElement->setAbsoluteFilePath(fileElement.absoluteFilePath());
        promise.addResult(fileTreeElement);
        qDebug() << "files/folders read:" << ++counter;
    }
}

void FileSystemManager::waitForCancelation(QPromise<void> &promise) {
    m_GetRootElementSizeFuture->cancel();
    m_GetInnerFilesFuture->cancel();
    m_GetRootElementSizeFuture->waitForFinished();
    m_GetInnerFilesFuture->waitForFinished();
}

void FileSystemManager::handleGetInnerFilesFinished() {
    QList<FileTreeElement *> results = m_GetInnerFilesFuture->results();
    m_FilesystemRootElement->setChildElements(results);

    m_FileTreeGenerationFlags.setGotInnerFilesFlag(true);
    if (m_FileTreeGenerationFlags.isAllFlagsSet()) {
        emit fileTreeGenerated(m_FileTreeRoot);
        m_FileTreeGenerationFlags.resetFlags();
    }
    qDebug() << QTime::currentTime() << "finished getting inner files";
}

void FileSystemManager::handleGetRootElementSizeFinished() {
    QList<quint64> results = m_GetRootElementSizeFuture->results();
    // in case operation is cancelled before even getting one result
    if (!results.isEmpty()) {
        m_FilesystemRootElement->setFileSize(results.last());
    }
    m_FileTreeGenerationFlags.setGotDirectorySizeFlag(true);
    if (m_FileTreeGenerationFlags.isAllFlagsSet()) {
        emit fileTreeGenerated(m_FileTreeRoot);
        m_FileTreeGenerationFlags.resetFlags();
    }
    qDebug() << QTime::currentTime() << "finished getting root element size";
}

void FileSystemManager::handleWaitForCancelationFinished() {
    emit setupModelCanceled();
}

void FileSystemManager::cancelSetupModelHandler() {
    if (m_CancelationFuture != nullptr) {
        delete m_CancelationFuture;
        m_CancelationFuture = nullptr;
    }
    if (m_CancelationWatcher != nullptr) {
        delete m_CancelationWatcher;
        m_CancelationWatcher = nullptr;
    }
    m_CancelationFuture = new QFuture<void>();
    m_CancelationWatcher = new QFutureWatcher<void>();
    *m_CancelationFuture =
            QtConcurrent::run(&FileSystemManager::waitForCancelation, this);
    QObject::connect(m_CancelationWatcher, &QFutureWatcher<void>::finished, this,
                     &FileSystemManager::handleWaitForCancelationFinished);
    m_CancelationWatcher->setFuture(*m_CancelationFuture);
    qDebug() << QTime::currentTime()
             << "FileSystemManager: cancel setup model handler";
}

quint64 FileSystemManager::getDirectorySize(const QString &directory) {
    quint64 size = 0;
    QDirIterator iterator(directory, QDir::Files | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        size += iterator.fileInfo().size();
    }
    return size;
}

void FileSystemManager::getDirectorySizeAsync(QPromise<quint64> &promise,
                                              const QString &directory) {
    quint64 size = 0;
    QDirIterator iterator(directory, QDir::Files | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        if (promise.isCanceled())
            return;
        iterator.next();
        size += iterator.fileInfo().size();
        promise.addResult(size);
    }
}

bool FileSystemManager::deleteFile(const QString &filename) {
    QFileInfo fileToDeleteInfo(filename);
    if (!fileToDeleteInfo.exists())
        return false;

    bool isRemoved = false;
    if (fileToDeleteInfo.isDir()) {
        QDir directoryToDelete(filename);
        isRemoved = directoryToDelete.removeRecursively();
    } else {
        QFile fileToDelete(filename);
        isRemoved = fileToDelete.remove();
    }

    return isRemoved;
}
