#pragma once

#include "Assets/AssetTypes.h"
#include "Assets/Guid.h"

#include <QMimeData>
#include <QStringList>

struct EditorAssetPayload
{
    AssetKey Key{};
    AssetType Type = AssetType::Unknown;
    QString VirtualPath;
};

inline constexpr const char* SakuraAssetMimeType = "application/x-sakura-asset";

inline QByteArray EncodeEditorAssetPayload(const EditorAssetPayload& Payload)
{
    const QString SubAsset = Payload.Key.HasSubAsset()
        ? QString::fromStdString(Payload.Key.SubAsset->ToString())
        : QString{};
    return QStringList{
        QString::fromStdString(Payload.Key.Asset.ToString()),
        SubAsset,
        QString::number(static_cast<int>(Payload.Type)),
        Payload.VirtualPath}.join('\n').toUtf8();
}

inline bool DecodeEditorAssetPayload(const QMimeData* MimeData, EditorAssetPayload& OutPayload)
{
    if (MimeData == nullptr || !MimeData->hasFormat(SakuraAssetMimeType))
    {
        return false;
    }
    const QStringList Parts = QString::fromUtf8(MimeData->data(SakuraAssetMimeType)).split('\n');
    if (Parts.size() != 4 || !Guid::TryParse(Parts[0].toStdString(), OutPayload.Key.Asset))
    {
        return false;
    }
    if (!Parts[1].isEmpty())
    {
        SubAssetId SubAsset{};
        if (!Guid::TryParse(Parts[1].toStdString(), SubAsset))
        {
            return false;
        }
        OutPayload.Key.SubAsset = SubAsset;
    }
    OutPayload.Type = static_cast<AssetType>(Parts[2].toInt());
    OutPayload.VirtualPath = Parts[3];
    return OutPayload.Key.IsValid();
}
