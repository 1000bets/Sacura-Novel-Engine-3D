#pragma once
#include <QJsonObject>
#include <QVector>
#include <functional>

class WidgetDocument
{
  public:
    bool Open(const QString &File, QString &Error);
    bool Save(QString &Error);
    bool Apply(QJsonObject Candidate, QString &Error);
    void Undo();
    void Redo();
    bool IsDirty() const
    {
        return Data != Saved;
    }
    bool IsOpen() const
    {
        return !Path.isEmpty();
    }
    bool CanUndo() const { return !Past.isEmpty(); }
    bool CanRedo() const { return !Future.isEmpty(); }
    const QJsonObject &GetData() const
    {
        return Data;
    }
    bool ExportWidget(const QString &Id, const QString &File, QString &Error, QJsonObject Metadata = {}) const;
    bool ImportWidget(QJsonObject Widget, QString &Id, QString &Error);
    static bool ReadPackage(const QString &File, QJsonObject &Widget, QString &Error);
    std::function<void()> Changed;

  private:
    QString Path;
    QByteArray DiskContents;
    bool Existed = false;
    QJsonObject Data, Saved;
    QVector<QJsonObject> Past, Future;
};
