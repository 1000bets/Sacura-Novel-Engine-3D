#include "PrefabOverridesPanel.h"

#include "Game/Scene.h"
#include "Gameplay/GameObject.h"
#include "Prefab/PrefabLibrary.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QColor>
#include <QFont>
#include <QTreeWidgetItem>

namespace
{
QString KindLabel(PrefabOverrideKind Kind)
{
    switch (Kind)
    {
    case PrefabOverrideKind::ObjectProperty: return QObject::tr("Object");
    case PrefabOverrideKind::ComponentProperty: return QObject::tr("Property");
    case PrefabOverrideKind::AddedComponent: return QObject::tr("Added component");
    case PrefabOverrideKind::RemovedComponent: return QObject::tr("Removed component");
    case PrefabOverrideKind::RemovedObject: return QObject::tr("Removed object");
    case PrefabOverrideKind::ReparentedObject: return QObject::tr("Hierarchy");
    case PrefabOverrideKind::AddedObject: return QObject::tr("Added object");
    }
    return {};
}

QString PrefabName(const AssetId& Prefab)
{
    return QString::fromStdString(PrefabLibrary::Get().GetPrefabDisplayName(Prefab));
}

// "Variant of X" when the prefab's root is an instance of another prefab.
QString RelationText(const AssetId& Prefab)
{
    std::string Error;
    std::shared_ptr<const PrefabGraph> Graph = PrefabLibrary::Get().Resolve(Prefab, Error);
    if (Graph == nullptr)
    {
        return QObject::tr("missing prefab asset");
    }
    for (const PrefabGraphInstance& Instance : Graph->Instances)
    {
        if (Instance.RootObjectId == Graph->RootId)
        {
            return QObject::tr("variant of %1").arg(PrefabName(Instance.Prefab));
        }
    }
    return QObject::tr("prefab");
}
}

void PrefabOverridesPanel::DecorateHierarchyItem(QTreeWidgetItem* Item, GameObject* Object)
{
    if (Item == nullptr || Object == nullptr)
    {
        return;
    }
    QFont Font = Item->font(0);
    if (PrefabUtility::IsOutermostInstanceRoot(Object))
    {
        const bool bMissing = Object->GetPrefabInstance()->IsMissing();
        Item->setForeground(0, QColor(bMissing ? "#e06c75" : "#6fa8ff"));
        Font.setBold(true);
        Item->setToolTip(0, bMissing
            ? QObject::tr("Missing prefab asset")
            : QObject::tr("Prefab instance: %1").arg(PrefabName(Object->GetPrefabInstance()->Prefab)));
    }
    else if (PrefabUtility::IsPartOfPrefabInstance(Object))
    {
        AssetId Nested{};
        const bool bNestedRoot = PrefabUtility::IsNestedPrefabRoot(Object, &Nested);
        Item->setForeground(0, QColor(bNestedRoot ? "#6fa8ff" : "#9cc3ff"));
        Font.setItalic(bNestedRoot);
        if (bNestedRoot)
        {
            Item->setToolTip(0, QObject::tr("Nested prefab: %1").arg(PrefabName(Nested)));
        }
    }
    else if (PrefabUtility::IsAddedObject(Object))
    {
        Item->setForeground(0, QColor("#7fd18b"));
        Item->setToolTip(0, QObject::tr("Added to prefab instance (override)"));
    }
    Item->setFont(0, Font);
}

PrefabOverridesPanel::PrefabOverridesPanel(QWidget* Parent)
    : QWidget(Parent)
{
    setObjectName("PrefabOverridesPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "#PrefabOverridesPanel { background:#26303f; border:1px solid #3c5a86; border-radius:4px; }"
        "#PrefabOverridesPanel QLabel { color:#b9d2ff; }"
        "#PrefabOverridesPanel QPushButton { padding:3px 8px; }");

    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(8, 6, 8, 6);
    Layout->setSpacing(4);

    TitleLabel = new QLabel(this);
    TitleLabel->setStyleSheet("font-weight:700;");
    DetailLabel = new QLabel(this);
    DetailLabel->setWordWrap(true);
    Layout->addWidget(TitleLabel);
    Layout->addWidget(DetailLabel);

    auto* FirstRow = new QHBoxLayout();
    OpenButton = new QPushButton(tr("Open"), this);
    OpenNestedButton = new QPushButton(tr("Open Nested"), this);
    SelectRootButton = new QPushButton(tr("Select Root"), this);
    FirstRow->addWidget(OpenButton);
    FirstRow->addWidget(OpenNestedButton);
    FirstRow->addWidget(SelectRootButton);
    FirstRow->addStretch(1);
    Layout->addLayout(FirstRow);

    auto* SecondRow = new QHBoxLayout();
    OverridesButton = new QPushButton(this);
    RevertAllButton = new QPushButton(tr("Revert All"), this);
    ApplyAllButton = new QPushButton(tr("Apply All"), this);
    UnpackButton = new QPushButton(tr("Unpack"), this);
    SecondRow->addWidget(OverridesButton);
    SecondRow->addWidget(RevertAllButton);
    SecondRow->addWidget(ApplyAllButton);
    SecondRow->addWidget(UnpackButton);
    SecondRow->addStretch(1);
    Layout->addLayout(SecondRow);

    auto* UnpackMenu = new QMenu(UnpackButton);
    UnpackMenu->addAction(tr("Unpack (outermost level)"), this, [this]()
    {
        if (Handlers.Unpack && InstanceRoot.IsValid())
        {
            Handlers.Unpack(InstanceRoot, PrefabUnpackMode::OutermostRoot);
        }
    });
    UnpackMenu->addAction(tr("Unpack Completely"), this, [this]()
    {
        if (Handlers.Unpack && InstanceRoot.IsValid())
        {
            Handlers.Unpack(InstanceRoot, PrefabUnpackMode::Completely);
        }
    });
    UnpackButton->setMenu(UnpackMenu);

    connect(OpenButton, &QPushButton::clicked, this, [this]()
    {
        if (Handlers.OpenPrefab && Prefab.IsValid())
        {
            Handlers.OpenPrefab(Prefab);
        }
    });
    connect(OpenNestedButton, &QPushButton::clicked, this, [this]()
    {
        if (Handlers.OpenPrefab && NestedPrefab.IsValid())
        {
            Handlers.OpenPrefab(NestedPrefab);
        }
    });
    connect(SelectRootButton, &QPushButton::clicked, this, [this]()
    {
        if (Handlers.Select && InstanceRoot.IsValid())
        {
            Handlers.Select(InstanceRoot);
        }
    });
    connect(OverridesButton, &QPushButton::clicked, this, &PrefabOverridesPanel::ShowOverridesDialog);
    connect(RevertAllButton, &QPushButton::clicked, this, [this]()
    {
        if (Handlers.Revert && InstanceRoot.IsValid())
        {
            Handlers.Revert(InstanceRoot, {});
        }
    });
    connect(ApplyAllButton, &QPushButton::clicked, this, [this]()
    {
        if (!Handlers.Apply || !InstanceRoot.IsValid())
        {
            return;
        }
        const auto Answer = QMessageBox::question(
            this,
            tr("Apply All"),
            tr("Apply all overrides to prefab '%1'? Every instance of the prefab will be updated. "
               "This operation writes the asset and cannot be undone.").arg(PrefabName(Prefab)));
        if (Answer == QMessageBox::Yes)
        {
            Handlers.Apply(InstanceRoot, {});
        }
    });
    hide();
}

void PrefabOverridesPanel::SetCallbacks(Callbacks InCallbacks)
{
    Handlers = std::move(InCallbacks);
}

GameObject* PrefabOverridesPanel::ResolveRoot() const
{
    return TargetScene != nullptr && InstanceRoot.IsValid() ? TargetScene->FindByHandle(InstanceRoot) : nullptr;
}

void PrefabOverridesPanel::SetTarget(Scene* InScene, GameObject* Selected, bool bInEditable)
{
    TargetScene = InScene;
    bEditable = bInEditable;
    Selection = Selected != nullptr ? Selected->GetObjectHandle() : ObjectHandle{};
    InstanceRoot = ObjectHandle{};
    Prefab = AssetId{};
    NestedPrefab = AssetId{};

    GameObject* Root = PrefabUtility::GetOutermostInstanceRoot(Selected);
    if (Root == nullptr && PrefabUtility::IsAddedObject(Selected))
    {
        Root = PrefabUtility::GetOutermostInstanceRoot(Selected->GetParent());
    }
    if (Root != nullptr)
    {
        InstanceRoot = Root->GetObjectHandle();
        Prefab = Root->GetPrefabInstance()->Prefab;
    }
    Refresh();
}

void PrefabOverridesPanel::Refresh()
{
    GameObject* Root = ResolveRoot();
    GameObject* Selected = TargetScene != nullptr && Selection.IsValid() ? TargetScene->FindByHandle(Selection) : nullptr;
    if (Root == nullptr || Selected == nullptr)
    {
        hide();
        return;
    }

    const std::shared_ptr<PrefabInstanceData>& Data = Root->GetPrefabInstance();
    const bool bMissing = Data == nullptr || Data->IsMissing();
    NestedPrefab = AssetId{};
    const bool bNested = PrefabUtility::IsNestedPrefabRoot(Selected, &NestedPrefab);

    QString Role = tr("Prefab instance");
    if (Selected != Root)
    {
        Role = PrefabUtility::IsAddedObject(Selected) ? tr("Added to prefab instance") : tr("Part of prefab instance");
    }
    TitleLabel->setText(QStringLiteral("\xE2\x97\x86  ") + PrefabName(Prefab));
    QString Detail = Role + QStringLiteral(" · ") + (bMissing ? tr("missing prefab asset") : RelationText(Prefab));
    if (bNested)
    {
        Detail += QStringLiteral("\n") + tr("Nested prefab: %1").arg(PrefabName(NestedPrefab));
    }
    DetailLabel->setText(Detail);

    size_t OverrideCount = 0;
    if (!bMissing && TargetScene != nullptr)
    {
        std::vector<PrefabOverride> Overrides;
        std::string Error;
        if (PrefabUtility::GetOverrides(*TargetScene, *Root, Overrides, Error))
        {
            OverrideCount = Overrides.size();
        }
    }
    OverridesButton->setText(tr("Overrides (%1)").arg(OverrideCount));
    OverridesButton->setEnabled(!bMissing && OverrideCount > 0);
    RevertAllButton->setEnabled(bEditable && !bMissing && OverrideCount > 0);
    ApplyAllButton->setEnabled(bEditable && !bMissing && OverrideCount > 0);
    UnpackButton->setEnabled(bEditable);
    OpenButton->setEnabled(!bMissing);
    OpenNestedButton->setVisible(bNested);
    SelectRootButton->setVisible(Selected != Root);
    show();
}

void PrefabOverridesPanel::ShowOverridesDialog()
{
    GameObject* Root = ResolveRoot();
    if (Root == nullptr || TargetScene == nullptr)
    {
        return;
    }
    std::vector<PrefabOverride> Overrides;
    std::string Error;
    if (!PrefabUtility::GetOverrides(*TargetScene, *Root, Overrides, Error))
    {
        QMessageBox::warning(this, tr("Overrides"), QString::fromStdString(Error));
        return;
    }

    QDialog Dialog(this);
    Dialog.setWindowTitle(tr("Overrides — %1").arg(PrefabName(Prefab)));
    Dialog.resize(520, 420);
    auto* Layout = new QVBoxLayout(&Dialog);
    auto* Tree = new QTreeWidget(&Dialog);
    Tree->setColumnCount(2);
    Tree->setHeaderLabels({tr("Override"), tr("Kind")});
    Tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    Tree->setRootIsDecorated(false);
    for (size_t Index = 0; Index < Overrides.size(); ++Index)
    {
        auto* Item = new QTreeWidgetItem(Tree, {QString::fromStdString(Overrides[Index].DisplayText), KindLabel(Overrides[Index].Kind)});
        Item->setData(0, Qt::UserRole, static_cast<qulonglong>(Index));
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setCheckState(0, Qt::Checked);
    }
    Layout->addWidget(Tree, 1);

    auto* Buttons = new QDialogButtonBox(&Dialog);
    QPushButton* RevertSelected = Buttons->addButton(tr("Revert Selected"), QDialogButtonBox::ActionRole);
    QPushButton* ApplySelected = Buttons->addButton(tr("Apply Selected"), QDialogButtonBox::ActionRole);
    Buttons->addButton(QDialogButtonBox::Close);
    RevertSelected->setEnabled(bEditable);
    ApplySelected->setEnabled(bEditable);
    Layout->addWidget(Buttons);

    auto CollectChecked = [&]()
    {
        std::vector<PrefabOverride> Checked;
        for (int Row = 0; Row < Tree->topLevelItemCount(); ++Row)
        {
            QTreeWidgetItem* Item = Tree->topLevelItem(Row);
            if (Item->checkState(0) == Qt::Checked)
            {
                Checked.push_back(Overrides[static_cast<size_t>(Item->data(0, Qt::UserRole).toULongLong())]);
            }
        }
        return Checked;
    };
    const ObjectHandle RootHandle = InstanceRoot;
    connect(RevertSelected, &QPushButton::clicked, &Dialog, [&]()
    {
        const std::vector<PrefabOverride> Checked = CollectChecked();
        if (!Checked.empty() && Handlers.Revert && Handlers.Revert(RootHandle, Checked))
        {
            Dialog.accept();
        }
    });
    connect(ApplySelected, &QPushButton::clicked, &Dialog, [&]()
    {
        const std::vector<PrefabOverride> Checked = CollectChecked();
        if (!Checked.empty() && Handlers.Apply && Handlers.Apply(RootHandle, Checked))
        {
            Dialog.accept();
        }
    });
    connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    Dialog.exec();
    Refresh();
}
