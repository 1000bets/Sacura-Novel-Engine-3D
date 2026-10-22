#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRectF>
#include <QStringList>

// Shared authoring/player data. No editor or application instance is required.
namespace WidgetLayout
{
QString NewId();
QStringList Kinds();
QString KindLabel(const QString &Kind);
QStringList Types();
QString TypeLabel(const QString &Type);
QJsonObject Element(const QString &Type);
QJsonObject Create(const QString &Kind, const QString &Name = {});
QJsonObject Defaults();
QJsonObject Resolve(const QJsonObject &Library, const QString &Kind);
QMap<QString, QRectF> Rects(const QJsonObject &Widget, QSizeF Size = {});
QJsonObject Reanchor(const QJsonObject &Layout, QSizeF Parent, QPointF Min, QPointF Max, QPointF Pivot);
QJsonObject IndependentCopy(const QJsonObject &Widget);
bool Validate(const QJsonObject &Library, QString &Error);
bool Read(const QString &Path, QJsonObject &Library, QString &Error);
QString ProjectFile(const QString &ContentRoot);
} // namespace WidgetLayout
