#ifndef FILETREEELEMENT_H
#define FILETREEELEMENT_H

#include <QVariant>
#include <QList>
#include <QDir>

enum FileTreeElementRole {
    FILE_NAME_ROLE = Qt::UserRole + 1,
    FILE_INNER_FILES_ROLE,
    FILE_SIZE_ROLE,
    FILE_ROLES_SIZE
};

class FileTreeElement
{
private:
    QString m_FileName;
    QString m_AbsoluteFilePath;
    uint m_InnerFilesCount;
    quint64 m_FileSize;
    QDate m_LastModificationDate;
    QList<FileTreeElement *> m_ChildFiles;
    FileTreeElement *m_ParentElement;

public:
    FileTreeElement(const QString &fileName,
                    quint64 fileSize,
                    QDate lastModificationDate,
                    FileTreeElement *parentElement,
                    QList<FileTreeElement *> childFiles = {});

    ~FileTreeElement();

    int getRolesCount() const;
    int getChildsCount() const;
    QVariant getData(int role) const;
    FileTreeElement *getParent() const;
    FileTreeElement *getChildAt(int row);
    void setChildElements(QList<FileTreeElement *> childFiles);
    void appendChild(FileTreeElement *element);
    QList<FileTreeElement *> getChildElements() const;
    const QString &fileName() const;
    void setFileSize(quint64 fileSize);
    quint64 getFileSize() const;
    QString formattedSize() const;
    void setAbsoluteFilePath(QString path);
    QString getAbsoluteFilePath() const;
    void setInnerFilesCount();
    uint getInnerFilesCount() const;
    QString formattedInnerFilesCount() const;
    void setLastModificationDate(QDate lastModificationDate);
    QDate getLastModificationDate();
    int row() const;
    QString getPathPrefix() const;
    void removeChildAt(int index);
    bool hasChildElements() const;
};

#endif // FILETREEELEMENT_H
