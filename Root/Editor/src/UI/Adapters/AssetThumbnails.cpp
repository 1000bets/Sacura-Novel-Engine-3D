#include "Assets/AssetPreview.h"
#include "QVariantMap"
#include "UI/Adapters/AssetThumbnails.h"
#include "UI/Styles/EditorStyle.h"
#include "Assets/AssetRegistry.h"
#include "Assets/Loaders/ModelLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Characters/CharacterDefinition.h"
#include "Materials/MaterialDocumentIO.h"
#include <QAbstractItemModel>
#include <QAction>
#include <QStandardItemModel>
#include <QApplication>
#include <QCache>
#include <QComboBox>
#include <QDateTime>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/ThreadContext.h"
#include <memory>
#include <exception>
#include <QPixmap>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

class AssetThumbnailStore
{
public:
    struct Destination
    {
        QPointer<QAbstractItemModel> Model;
        QPersistentModelIndex Index;
    };
    QCache<QString, QImage> Images{16384};
    uint64_t Generation = 0;
    std::map<QString, std::vector<Destination>> Pending;

    static AssetThumbnailStore& Get()
    {
        static AssetThumbnailStore Store;
        return Store;
    }

    static QImage Background()
    {
        QImage Image(128, 128, QImage::Format_ARGB32);
        Image.fill(QColor(30, 31, 39));
        return Image;
    }

};

void AssetThumbnails::Invalidate()
{
    auto& Store = AssetThumbnailStore::Get();
    ++Store.Generation;
    Store.Images.clear();
}

void AssetThumbnails::Bind(QAbstractItemModel& Model, const QModelIndex& Index, const AssetRegistry& Registry, const AssetKey& Key)
{
    AssetRegistryEntry Entry;
    const SubAssetRecord* SubAsset = nullptr;
    if (!Registry.TryResolveKey(Key, Entry, &SubAsset))
    {
        QImage Image = AssetThumbnailStore::Background();
        QPainter Painter(&Image);
        Painter.setPen(EditorUi::EditorStyle::Color("--text-muted"));
        QString Caption = QApplication::translate("AssetThumbnails", "Пусто");
        if (Key.IsValid())
        {
            Caption = QApplication::translate("AssetThumbnails", "Недоступен");
        }
        Painter.drawText(Image.rect(), Qt::AlignCenter, Caption);
        Painter.end();
        Model.setData(Index, QString{}, Qt::UserRole + 512);
        Model.setData(Index, QIcon(QPixmap::fromImage(Image)), Qt::DecorationRole);
        return;
    }
    const QFileInfo File(QString::fromStdString(Entry.AbsolutePath));
    QString CacheKey = File.absoluteFilePath() + QString::fromStdString(Key.Asset.ToString())
        + QString::number(File.lastModified().toMSecsSinceEpoch()) + QString::number(File.size())
        + QString::fromStdString(AssetMetadataIO::ToJson(Entry.Metadata).dump());
    if (Key.HasSubAsset())
    {
        CacheKey += QString::fromStdString(Key.SubAsset->ToString());
    }
    auto& Store = AssetThumbnailStore::Get();
    CacheKey += ":" + QString::number(Store.Generation);
    Model.setData(Index, CacheKey, Qt::UserRole + 512);
    if (const auto* Cached = Store.Images.object(CacheKey))
    {
        Model.setData(Index, QIcon(QPixmap::fromImage(*Cached)), Qt::DecorationRole);
        return;
    }
    QImage Placeholder = AssetThumbnailStore::Background();
    QPainter Painter(&Placeholder);
    Painter.setPen(EditorUi::EditorStyle::Color("--text-secondary"));
    QFont Font = QApplication::font();
    Font.setPixelSize(12);
    Painter.setFont(Font);
    AssetType Type = Entry.Metadata.Type;
    if (SubAsset)
    {
        Type = SubAsset->Type;
    }
    Painter.drawText(Placeholder.rect().adjusted(6, 6, -6, -6), Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8(AssetTypeToString(Type)));
    Painter.end();
    Model.setData(Index, QIcon(QPixmap::fromImage(Placeholder)), Qt::DecorationRole);
    auto& Destinations = Store.Pending[CacheKey];
    Destinations.push_back({&Model, QPersistentModelIndex(Index)});
    if (Destinations.size() > 1)
    {
        return;
    }
    auto* Jobs = JobSystem::Get();
    if (Jobs == nullptr || !Jobs->IsInitialized())
    {
        Store.Pending.erase(CacheKey);
        return;
    }
    const auto Snapshot = std::make_shared<AssetRegistry>(Registry);
    const uint64_t Generation = Store.Generation;
    Jobs->Schedule([Snapshot, Key, CacheKey, Placeholder, Generation]()
    {
        QImage Image;
        try
        {
            Image = GenerateAssetPreview(*Snapshot, Key);
        }
        catch (const std::exception& Error)
        {
            PrintString(std::string("Asset thumbnail generation failed: ") + Error.what());
        }
        if (Image.isNull())
        {
            Image = Placeholder;
        }
        else
        {
            QImage Framed = AssetThumbnailStore::Background();
            QPainter Painter(&Framed);
            const QSize Size = Image.size().scaled(128, 128, Qt::KeepAspectRatio);
            Painter.drawImage(QRect(QPoint((128 - Size.width()) / 2, (128 - Size.height()) / 2), Size), Image);
            Painter.end();
            Image = Framed;
        }
        QMetaObject::invokeMethod(qApp, [CacheKey, Image, Generation]()
        {
            auto& Store = AssetThumbnailStore::Get();
            if (Store.Generation != Generation)
            {
                Store.Pending.erase(CacheKey);
                return;
            }
            Store.Images.insert(CacheKey, new QImage(Image), 64);
            const auto Found = Store.Pending.find(CacheKey);
            if (Found == Store.Pending.end())
            {
                return;
            }
            const QIcon Icon(QPixmap::fromImage(Image));
            for (const auto& Destination : Found->second)
            {
                if (Destination.Model && Destination.Index.isValid()
                    && Destination.Index.data(Qt::UserRole + 512).toString() == CacheKey)
                {
                    Destination.Model->setData(Destination.Index, Icon, Qt::DecorationRole);
                }
            }
            Store.Pending.erase(Found);
        }, Qt::QueuedConnection);
    });
}

void AssetThumbnails::Bind(QAction& Action, const AssetRegistry& Registry, const AssetKey& Key)
{
    auto* Model = new QStandardItemModel(1, 1, &Action);
    QObject::connect(Model, &QAbstractItemModel::dataChanged, &Action, [&Action, Model]()
    {
        Action.setIcon(qvariant_cast<QIcon>(Model->index(0, 0).data(Qt::DecorationRole)));
    });
    Bind(*Model, Model->index(0, 0), Registry, Key);
}

void AssetThumbnails::Bind(QComboBox& Picker, int Index, const AssetRegistry& Registry, const AssetKey& Key)
{
    QVariantMap Identity;
    Identity.insert("assetId", QString::fromStdString(Key.Asset.ToString()));
    if (Key.HasSubAsset())
    {
        Identity.insert("subAssetId", QString::fromStdString(Key.SubAsset->ToString()));
    }
    Picker.setItemData(Index, Identity, Qt::UserRole + 513);
    Picker.setProperty("assetReference", true);
    Picker.setIconSize(QSize(40, 40));
    Picker.setMinimumHeight(48);
    Bind(*Picker.model(), Picker.model()->index(Index, Picker.modelColumn(), Picker.rootModelIndex()), Registry, Key);
}
