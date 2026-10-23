#include "UI/Views/AssetReferenceField.h"
#include "UI/Styles/StudioIcons.h"
#include "Assets/AssetRegistry.h"
#include <QComboBox>
#include <QAbstractItemModel>
#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QVariantMap>

class AssetReferenceNavigation
{
public:
    QPointer<QObject> Owner;
    std::function<void(const AssetKey&, bool)> Callback;
    static AssetReferenceNavigation& Get()
    {
        static AssetReferenceNavigation Navigation;
        return Navigation;
    }
};

void AssetReferenceField::SetNavigation(QObject& Owner, std::function<void(const AssetKey&, bool)> Navigate)
{
    AssetReferenceNavigation::Get().Owner = &Owner;
    AssetReferenceNavigation::Get().Callback = std::move(Navigate);
}

AssetReferenceField::AssetReferenceField(QComboBox& InPicker, const AssetRegistry* InRegistry)
    : QWidget(InPicker.parentWidget()), Picker(&InPicker), Registry(InRegistry)
{
    setProperty("uiRole", "assetReferenceField");
    auto* Row = new QHBoxLayout(this);
    Row->setContentsMargins(0, 0, 0, 0);
    Row->setSpacing(2);
    Row->addWidget(Picker, 1);
    auto AddButton = [this, Row](const QString& Icon, const QString& Caption)
    {
        auto* Button = new QPushButton(EditorUi::MakeFrontendIcon(Icon), {}, this);
        Button->setProperty("uiRole", "assetReferenceAction");
        Button->setFixedSize(26, 28);
        Button->setIconSize(QSize(16, 16));
        Button->setToolTip(Caption);
        Button->setAccessibleName(Caption);
        Row->addWidget(Button);
        return Button;
    };
    Open = AddButton("ExternalLink", tr("Открыть ассет"));
    Browse = AddButton("Search", tr("Выделить в Проводнике"));
    Clear = AddButton("Trash2", tr("Очистить ссылку на ассет"));
    connect(Open, &QPushButton::clicked, this, [this]() { Navigate(true); });
    connect(Browse, &QPushButton::clicked, this, [this]() { Navigate(false); });
    connect(Clear, &QPushButton::clicked, this, [this]() { ClearAsset(); });
    connect(Picker, &QComboBox::currentIndexChanged, this, [this]() { RefreshActions(); });
    connect(Picker->model(), &QAbstractItemModel::dataChanged, this, [this]() { RefreshActions(); });
    Picker->installEventFilter(this);
    Picker->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(Picker, &QWidget::customContextMenuRequested, this, [this](const QPoint& Position)
    {
        QMenu Menu(this);
        auto* OpenAction = Menu.addAction(Open->icon(), Open->toolTip());
        OpenAction->setEnabled(Open->isEnabled());
        auto* BrowseAction = Menu.addAction(Browse->icon(), Browse->toolTip());
        BrowseAction->setEnabled(Browse->isEnabled());
        auto* ClearAction = Menu.addAction(Clear->icon(), Clear->toolTip());
        ClearAction->setEnabled(Clear->isEnabled());
        connect(OpenAction, &QAction::triggered, this, [this]() { Navigate(true); });
        connect(BrowseAction, &QAction::triggered, this, [this]() { Navigate(false); });
        connect(ClearAction, &QAction::triggered, this, [this]() { ClearAsset(); });
        Menu.exec(Picker->mapToGlobal(Position));
    });
    RefreshActions();
}

void AssetReferenceField::SetRegistry(const AssetRegistry* Value)
{
    Registry = Value;
    RefreshActions();
}

AssetKey AssetReferenceField::CurrentAsset() const
{
    AssetKey Key;
    const auto Identity = Picker->currentData(Qt::UserRole + 513).toMap();
    if (!Identity.isEmpty())
    {
        Guid::TryParse(Identity.value("assetId").toString().toStdString(), Key.Asset);
        SubAssetId SubAsset;
        if (Guid::TryParse(Identity.value("subAssetId").toString().toStdString(), SubAsset) && SubAsset.IsValid())
        {
            Key.SubAsset = SubAsset;
        }
        return Key;
    }
    QString Identifier = Picker->currentData().toString();
    if (Identifier.startsWith("character:"))
    {
        Identifier.remove(0, 10);
    }
    Guid::TryParse(Identifier.toStdString(), Key.Asset);
    return Key;
}

void AssetReferenceField::RefreshActions()
{
    const AssetKey Key = CurrentAsset();
    AssetRegistryEntry Entry;
    const bool bAvailable = Registry && Registry->TryResolveKey(Key, Entry);
    Open->setEnabled(bAvailable);
    Browse->setEnabled(bAvailable);
    Clear->setEnabled(Key.IsValid() && Picker->isEnabled());
}

void AssetReferenceField::Navigate(bool bOpen)
{
    auto& Navigation = AssetReferenceNavigation::Get();
    if (Navigation.Owner && Navigation.Callback)
    {
        Navigation.Callback(CurrentAsset(), bOpen);
    }
}

void AssetReferenceField::ClearAsset()
{
    if (!Picker->isEnabled())
    {
        return;
    }
    const QPointer<QComboBox> Guard(Picker);
    Picker->setCurrentIndex(0);
    if (Guard)
    {
        QMetaObject::invokeMethod(Guard, "activated", Qt::DirectConnection, Q_ARG(int, 0));
    }
}

bool AssetReferenceField::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Watched == Picker && Event->type() == QEvent::EnabledChange)
    {
        RefreshActions();
    }
    if (Watched == Picker && Event->type() == QEvent::MouseButtonDblClick)
    {
        const auto* Mouse = static_cast<QMouseEvent*>(Event);
        if (Mouse->button() == Qt::LeftButton && Mouse->position().x() <= Picker->iconSize().width() + 8)
        {
            Navigate(true);
            return true;
        }
    }
    return QWidget::eventFilter(Watched, Event);
}
