#include "UI/Presentation/LocalizedEditorUi.h"
#include "UI/Styles/StudioIcons.h"
#include "UI/Views/AssetReferenceField.h"
#include "UI/Adapters/AssetThumbnails.h"
#include "UI/Views/StaticMeshEditorWindow.h"
#include "UI/Styles/EditorStyle.h"
#include "Viewport/EditorViewportWidget.h"
#include "Engine.h"
#include "Core/Object/MemorySubsystem.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/LightComponent.h"
#include "World/Components/PhysicsBodyComponent.h"
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QListWidget>
#include <QTabWidget>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

StaticMeshEditorWindow::StaticMeshEditorWindow(Engine& InRuntime, const AssetKey& Asset, QWidget* Parent)
    : QMainWindow(Parent, Qt::Window), Runtime(InRuntime), Document(Runtime.GetAssetRegistry())
{
    setObjectName("StaticMeshEditorWindow");
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
        | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    EditorUi::EditorStyle::Apply(*this);
    resize(1280, 800);
    std::string Error;
    Document.Open(Asset, Error);
    static uint32_t NextView = 30000;
    ViewId = RenderViewId{NextView++};
    auto* Content = new QWidget(this);
    Content->setObjectName("StaticMeshEditorContent");
    setCentralWidget(Content);
    auto* Layout = new QVBoxLayout(Content);
    auto* FileMenu = BindEditorCaption(menuBar()->addMenu(tr("Файл")), QString::fromUtf8("Файл"));
    auto* EditMenu = BindEditorCaption(menuBar()->addMenu(tr("Правка")), QString::fromUtf8("Правка"));
    auto* ViewMenu = BindEditorCaption(menuBar()->addMenu(tr("Вид")), QString::fromUtf8("Вид"));
    auto* Toolbar = new QToolBar(tr("Документ"), this);
    Toolbar->setObjectName("StaticMeshDocumentToolbar");
    Toolbar->setMovable(false);
    Toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    Toolbar->setIconSize(QSize(20, 20));
    addToolBar(Toolbar);
    for (const auto& Action : std::vector<std::pair<QString, QString>>{
        {tr("Сохранить"), "save"}, {tr("Отменить"), "undo"}, {tr("Повторить"), "redo"}, {tr("Перечитать"), "reload"}})
    {
        auto* Command = new QAction(Action.first, this);
        if (Action.second == "save")
        {
            Command->setShortcut(QKeySequence::Save);
            Command->setIcon(EditorUi::MakeFrontendIcon("Save"));
            BindEditorProperty(Command, "toolTip", QString::fromUtf8("Сохранить (Ctrl+S)"));
            Toolbar->addAction(Command);
            FileMenu->addAction(Command);
        }
        else if (Action.second == "reload")
        {
            FileMenu->addAction(Command);
        }
        else
        {
            EditMenu->addAction(Command);
            if (Action.second == "undo")
            {
                Command->setShortcut(QKeySequence::Undo);
            }
            else
            {
                Command->setShortcut(QKeySequence::Redo);
            }
        }
        connect(Command, &QAction::triggered, this, [this, Operation = Action.second]()
        {
            if (Operation == "reload" && Document.IsDirty()
                && QMessageBox::question(this, tr("Перечитать меш"), tr("Отбросить изменения?")) != QMessageBox::Yes)
            {
                return;
            }
            std::string Error;
            Run(Operation, {}, Error);
            Diagnostic->setText(QString::fromStdString(Error));
        });
    }
    FileMenu->addSeparator();
    auto* CloseAction = BindEditorCaption(FileMenu->addAction(tr("Закрыть")), QString::fromUtf8("Закрыть"));
    CloseAction->setShortcut(QKeySequence::Close);
    connect(CloseAction, &QAction::triggered, this, [this]() { close(); });
    auto* ShowCollision = BindEditorCaption(ViewMenu->addAction(tr("Показать коллизию")), QString::fromUtf8("Показать коллизию"));
    ShowCollision->setCheckable(true);
    ShowCollision->setChecked(true);
    auto* Splitter = new QSplitter(this);
    Viewport = new EditorViewportWidget(Splitter);
    Viewport->SetViewId(ViewId);
    Viewport->SetEngine(&Runtime);
    Viewport->SetCollisionPreviewLines({});
    Viewport->SetCollisionVisible(true);
    Viewport->SetEditorToolsEnabled(true);
    Viewport->SetTransformCallbacks([this](const Transform& Value) { PreviewTransform(Value); },
        [this](const Transform&, const Transform&) { CommitTransform(); });
    connect(Viewport, &EditorViewportWidget::TransformCancelled, this, &StaticMeshEditorWindow::CancelTransform);
    connect(Viewport, &EditorViewportWidget::ObjectSelectionRequested, this, &StaticMeshEditorWindow::PickCollision);
    auto* ToolRow = new QHBoxLayout();
    Layout->addLayout(ToolRow);
    auto* Modes = new QButtonGroup(this);
    int OperationIndex = 0;
    for (const QString& Title : {tr("Перемещение (W)"), tr("Поворот (E)"), tr("Размер (R)"), tr("Выбор (Q)")})
    {
        const std::vector<EditorUi::StudioIcon> Icons{
            EditorUi::StudioIcon::Move, EditorUi::StudioIcon::Rotate,
            EditorUi::StudioIcon::Scale, EditorUi::StudioIcon::Select};
        auto* Button = new QPushButton(EditorUi::MakeStudioIcon(Icons[OperationIndex]), {}, this);
        Button->setProperty("uiRole", "viewportTool");
        Button->setFixedSize(28, 26);
        Button->setIconSize(QSize(14, 14));
        Button->setToolTip(Title);
        Button->setAccessibleName(Title);
        Button->setAutoDefault(false);
        Button->setCheckable(true);
        Modes->addButton(Button, OperationIndex++);
        ToolRow->addWidget(Button);
    }
    ToolRow->addStretch();
    Modes->button(0)->setChecked(true);
    connect(Modes, &QButtonGroup::idClicked, this, [this](int Index)
    {
        Viewport->SetGizmoOperation(static_cast<EditorViewportWidget::GizmoOperation>(Index));
    });
    connect(Viewport, &EditorViewportWidget::GizmoOperationChanged, this, [Modes](EditorViewportWidget::GizmoOperation Operation)
    {
        Modes->button(static_cast<int>(Operation))->setChecked(true);
    });
    auto* FrameToolbar = new QHBoxLayout();
    for (bool bMesh : {true, false})
    {
        QString Label = tr("Выбранная форма (F)");
        if (bMesh)
        {
            Label = tr("Меш целиком (Home)");
        }
        auto* Button = new QPushButton(Label, this);
        Button->setAutoDefault(false);
        FrameToolbar->addWidget(Button);
        connect(Button, &QPushButton::clicked, this, [this, bMesh]() { FrameSelection(bMesh); });
        auto* FrameAction = ViewMenu->addAction(Label);
        connect(FrameAction, &QAction::triggered, this, [this, bMesh]() { FrameSelection(bMesh); });
        QKeySequence Key(Qt::Key_F);
        if (bMesh)
        {
            Key = QKeySequence(Qt::Key_Home);
        }
        auto* Shortcut = new QShortcut(Key, Viewport);
        Shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(Shortcut, &QShortcut::activated, this, [this, bMesh]() { FrameSelection(bMesh); });
    }
    auto* Help = BindEditorCaption(new QLabel(tr("ПКМ + WASD/QE — полёт; Alt + ЛКМ — орбита; СКМ — сдвиг; колесо — приближение"), this), QString::fromUtf8("ПКМ + WASD/QE — полёт; Alt + ЛКМ — орбита; СКМ — сдвиг; колесо — приближение"));
    Help->setWordWrap(true);
    FrameToolbar->addWidget(Help, 1);
    Layout->addLayout(FrameToolbar);
    ViewMenu->addSeparator();
    ViewMenu->addAction(Toolbar->toggleViewAction());
    connect(ShowCollision, &QAction::toggled, Viewport, &EditorViewportWidget::SetCollisionVisible);
    Viewport->setMinimumSize(320, 300);
    Tabs = new QTabWidget(Splitter);
    auto* Scroll = new QScrollArea(Tabs);
    Scroll->setWidgetResizable(true);
    auto* Container = new QWidget(Scroll);
    Fields = new QFormLayout(Container);
    Fields->setRowWrapPolicy(QFormLayout::WrapLongRows);
    Fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    Scroll->setWidget(Container);
    Tabs->addTab(Scroll, tr("Коллизия"));
    auto* MaterialScroll = new QScrollArea(Tabs);
    MaterialScroll->setWidgetResizable(true);
    auto* MaterialContainer = new QWidget(MaterialScroll);
    MaterialFields = new QFormLayout(MaterialContainer);
    MaterialScroll->setWidget(MaterialContainer);
    Tabs->addTab(MaterialScroll, tr("Материалы"));
    Tabs->setMinimumWidth(330);
    Splitter->setSizes({870, 370});
    Layout->addWidget(Splitter, 1);
    Diagnostic = new QLabel(QString::fromStdString(Error), this);
    Diagnostic->setWordWrap(true);
    Layout->addWidget(Diagnostic);
    PreviewScene = MemorySubsystem::Get()->NewObject<Scene>();
    PreviewScene->SetAssetManager(&Runtime.GetAssetManager());
    auto* Light = PreviewScene->CreateGameObject("Preview Light");
    Transform LightTransform;
    LightTransform.Rotation = Quaternion::CreateFromYawPitchRoll(-0.5f, -0.7f, 0.f);
    Light->SetTransform(LightTransform);
    Light->SetVisual(false);
    Light->AddComponent<LightComponent>()->Intensity = 2.f;
    PreviewMesh = PreviewScene->CreateGameObject("Static Mesh Preview")->AddComponent<MeshRendererComponent>();
    PreviewMesh->SetMeshAssetKey(Asset);
    PreviewCollision = PreviewMesh->GetGameObject()->AddComponent<PhysicsBodyComponent>();
    Viewport->SetPreviewScene(PreviewScene);
    connect(Viewport, &RenderViewportWidget::NativeSurfaceChanged, this, [this](RenderViewportWidget* Surface)
    {
        if (!Surface->HasValidNativeHandle())
        {
            Runtime.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
        }
    });
    connect(Viewport, &RenderViewportWidget::ViewportResized, this, [this](RenderViewportWidget* Surface)
    {
        const auto Info = Surface->BuildNativeWindowInfo();
        Runtime.ResizeRenderSurface(RenderSurfaceId{ViewId.Value}, Info.Width, Info.Height);
    });
    auto* Timer = new QTimer(this);
    connect(Timer, &QTimer::timeout, this, [this]() { TickPreview(); });
    Timer->start(16);
    RebuildFields();
}

StaticMeshEditorWindow::~StaticMeshEditorWindow()
{
    Runtime.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
    Viewport->SetPreviewScene(nullptr);
    MemorySubsystem::Get()->DestroyObject(PreviewScene);
}

bool StaticMeshEditorWindow::Run(const QString& Operation, const nlohmann::json& Value, std::string& Error)
{
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        Error = "Stop Play before editing mesh defaults";
        return false;
    }
    CancelTransform();
    bool bResult = false;
    if (Operation == "edit")
    {
        bResult = Document.Edit(Value, Error);
    }
    else if (Operation == "save")
    {
        bResult = Document.Save(Runtime.GetAssetManager(), Error);
    }
    else if (Operation == "undo")
    {
        bResult = Document.Undo();
    }
    else if (Operation == "redo")
    {
        bResult = Document.Redo();
    }
    else if (Operation == "reload")
    {
        Runtime.GetAssetRegistry().ScanContent();
        const auto Asset = Document.GetAsset();
        bResult = Document.Open(Asset, Error);
    }
    else
    {
        Error = "Unknown mesh document operation";
    }
    RebuildFields();
    return bResult;
}

void StaticMeshEditorWindow::Change(const nlohmann::json& Value)
{
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        Diagnostic->setText(tr("Остановите Play перед изменением меша."));
        return;
    }
    CancelTransform();
    std::string Error;
    Document.Edit(Value, Error);
    Diagnostic->setText(QString::fromStdString(Error));
    SyncSelection();
}

void StaticMeshEditorWindow::RebuildFields()
{
    while (Fields->rowCount() > 0)
    {
        Fields->removeRow(0);
    }
    while (MaterialFields->rowCount() > 0)
    {
        MaterialFields->removeRow(0);
    }
    const auto& Defaults = Document.GetDefaults();
    if (SelectedCollisionElement >= Defaults.CollisionElements.size())
    {
        SelectedCollisionElement = 0;
    }
    Elements = new QListWidget(this);
    Elements->setFixedHeight(std::clamp(static_cast<int>(Defaults.CollisionElements.size()) * 28 + 12, 100, 170));
    Elements->setObjectName("StaticMeshCollisionElements");
    const QStringList ShapeNames{tr("Параллелепипед"), tr("Сфера"), tr("Капсула"), tr("Треугольники меша")};
    for (size_t Index = 0; Index < Defaults.CollisionElements.size(); ++Index)
    {
        Elements->addItem(tr("Элемент %1 — %2").arg(Index + 1).arg(ShapeNames[Defaults.CollisionElements[Index].ShapeType]));
    }
    Elements->setCurrentRow(static_cast<int>(SelectedCollisionElement));
    Fields->addRow(BindEditorCaption(new QLabel(tr("Элементы коллизии"), this), QString::fromUtf8("Элементы коллизии")));
    Fields->addRow(Elements);
    connect(Elements, &QListWidget::currentRowChanged, this, [this](int Index)
    {
        if (Index >= 0)
        {
            CancelTransform();
            SelectedCollisionElement = static_cast<size_t>(Index);
            SyncSelection();
            QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
        }
    });
    auto* ElementActions = new QWidget(this);
    auto* ElementLayout = new QHBoxLayout(ElementActions);
    ElementLayout->setContentsMargins(0, 0, 0, 0);
    auto* AddElement = BindEditorCaption(new QPushButton(tr("Добавить"), ElementActions), QString::fromUtf8("Добавить"));
    AddElement->setObjectName("StaticMeshAddCollisionElement");
    ElementLayout->addWidget(AddElement);
    auto* DuplicateElement = BindEditorCaption(new QPushButton(tr("Дублировать"), ElementActions), QString::fromUtf8("Дублировать"));
    DuplicateElement->setEnabled(!Defaults.CollisionElements.empty());
    ElementLayout->addWidget(DuplicateElement);
    auto* RemoveElement = BindEditorCaption(new QPushButton(tr("Удалить"), ElementActions), QString::fromUtf8("Удалить"));
    RemoveElement->setObjectName("StaticMeshRemoveCollisionElement");
    RemoveElement->setEnabled(!Defaults.CollisionElements.empty());
    ElementLayout->addWidget(RemoveElement);
    Fields->addRow(ElementActions);
    auto* AddMenu = new QMenu(AddElement);
    AddMenu->setObjectName("StudioActionMenu");
    AddElement->setMenu(AddMenu);
    for (int ShapeIndex = 0; ShapeIndex < ShapeNames.size(); ++ShapeIndex)
    {
        auto* Action = AddMenu->addAction(ShapeNames[ShapeIndex]);
        connect(Action, &QAction::triggered, this, [this, ShapeIndex]()
        {
            auto Defaults = Document.GetDefaults();
            StaticMeshCollisionElement Element;
            Element.ShapeType = ShapeIndex;
            Defaults.CollisionElements.push_back(Element);
            Defaults.bCollisionEnabled = true;
            SelectedCollisionElement = Defaults.CollisionElements.size() - 1;
            Change(Defaults.ToJson());
            QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
        });
    }
    connect(DuplicateElement, &QPushButton::clicked, this, [this]()
    {
        auto Defaults = Document.GetDefaults();
        const auto Element = Defaults.CollisionElements.at(SelectedCollisionElement);
        Defaults.CollisionElements.push_back(Element);
        SelectedCollisionElement = Defaults.CollisionElements.size() - 1;
        Change(Defaults.ToJson());
        QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
    });
    connect(RemoveElement, &QPushButton::clicked, this, [this]()
    {
        auto Defaults = Document.GetDefaults();
        Defaults.CollisionElements.erase(Defaults.CollisionElements.begin() + SelectedCollisionElement);
        if (Defaults.CollisionElements.empty())
        {
            Defaults.bCollisionEnabled = false;
        }
        Change(Defaults.ToJson());
        QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
    });
    auto* Enabled = new QCheckBox(this);
    BindEditorProperty(Enabled, "toolTip", QString::fromUtf8("Включает физическую коллизию у объектов, использующих настройки меша. Формы остаются видимыми для редактирования."));
    Enabled->setChecked(Defaults.bCollisionEnabled);
    Fields->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Коллизия по умолчанию")), Enabled);
    connect(Enabled, &QCheckBox::toggled, this, [this](bool bEnabled)
    {
        auto Value = Document.GetDefaults().ToJson();
        Value["collision"]["enabled"] = bEnabled;
        if (bEnabled && Value["collision"]["elements"].empty())
        {
            Value["collision"]["elements"].push_back(StaticMeshCollisionElement{}.ToJson());
        }
        Change(Value);
        QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
    });
    if (!Defaults.CollisionElements.empty())
    {
        const auto& Element = Defaults.CollisionElements.at(SelectedCollisionElement);
        auto* Shape = new QComboBox(this);
        Shape->setObjectName("StaticMeshCollisionShape");
        Shape->addItems({tr("Параллелепипед"), tr("Сфера"), tr("Капсула"), tr("Треугольники меша (Static / Kinematic)")});
        Shape->setCurrentIndex(static_cast<int>(Element.ShapeType));
        Fields->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Форма коллизии")), Shape);
        connect(Shape, &QComboBox::currentIndexChanged, this, [this](int Index)
        {
            auto Value = Document.GetDefaults().ToJson();
            Value["collision"]["elements"][SelectedCollisionElement]["shape"] = Index;
            Change(Value);
            QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
        });
        auto AddNumber = [this](const QString& Label, const std::string& Property, int Axis, double Initial, double Minimum)
        {
            auto* Number = new QDoubleSpinBox(this);
            Number->setRange(Minimum, 1000000);
            Number->setDecimals(4);
            Number->setKeyboardTracking(false);
            Number->setValue(Initial);
            Fields->addRow(Label, Number);
            connect(Number, &QDoubleSpinBox::valueChanged, this, [this, Property, Axis](double Next)
            {
                auto Value = Document.GetDefaults().ToJson();
                if (Axis >= 0)
                {
                    Value["collision"]["elements"][SelectedCollisionElement][Property][Axis] = Next;
                }
                else
                {
                    Value["collision"]["elements"][SelectedCollisionElement][Property] = Next;
                }
                Change(Value);
            });
        };
        const QStringList Axes{"X", "Y", "Z"};
        const auto Value = Element.ToJson();
        for (const std::string Property : {"center", "rotation", "halfExtents"})
        {
            if (Property == "halfExtents" && Element.ShapeType != 0)
            {
                continue;
            }
            QString Label = tr("Положение (м)");
            double Minimum = -1000000;
            if (Property == "rotation")
            {
                Label = tr("Поворот (°)");
            }
            else if (Property == "halfExtents")
            {
                Label = tr("Полуразмер (м)");
                Minimum = 0.0001;
            }
            auto* Row = new QWidget(this);
            auto* RowLayout = new QHBoxLayout(Row);
            RowLayout->setContentsMargins(0, 0, 0, 0);
            RowLayout->setSpacing(4);
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                auto* Caption = new QLabel(Axes[Axis], Row);
                Caption->setObjectName("TransformAxis" + Axes[Axis]);
                RowLayout->addWidget(Caption);
                auto* Number = new QDoubleSpinBox(Row);
                Number->setRange(Minimum, 1000000);
                Number->setDecimals(3);
                Number->setKeyboardTracking(false);
                Number->setValue(Value[Property][Axis].get<double>());
                Number->setMinimumWidth(60);
                RowLayout->addWidget(Number, 1);
                connect(Number, &QDoubleSpinBox::valueChanged, this, [this, Property, Axis](double Next)
                {
                    auto Updated = Document.GetDefaults().ToJson();
                    Updated["collision"]["elements"][SelectedCollisionElement][Property][Axis] = Next;
                    Change(Updated);
                });
            }
            Fields->addRow(Label, Row);
        }
        if (Element.ShapeType == 1 || Element.ShapeType == 2)
        {
            AddNumber(tr("Радиус (м)"), "radius", -1, Element.Radius, 0.0001);
        }
        if (Element.ShapeType == 2)
        {
            AddNumber(tr("Половина высоты цилиндра (м)"), "capsuleHalfHeight", -1, Element.CapsuleHalfHeight, 0.0001);
        }
        if (Element.ShapeType == 3)
        {
            auto* Explanation = BindEditorCaption(new QLabel(tr("Геометрия исходного меша. Доступны перемещение и поворот. Только Static / Kinematic."), this), QString::fromUtf8("Геометрия исходного меша. Доступны перемещение и поворот. Только Static / Kinematic."));
            Explanation->setWordWrap(true);
            Fields->addRow(Explanation);
        }
        auto* Fit = BindEditorCaption(new QPushButton(tr("Подогнать коллизию по границам меша"), this), QString::fromUtf8("Подогнать коллизию по границам меша"));
        Fit->setEnabled(Element.ShapeType != 3);
        Fields->addRow(Fit);
        connect(Fit, &QPushButton::clicked, this, [this]()
        {
            AssetHandle<StaticMeshResource> Mesh;
            if (!Runtime.GetAssetManager().TryGetLoaded(Document.GetAsset(), Mesh))
            {
                return;
            }
            auto Value = Document.GetDefaults().ToJson();
            const Vector3 Center = (Mesh->Bounds.Minimum + Mesh->Bounds.Maximum) * 0.5f;
            const Vector3 Extents = Vector3::Max((Mesh->Bounds.Maximum - Mesh->Bounds.Minimum) * 0.5f, Vector3(0.0001f, 0.0001f, 0.0001f));
            Value["collision"]["elements"][SelectedCollisionElement]["rotation"] = {0, 0, 0};
            Value["collision"]["elements"][SelectedCollisionElement]["center"] = {Center.x, Center.y, Center.z};
            Value["collision"]["elements"][SelectedCollisionElement]["halfExtents"] = {Extents.x, Extents.y, Extents.z};
            float Radius = std::max(Extents.x, Extents.z);
            if (Document.GetDefaults().CollisionElements.at(SelectedCollisionElement).ShapeType == 1)
            {
                Radius = Extents.Length();
            }
            Value["collision"]["elements"][SelectedCollisionElement]["radius"] = Radius;
            Value["collision"]["elements"][SelectedCollisionElement]["capsuleHalfHeight"] = std::max(0.0001f, Extents.y - Radius);
            Change(Value);
            QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
        });
    }
    std::set<int32_t> Slots;
    AssetHandle<StaticMeshResource> Mesh;
    if (Runtime.GetAssetManager().TryGetLoaded(Document.GetAsset(), Mesh))
    {
        for (const auto& Submesh : Mesh->Submeshes)
        {
            Slots.insert(Submesh.MaterialSlot);
        }
    }
    for (const auto& Slot : Defaults.Materials)
    {
        Slots.insert(Slot.first);
    }
    for (int32_t Slot : Slots)
    {
        auto* Material = new QComboBox(this);
        Material->setObjectName(QString("StaticMeshDefaultMaterial_%1").arg(Slot));
        Material->addItem(tr("Не назначен"), QString{});
        AssetThumbnails::Bind(*Material, 0, Runtime.GetAssetRegistry(), {});
        for (const auto& Type : {MaterialAssetType, MaterialInstanceAssetType})
        {
            for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(Type))
            {
                Material->addItem(QString::fromStdString(Entry.VirtualPath), QString::fromStdString(Entry.Metadata.Guid.ToString()));
            AssetThumbnails::Bind(*Material, Material->count() - 1, Runtime.GetAssetRegistry(), {Entry.Metadata.Guid, std::nullopt});
            }
        }
        const auto Found = Defaults.Materials.find(Slot);
        if (Found != Defaults.Materials.end())
        {
            const auto Identifier = QString::fromStdString(Found->second.Asset.ToString());
            int Index = Material->findData(Identifier);
            if (Index < 0)
            {
                Material->addItem(tr("Недоступен: %1").arg(Identifier), Identifier);
                Index = Material->count() - 1;
            }
            Material->setCurrentIndex(Index);
        }
        Material->setEnabled(Document.GetEntry().Mount == AssetMount::Game);
        MaterialFields->addRow(tr("Слот %1").arg(Slot), new AssetReferenceField(*Material, Runtime.GetAssetRegistry()));
        connect(Material, &QComboBox::currentIndexChanged, this, [this, Material, Slot](int)
        {
            auto Defaults = Document.GetDefaults();
            Defaults.Materials.erase(Slot);
            const auto Identifier = Material->currentData().toString();
            if (!Identifier.isEmpty())
            {
                AssetKey Key;
                Guid::TryParse(Identifier.toStdString(), Key.Asset);
                Defaults.Materials[Slot] = Key;
            }
            Change(Defaults.ToJson());
        });
    }
    Fields->parentWidget()->setEnabled(Document.GetEntry().Mount == AssetMount::Game);
    MaterialFields->parentWidget()->setEnabled(true);
    SyncSelection();
}

void StaticMeshEditorWindow::TickPreview()
{
    setWindowTitle(tr("Static Mesh — %1").arg(QString::fromStdString(Document.GetEntry().VirtualPath)));
    if (Document.IsDirty())
    {
        setWindowTitle(windowTitle() + " *");
    }
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        CancelTransform();
        Viewport->ClearSelectedTransform();
    }
    else
    {
        SyncSelection();
    }
    const auto& Collision = GetPreviewDefaults();
    PreviewCollision->SetEnabled(true);
    PreviewCollision->SetCollisionPreview(Collision);
    PreviewCollision->PrepareCollisionMesh(&Runtime.GetAssetManager());
    RebuildCollisionPreview();
    PreviewMesh->MaterialSlots.Entries.clear();
    std::set<int32_t> Slots;
    for (const auto& Submesh : PreviewMesh->Submeshes)
    {
        Slots.insert(Submesh.MaterialSlot);
    }
    for (int32_t Slot : Slots)
    {
        MaterialSlotOverride Override;
        Override.Slot = Slot;
        const auto Found = Document.GetDefaults().Materials.find(Slot);
        if (Found != Document.GetDefaults().Materials.end())
        {
            Override.Material = Found->second;
        }
        PreviewMesh->MaterialSlots.Entries.push_back(Override);
    }
    Runtime.GetAssetManager().LoadAsync<StaticMeshResource>(Document.GetAsset());
    if (Runtime.IsPresenting())
    {
        Runtime.GetSceneAssetResolver().ResolveScene(*PreviewScene);
    }
    AssetHandle<StaticMeshResource> Mesh;
    if (!bFramed && Runtime.GetAssetManager().TryGetLoaded(Document.GetAsset(), Mesh))
    {
        RenderViewCamera Camera;
        Camera.Target = (Mesh->Bounds.Minimum + Mesh->Bounds.Maximum) * 0.5f;
        const float Radius = std::max(0.1f, (Mesh->Bounds.Maximum - Mesh->Bounds.Minimum).Length() * 0.5f);
        Camera.Position = Camera.Target + Vector3(1.5f, 1.f, -2.5f) * Radius;
        Camera.NearPlane = std::max(0.001f, Radius / 1000.f);
        Camera.FarPlane = std::max(1000.f, Radius * 10.f);
        Viewport->SetViewCamera(Camera);
        Viewport->SetCameraMoveSpeed(Radius * 2.f);
        bFramed = true;
        RebuildFields();
    }
    if (!Viewport->HasValidNativeHandle() || !Runtime.IsPresenting() || !isVisible())
    {
        return;
    }
    const auto Info = Viewport->BuildNativeWindowInfo();
    const RenderSurfaceId Surface{ViewId.Value};
    if (Runtime.RegisterRenderSurface(Surface, Info))
    {
        RenderCamera Camera;
        Viewport->GetViewCamera().BuildRenderCamera(static_cast<float>(Info.Width) / std::max(1u, Info.Height), Camera);
        Runtime.ConfigureRenderSurface(Surface, true, Camera);
        Runtime.SetRenderSurfaceScene(Surface, PreviewScene);
        Viewport->SyncGizmoOverlay();
    }
}

void StaticMeshEditorWindow::closeEvent(QCloseEvent* Event)
{
    CancelTransform();
    if (Document.IsDirty())
    {
        const auto Answer = QMessageBox::question(this, tr("Static Mesh"), tr("Сохранить настройки меша?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (Answer == QMessageBox::Cancel)
        {
            Event->ignore();
            return;
        }
        if (Answer == QMessageBox::Save)
        {
            std::string Error;
            if (!Run("save", {}, Error))
            {
                Diagnostic->setText(QString::fromStdString(Error));
                Event->ignore();
                return;
            }
        }
    }
    Runtime.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
    Event->accept();
}

const StaticMeshDefaults& StaticMeshEditorWindow::GetPreviewDefaults() const
{
    if (TransformDraft)
    {
        return *TransformDraft;
    }
    return Document.GetDefaults();
}

void StaticMeshEditorWindow::SyncSelection()
{
    if (Viewport->IsTransformManipulating())
    {
        return;
    }
    const auto& Defaults = Document.GetDefaults();
    if (SelectedCollisionElement >= Defaults.CollisionElements.size() || Document.GetEntry().Mount != AssetMount::Game)
    {
        Viewport->ClearSelectedTransform();
        return;
    }
    const auto& Element = Defaults.CollisionElements[SelectedCollisionElement];
    Transform Placement;
    Placement.Position = Element.Center;
    Placement.Rotation = Quaternion::CreateFromYawPitchRoll(Element.RotationDegrees.y * 0.01745329252f,
        Element.RotationDegrees.x * 0.01745329252f, Element.RotationDegrees.z * 0.01745329252f);
    if (Element.ShapeType == 0)
    {
        Placement.Scale = Element.HalfExtents;
    }
    else if (Element.ShapeType == 1)
    {
        Placement.Scale = Vector3(Element.Radius);
    }
    else if (Element.ShapeType == 2)
    {
        Placement.Scale = Vector3(Element.Radius, Element.CapsuleHalfHeight, Element.Radius);
    }
    Viewport->SetSelectedTransform(Placement, Placement, {});
}

void StaticMeshEditorWindow::PreviewTransform(const Transform& Value)
{
    if (Document.GetEntry().Mount != AssetMount::Game || Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning()
        || SelectedCollisionElement >= Document.GetDefaults().CollisionElements.size())
    {
        return;
    }
    TransformDraft = Document.GetDefaults();
    auto& Element = TransformDraft->CollisionElements[SelectedCollisionElement];
    const auto Operation = Viewport->GetGizmoOperation();
    if (Operation == EditorViewportWidget::GizmoOperation::Translate)
    {
        Element.Center = Value.Position;
    }
    else if (Operation == EditorViewportWidget::GizmoOperation::Rotate)
    {
        Element.RotationDegrees = Value.Rotation.ToEuler() * 57.295779513f;
    }
    else if (Operation == EditorViewportWidget::GizmoOperation::Scale)
    {
        const Vector3 Size = Vector3::Max(Value.Scale, Vector3(0.0001f));
        if (Element.ShapeType == 0)
        {
            Element.HalfExtents = Size;
        }
        else if (Element.ShapeType == 1 || Element.ShapeType == 2)
        {
            float Radius = Size.x;
            if (std::abs(Size.z - Element.Radius) > std::abs(Radius - Element.Radius))
            {
                Radius = Size.z;
            }
            if (Element.ShapeType == 1 && std::abs(Size.y - Element.Radius) > std::abs(Radius - Element.Radius))
            {
                Radius = Size.y;
            }
            Element.Radius = Radius;
            if (Element.ShapeType == 2)
            {
                Element.CapsuleHalfHeight = Size.y;
            }
        }
        else
        {
            Diagnostic->setText(tr("Размер треугольной коллизии задаётся исходным мешем. Используйте перемещение или поворот."));
        }
    }
    DisplayedCollision = {};
}

void StaticMeshEditorWindow::CommitTransform()
{
    if (!TransformDraft)
    {
        return;
    }
    const auto Value = TransformDraft->ToJson();
    TransformDraft.reset();
    Change(Value);
    QTimer::singleShot(0, this, &StaticMeshEditorWindow::RebuildFields);
}

void StaticMeshEditorWindow::CancelTransform()
{
    if (TransformDraft)
    {
        TransformDraft.reset();
        DisplayedCollision = {};
        Viewport->ClearSelectedTransform();
        SyncSelection();
    }
}

void StaticMeshEditorWindow::RebuildCollisionPreview()
{
    const auto Collision = GetPreviewDefaults().ToJson()["collision"];
    if (Collision == DisplayedCollision && DisplayedSelection == SelectedCollisionElement)
    {
        return;
    }
    PhysicsBodyDefinition Definition;
    std::string Error;
    if (!PreviewCollision->BuildDefinition(Definition, Error))
    {
        Viewport->SetCollisionPreviewLines({});
        return;
    }
    CollisionGeometry.clear();
    std::vector<PhysicsDebugLine> Lines;
    for (size_t ElementIndex = 0; ElementIndex < Definition.Shapes.size(); ++ElementIndex)
    {
        PhysicsBodyDefinition Element;
        static_cast<PhysicsShapeDefinition&>(Element) = Definition.Shapes[ElementIndex];
        auto Geometry = MakePhysicsTriangleMesh(Element);
        Color LineColor(0.2f, 0.85f, 0.95f, 1.f);
        if (ElementIndex == SelectedCollisionElement)
        {
            LineColor = Color(1.f, 0.7f, 0.15f, 1.f);
        }
        std::set<std::pair<uint32_t, uint32_t>> Edges;
        for (size_t TriangleIndex = 0; TriangleIndex + 2 < Geometry.Indices.size(); TriangleIndex += 3)
        {
            for (size_t EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
            {
                const auto First = Geometry.Indices[TriangleIndex + EdgeIndex];
                const auto Second = Geometry.Indices[TriangleIndex + (EdgeIndex + 1) % 3];
                if (Edges.insert(std::minmax(First, Second)).second)
                {
                    Lines.push_back({Geometry.Vertices[First], Geometry.Vertices[Second], LineColor});
                }
            }
        }
        CollisionGeometry.push_back(std::move(Geometry));
    }
    Viewport->SetCollisionPreviewLines(std::move(Lines));
    DisplayedCollision = Collision;
    DisplayedSelection = SelectedCollisionElement;
}

void StaticMeshEditorWindow::PickCollision(QPoint Position)
{
    if (!Viewport->IsCollisionVisible() || Viewport->width() <= 0 || Viewport->height() <= 0)
    {
        return;
    }
    const auto& Camera = Viewport->GetViewCamera();
    const Matrix Inverse = (Matrix::CreateLookAt(Camera.Position, Camera.Target, Camera.Up)
        * Matrix::CreatePerspectiveFieldOfView(Camera.FieldOfViewDegrees * 0.01745329252f,
            static_cast<float>(Viewport->width()) / Viewport->height(), Camera.NearPlane, Camera.FarPlane)).Invert();
    const float Horizontal = 2.f * Position.x() / Viewport->width() - 1.f;
    const float Vertical = 1.f - 2.f * Position.y() / Viewport->height();
    Vector3 Direction = Vector3::Transform(Vector3(Horizontal, Vertical, 1.f), Inverse) - Camera.Position;
    Direction.Normalize();
    const DirectX::SimpleMath::Ray PickingRay(Camera.Position, Direction);
    float Closest = Camera.FarPlane;
    size_t Selected = SelectedCollisionElement;
    for (size_t ElementIndex = 0; ElementIndex < CollisionGeometry.size(); ++ElementIndex)
    {
        const auto& Geometry = CollisionGeometry[ElementIndex];
        for (size_t TriangleIndex = 0; TriangleIndex + 2 < Geometry.Indices.size(); TriangleIndex += 3)
        {
            float Distance = 0.f;
            if (PickingRay.Intersects(Geometry.Vertices[Geometry.Indices[TriangleIndex]],
                Geometry.Vertices[Geometry.Indices[TriangleIndex + 1]], Geometry.Vertices[Geometry.Indices[TriangleIndex + 2]], Distance)
                && Distance < Closest)
            {
                Closest = Distance;
                Selected = ElementIndex;
            }
        }
    }
    if (Selected != SelectedCollisionElement)
    {
        SelectedCollisionElement = Selected;
        Tabs->setCurrentIndex(0);
        RebuildFields();
    }
}

void StaticMeshEditorWindow::FrameSelection(bool bMesh)
{
    Vector3 Minimum;
    Vector3 Maximum;
    if (bMesh)
    {
        AssetHandle<StaticMeshResource> Mesh;
        if (!Runtime.GetAssetManager().TryGetLoaded(Document.GetAsset(), Mesh))
        {
            return;
        }
        Minimum = Mesh->Bounds.Minimum;
        Maximum = Mesh->Bounds.Maximum;
    }
    else
    {
        if (SelectedCollisionElement >= CollisionGeometry.size() || CollisionGeometry[SelectedCollisionElement].Vertices.empty())
        {
            return;
        }
        const auto& Vertices = CollisionGeometry[SelectedCollisionElement].Vertices;
        Minimum = Vertices.front();
        Maximum = Vertices.front();
        for (const auto& Vertex : Vertices)
        {
            Minimum = Vector3::Min(Minimum, Vertex);
            Maximum = Vector3::Max(Maximum, Vertex);
        }
    }
    auto Camera = Viewport->GetViewCamera();
    Vector3 Direction = Camera.Position - Camera.Target;
    Direction.Normalize();
    const float Radius = std::max(0.1f, (Maximum - Minimum).Length() * 0.5f);
    Camera.Target = (Minimum + Maximum) * 0.5f;
    Camera.Position = Camera.Target + Direction * Radius * 3.f;
    Camera.NearPlane = std::max(0.001f, Radius / 1000.f);
    Camera.FarPlane = std::max(1000.f, Radius * 20.f);
    Viewport->SetViewCamera(Camera);
    Viewport->SetCameraMoveSpeed(Radius * 2.f);
}
