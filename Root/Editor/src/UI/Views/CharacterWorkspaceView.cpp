#include "UI/Views/AssetReferenceField.h"
#include "UI/Adapters/AssetThumbnails.h"
#include "UI/Views/CharacterWorkspaceView.h"
#include "UI/Styles/EditorStyle.h"
#include "Viewport/EditorViewportWidget.h"
#include "Engine.h"
#include "Core/Object/MemorySubsystem.h"
#include "World/Components/CharacterComponent.h"
#include "World/Components/LightComponent.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "UI/WidgetLayout.h"
#include <QJsonArray>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

CharacterWorkspaceView::CharacterWorkspaceView(Engine& InRuntime, CharacterDocuments& InDocuments, QWidget* Parent)
    : QDialog(Parent), Runtime(InRuntime), Documents(InDocuments)
{
    setObjectName("CharacterWorkspaceView");
    EditorUi::EditorStyle::Apply(*this);
    static uint32_t NextView = 20000;
    ViewId = RenderViewId{NextView++};
    setWindowTitle(tr("Персонажи"));
    resize(1120, 760);
    auto* Layout = new QVBoxLayout(this);
    auto* Toolbar = new QHBoxLayout();
    const auto Button = [&](const QString& Label, auto Callback)
    {
        auto* Control = new QPushButton(Label, this);
        Toolbar->addWidget(Control);
        connect(Control, &QPushButton::clicked, this, Callback);
    };
    Button(tr("Создать"), [this]() { Create(false); });
    Button(tr("Дублировать"), [this]() { Create(true); });
    Button(tr("Сохранить"), [this]() { Save(); });
    Button(tr("Удалить"), [this]()
    {
        if (!Document || Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
        {
            return;
        }
        const auto Answer = QMessageBox::question(this, tr("Удаление персонажа"),
            tr("Удалить ресурс персонажа? Ссылки в сценах и сценариях останутся недоступными."));
        if (Answer != QMessageBox::Yes)
        {
            return;
        }
        std::string Error;
        if (!Documents.Delete(Runtime.GetAssetRegistry(), Runtime.GetAssetManager(), Document->GetEntry(), Error))
        {
            Diagnostic->setText(QString::fromStdString(Error));
            return;
        }
        Document.reset();
        PreviewCharacter->SetPreview({});
        PreviewCharacter->PosedMesh.reset();
        Refresh();
    });
    Button(tr("Отменить"), [this]()
    {
        if (Document && !Runtime.GetPlaySession().IsSimulating())
        {
            Document->Undo();
            RefreshFields();
        }
    });
    Button(tr("Повторить"), [this]()
    {
        if (Document && !Runtime.GetPlaySession().IsSimulating())
        {
            Document->Redo();
            RefreshFields();
        }
    });
    Button(tr("Добавить в открытую сцену"), [this]()
    {
        if (Document && Place && Save())
        {
            Place(Document->GetEntry());
        }
    });
    Layout->addLayout(Toolbar);
    auto* Splitter = new QSplitter(this);
    auto* Sidebar = new QWidget(Splitter);
    auto* SidebarLayout = new QVBoxLayout(Sidebar);
    auto* Search = new QLineEdit(Sidebar);
    Search->setPlaceholderText(tr("Найти персонажа"));
    SidebarLayout->addWidget(Search);
    Library = new QListWidget(Sidebar);
    SidebarLayout->addWidget(Library);
    connect(Search, &QLineEdit::textChanged, this, [this](const QString& Text)
    {
        for (int Index = 0; Index < Library->count(); ++Index)
        {
            Library->item(Index)->setHidden(!Library->item(Index)->text().contains(Text, Qt::CaseInsensitive));
        }
    });
    connect(Library, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* Item)
    {
        AssetRegistryEntry Entry;
        if (Item && Runtime.GetAssetRegistry().TryGetByPath(Item->data(Qt::UserRole).toString().toStdString(), Entry))
        {
            OpenAsset(Entry);
        }
    });
    auto* PreviewPanel = new QWidget(Splitter);
    auto* PreviewLayout = new QVBoxLayout(PreviewPanel);
    Viewport = new EditorViewportWidget(PreviewPanel);
    Viewport->SetViewId(ViewId);
    RenderViewCamera Camera;
    Camera.Position = {2.5f, 1.6f, 3.5f};
    Camera.Target = {0.f, 0.9f, 0.f};
    Viewport->SetViewCamera(Camera);
    Viewport->SetEngine(&Runtime);
    Viewport->SetEditorToolsEnabled(true);
    Viewport->setMinimumSize(300, 320);
    PreviewLayout->addWidget(Viewport);
    auto* Playback = new QPushButton(tr("Пауза / воспроизведение"), PreviewPanel);
    PreviewLayout->addWidget(Playback);
    connect(Playback, &QPushButton::clicked, this, [this]() { bPlaying = !bPlaying; });
    auto* Scroll = new QScrollArea(Splitter);
    Scroll->setWidgetResizable(true);
    FieldContainer = new QWidget(Scroll);
    Fields = new QVBoxLayout(FieldContainer);
    Scroll->setWidget(FieldContainer);
    Splitter->setSizes({200, 450, 450});
    Layout->addWidget(Splitter, 1);
    Diagnostic = new QLabel(this);
    Diagnostic->setWordWrap(true);
    Diagnostic->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    Layout->addWidget(Diagnostic);
    PreviewScene = MemorySubsystem::Get()->NewObject<Scene>();
    PreviewScene->SetAssetManager(&Runtime.GetAssetManager());
    auto* Light = PreviewScene->CreateGameObject("Preview Light");
    Transform LightTransform;
    LightTransform.Rotation = Quaternion::CreateFromYawPitchRoll(-0.5f, -0.7f, 0.f);
    Light->SetTransform(LightTransform);
    Light->SetVisual(false);
    Light->AddComponent<LightComponent>()->Intensity = 2.f;
    auto* Object = PreviewScene->CreateGameObject("Character Preview");
    PreviewCharacter = Object->AddComponent<CharacterComponent>();
    PreviewCharacter->SetPreview({});
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
    PreviewClock.start();
    Timer->start(16);
    Refresh();
}

CharacterWorkspaceView::~CharacterWorkspaceView()
{
    Runtime.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
    Viewport->SetPreviewScene(nullptr);
    MemorySubsystem::Get()->DestroyObject(PreviewScene);
}

void CharacterWorkspaceView::RefreshLibrary()
{
    QSignalBlocker Block(Library);
    Library->clear();
    for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(CharacterAssetType))
    {
        auto* Item = new QListWidgetItem(QString::fromStdWString(std::filesystem::path(Entry.AbsolutePath).stem().wstring()), Library);
        Item->setData(Qt::UserRole, QString::fromStdString(Entry.VirtualPath));
        Library->setIconSize(QSize(48, 48));
        AssetThumbnails::Bind(*Library->model(), Library->indexFromItem(Item), Runtime.GetAssetRegistry(), {Entry.Metadata.Guid, std::nullopt});
        if (Document && Document->GetEntry().Metadata.Guid == Entry.Metadata.Guid)
        {
            Library->setCurrentItem(Item);
        }
    }
}

void CharacterWorkspaceView::Refresh()
{
    AssetRegistryEntry Current;
    if (Document && !Runtime.GetAssetRegistry().TryGetById(Document->GetEntry().Metadata.Guid, Current))
    {
        Document.reset();
        PreviewCharacter->SetPreview({});
        PreviewCharacter->PosedMesh.reset();
    }
    RefreshLibrary();
    RefreshFields();
}

void CharacterWorkspaceView::OpenAsset(const AssetRegistryEntry& Entry)
{
    if (Document && Document->GetEntry().Metadata.Guid != Entry.Metadata.Guid && !ConfirmClose())
    {
        return;
    }
    std::string Error;
    auto Opened = Documents.Open(Runtime.GetAssetRegistry(), Entry, Error);
    if (!Opened)
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    Document = Opened;
    PreviewAnimation = Document->GetData().IdleAnimation;
    Refresh();
}

void CharacterWorkspaceView::Edit(const std::function<void(CharacterDefinition&)>& Change)
{
    if (!Document || Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        Diagnostic->setText(tr("Остановите воспроизведение перед изменением персонажа."));
        return;
    }
    auto Candidate = Document->GetData();
    Change(Candidate);
    std::string Error;
    if (!Document->Apply(Candidate.ToJson(), Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
    }
    LastDocument.clear();
}

void CharacterWorkspaceView::RefreshFields()
{
    while (auto* Item = Fields->takeAt(0))
    {
        if (Item->widget())
        {
            Item->widget()->hide();
            Item->widget()->deleteLater();
        }
        delete Item;
    }
    if (!Document)
    {
        Fields->addWidget(new QLabel(tr("Создайте персонажа или выберите ресурс в списке."), FieldContainer));
        return;
    }
    const auto& Data = Document->GetData();
    auto* FormWidget = new QWidget(FieldContainer);
    auto* Form = new QFormLayout(FormWidget);
    Form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    Fields->addWidget(FormWidget);
    const auto TextField = [&](const QString& Label, const std::string& Text, auto Change)
    {
        auto* Input = new QLineEdit(QString::fromStdString(Text), FormWidget);
        Form->addRow(Label, Input);
        connect(Input, &QLineEdit::editingFinished, this, [this, Input, Change]()
        {
            Edit([&](CharacterDefinition& Value) { Change(Value, Input->text().toStdString()); });
        });
    };
    TextField(tr("Имя"), Data.Name, [](CharacterDefinition& Value, const std::string& Text) { Value.Name = Text; });
    TextField(tr("Описание"), Data.Description, [](CharacterDefinition& Value, const std::string& Text) { Value.Description = Text; });
    auto* DialogueWidget = new QComboBox(FormWidget);
    DialogueWidget->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    DialogueWidget->setMinimumContentsLength(8);
    DialogueWidget->addItem(tr("Стандартный виджет проекта"), QString{});
    QJsonObject WidgetLibrary;
    QString WidgetError;
    if (WidgetLayout::Read(WidgetLayout::ProjectFile(QString::fromStdWString(Runtime.GetAssetRegistry().GetGameContentRoot().wstring())), WidgetLibrary, WidgetError))
    {
        for (const auto& Value : WidgetLibrary.value("widgets").toArray())
        {
            const auto Widget = Value.toObject();
            if (Widget.value("kind") == "dialogue")
            {
                DialogueWidget->addItem(Widget.value("name").toString(), Widget.value("id").toString());
            }
        }
    }
    if (!Data.DialogueWidget.empty() && DialogueWidget->findData(QString::fromStdString(Data.DialogueWidget)) < 0)
    {
        DialogueWidget->addItem(tr("Недоступный виджет"), QString::fromStdString(Data.DialogueWidget));
    }
    DialogueWidget->setCurrentIndex(std::max(0, DialogueWidget->findData(QString::fromStdString(Data.DialogueWidget))));
    Form->addRow(tr("Виджет диалога"), DialogueWidget);
    connect(DialogueWidget, &QComboBox::currentIndexChanged, this, [this, DialogueWidget]()
    {
        Edit([&](CharacterDefinition& Value) { Value.DialogueWidget = DialogueWidget->currentData().toString().toStdString(); });
    });
    auto* Enabled = new QCheckBox(tr("Персонаж включён"), FormWidget);
    Enabled->setChecked(Data.bEnabled);
    Form->addRow(Enabled);
    connect(Enabled, &QCheckBox::toggled, this, [this](bool Value) { Edit([&](CharacterDefinition& Data) { Data.bEnabled = Value; }); });
    auto* Color = new QPushButton(tr("Цвет стандартной модели"), FormWidget);
    Form->addRow(Color);
    connect(Color, &QPushButton::clicked, this, [this]()
    {
        const auto& Value = Document->GetData().Color;
        const auto Chosen = QColorDialog::getColor(QColor::fromRgbF(Value.x, Value.y, Value.z), this);
        if (Chosen.isValid())
        {
            Edit([&](CharacterDefinition& Data) { Data.Color = {static_cast<float>(Chosen.redF()), static_cast<float>(Chosen.greenF()), static_cast<float>(Chosen.blueF()), 1.f}; });
        }
    });
    auto* Model = new QPushButton(tr("Загрузить модель GLB"), FormWidget);
    Form->addRow(Model);
    connect(Model, &QPushButton::clicked, this, [this]() { Import(false); });
    auto* Reset = new QPushButton(tr("Вернуть стандартную модель"), FormWidget);
    Form->addRow(Reset);
    connect(Reset, &QPushButton::clicked, this, [this]()
    {
        Edit([](CharacterDefinition& Data)
        {
            const auto Standard = CharacterDefinition::Create(Data.Name);
            Data.Model = {};
            Data.Skeleton = {};
            Data.Animations = Standard.Animations;
            Data.IdleAnimation = Standard.IdleAnimation;
            Data.WalkAnimation = Standard.WalkAnimation;
        });
        RefreshFields();
    });
    for (const auto& Role : {std::string("idle"), std::string("walk"), std::string("preview")})
    {
        auto* Choice = new QComboBox(FormWidget);
        Choice->addItem(tr("Не назначена"), QString{});
        std::string Selected = PreviewAnimation;
        QString Label = tr("Предпросмотр");
        if (Role == "idle")
        {
            Selected = Data.IdleAnimation;
            Label = tr("Ожидание");
        }
        else if (Role == "walk")
        {
            Selected = Data.WalkAnimation;
            Label = tr("Ходьба");
        }
        for (const auto& Animation : Data.Animations)
        {
            if (Animation.bEnabled)
            {
                Choice->addItem(QString::fromStdString(Animation.Name), QString::fromStdString(Animation.Identifier));
                AssetThumbnails::Bind(*Choice, Choice->count() - 1, Runtime.GetAssetRegistry(), Animation.Clip);
            }
        }
        Choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        Choice->setMinimumContentsLength(8);
        Choice->setCurrentIndex(std::max(0, Choice->findData(QString::fromStdString(Selected))));
        Form->addRow(Label, new AssetReferenceField(*Choice, Runtime.GetAssetRegistry()));
        connect(Choice, &QComboBox::currentIndexChanged, this, [this, Choice, Role]()
        {
            const auto Selected = Choice->currentData().toString().toStdString();
            if (Role == "preview")
            {
                PreviewAnimation = Selected;
                PreviewCharacter->Animation = Selected;
                bPlaying = true;
                return;
            }
            Edit([&](CharacterDefinition& Data)
            {
                if (Role == "idle")
                {
                    Data.IdleAnimation = Selected;
                }
                else
                {
                    Data.WalkAnimation = Selected;
                }
            });
        });
    }
    auto* AddAnimation = new QPushButton(tr("Добавить анимации GLB / Mixamo FBX"), FormWidget);
    Form->addRow(AddAnimation);
    connect(AddAnimation, &QPushButton::clicked, this, [this]() { Import(true); });
    for (const auto& Animation : Data.Animations)
    {
        const auto Identifier = Animation.Identifier;
        auto* Group = new QWidget(FieldContainer);
        auto* Row = new QFormLayout(Group);
        Row->setRowWrapPolicy(QFormLayout::WrapLongRows);
        Row->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        Fields->addWidget(Group);
        const auto ChangeMotion = [this, Identifier](auto Change)
        {
            Edit([&](CharacterDefinition& Data)
            {
                for (auto& Motion : Data.Animations)
                {
                    if (Motion.Identifier == Identifier)
                    {
                        Change(Motion);
                    }
                }
            });
        };
        auto* Identity = new QLineEdit(QString::fromStdString(Identifier), Group);
        Identity->setReadOnly(true);
        Row->addRow(tr("Идентификатор"), Identity);
        auto* Name = new QLineEdit(QString::fromStdString(Animation.Name), Group);
        Row->addRow(tr("Анимация"), Name);
        connect(Name, &QLineEdit::editingFinished, this, [Name, ChangeMotion]()
        { ChangeMotion([&](CharacterAnimation& Value) { Value.Name = Name->text().toStdString(); }); });
        auto* Description = new QLineEdit(QString::fromStdString(Animation.Description), Group);
        Row->addRow(tr("Описание"), Description);
        connect(Description, &QLineEdit::editingFinished, this, [Description, ChangeMotion]()
        { ChangeMotion([&](CharacterAnimation& Value) { Value.Description = Description->text().toStdString(); }); });
        auto* Speed = new QDoubleSpinBox(Group);
        Speed->setRange(0.1, 4.0);
        Speed->setSingleStep(0.1);
        Speed->setValue(Animation.Speed);
        Row->addRow(tr("Скорость"), Speed);
        connect(Speed, &QDoubleSpinBox::editingFinished, this, [Speed, ChangeMotion]()
        { ChangeMotion([&](CharacterAnimation& Value) { Value.Speed = static_cast<float>(Speed->value()); }); });
        for (const auto& Setting : {std::string("enabled"), std::string("loop"), std::string("inPlace")})
        {
            QString Label = tr("Доступна в сценарии");
            bool bValue = Animation.bEnabled;
            if (Setting == "loop")
            {
                Label = tr("Повторять");
                bValue = Animation.bLoop;
            }
            else if (Setting == "inPlace")
            {
                Label = tr("Движение на месте");
                bValue = Animation.bInPlace;
            }
            auto* Toggle = new QCheckBox(Label, Group);
            Toggle->setChecked(bValue);
            Row->addRow(Toggle);
            connect(Toggle, &QCheckBox::toggled, this, [Setting, ChangeMotion](bool Value)
            {
                ChangeMotion([&](CharacterAnimation& Motion)
                {
                    if (Setting == "enabled")
                    {
                        Motion.bEnabled = Value;
                    }
                    else if (Setting == "loop")
                    {
                        Motion.bLoop = Value;
                    }
                    else
                    {
                        Motion.bInPlace = Value;
                    }
                });
            });
        }
        auto* DuplicateMotion = new QPushButton(tr("Создать вариант"), Group);
        Row->addRow(DuplicateMotion);
        connect(DuplicateMotion, &QPushButton::clicked, this, [this, Animation]()
        {
            Edit([&](CharacterDefinition& Definition)
            {
                auto Variant = Animation;
                Variant.Identifier = Guid::Generate().ToString();
                Variant.Name += " (вариант)";
                Definition.Animations.push_back(std::move(Variant));
            });
            RefreshFields();
        });
        auto* RemoveMotion = new QPushButton(tr("Удалить анимацию"), Group);
        Row->addRow(RemoveMotion);
        connect(RemoveMotion, &QPushButton::clicked, this, [this, Identifier]()
        {
            Edit([&](CharacterDefinition& Definition)
            {
                Definition.Animations.erase(std::remove_if(Definition.Animations.begin(), Definition.Animations.end(), [&](const CharacterAnimation& Motion)
                {
                    return Motion.Identifier == Identifier;
                }), Definition.Animations.end());
                if (Definition.IdleAnimation == Identifier)
                {
                    Definition.IdleAnimation.clear();
                }
                if (Definition.WalkAnimation == Identifier)
                {
                    Definition.WalkAnimation.clear();
                }
            });
            RefreshFields();
        });
        auto* Preview = new QPushButton(tr("Посмотреть"), Group);
        Row->addRow(Preview);
        connect(Preview, &QPushButton::clicked, this, [this, Identifier]()
        {
            PreviewAnimation = Identifier;
            PreviewCharacter->Animation = Identifier;
            PreviewCharacter->SetPreview(std::make_shared<CharacterDefinition>(Document->GetData()));
            bPlaying = true;
        });
    }
    Fields->addStretch();
    FormWidget->setEnabled(!Document->IsReadOnly());
    PreviewCharacter->Animation = PreviewAnimation;
    PreviewCharacter->SetPreview(std::make_shared<CharacterDefinition>(Data));
    LastDocument = Data.ToJson().dump();
}

void CharacterWorkspaceView::Create(bool bDuplicate)
{
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        return;
    }
    const auto Name = QInputDialog::getText(this, tr("Персонаж"), tr("Имя персонажа")).trimmed();
    if (Name.isEmpty())
    {
        return;
    }
    auto Data = CharacterDefinition::Create(Name.toStdString());
    if (bDuplicate && Document)
    {
        Data = Document->GetData();
        Data.Name = Name.toStdString();
    }
    AssetRegistryEntry Entry;
    std::string Error;
    if (!CharacterDocument::Create(Runtime.GetAssetRegistry(), Data, Entry, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    OpenAsset(Entry);
}

void CharacterWorkspaceView::Import(bool bAnimationsOnly)
{
    if (!Document || Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        return;
    }
    QString Filter = tr("Модель GLB (*.glb)");
    if (bAnimationsOnly)
    {
        Filter = tr("Анимации (*.glb *.fbx)");
    }
    const auto Source = QFileDialog::getOpenFileName(this, tr("Импорт персонажа"), {}, Filter);
    if (Source.isEmpty())
    {
        return;
    }
    std::string Error;
    if (!Document->ImportModel(Runtime.GetAssetRegistry(), std::filesystem::path(Source.toStdWString()), bAnimationsOnly, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
    }
    RefreshFields();
}

bool CharacterWorkspaceView::Save()
{
    if (!Document || Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        return false;
    }
    std::string Error;
    const bool bSaved = Document->Save(Runtime.GetAssetManager(), Error);
    Diagnostic->setText(QString::fromStdString(Error));
    return bSaved;
}

bool CharacterWorkspaceView::ConfirmClose()
{
    if (!Document || !Document->IsDirty())
    {
        return true;
    }
    const auto Answer = QMessageBox::question(this, tr("Персонаж"), tr("Сохранить изменения персонажа?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (Answer == QMessageBox::Save)
    {
        return Save();
    }
    if (Answer == QMessageBox::Discard)
    {
        std::string Error;
        return Document->Open(Runtime.GetAssetRegistry(), Document->GetEntry(), Error);
    }
    return false;
}

void CharacterWorkspaceView::closeEvent(QCloseEvent* Event)
{
    if (!ConfirmClose())
    {
        Event->ignore();
        return;
    }
    Runtime.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
    QDialog::closeEvent(Event);
}

void CharacterWorkspaceView::TickPreview()
{
    const float DeltaTime = std::min(0.1f, static_cast<float>(PreviewClock.restart()) / 1000.f);
    if (!isVisible() || !Document)
    {
        return;
    }
    const auto Current = Document->GetData().ToJson().dump();
    if (LastDocument != Current)
    {
        PreviewCharacter->SetPreview(std::make_shared<CharacterDefinition>(Document->GetData()));
        LastDocument = Current;
    }
    PreviewCharacter->Resolve(Runtime.GetAssetManager());
    if (bPlaying)
    {
        PreviewCharacter->AdvancePreview(DeltaTime);
    }
    QString Title = tr("Персонажи — ") + QString::fromStdString(Document->GetData().Name);
    if (Document->IsDirty())
    {
        Title += " *";
    }
    setWindowTitle(Title);
    if (!Viewport->HasValidNativeHandle() || !Runtime.IsPresenting())
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
    }
}
