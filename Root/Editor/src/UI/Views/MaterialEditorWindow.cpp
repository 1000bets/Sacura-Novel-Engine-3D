#include "UI/Presentation/LocalizedEditorUi.h"
#include "UI/Styles/StudioIcons.h"
#include "UI/Views/AssetReferenceField.h"
#include "UI/Adapters/AssetThumbnails.h"
#include "UI/Views/MaterialEditorWindow.h"
#include "Engine.h"
#include "UI/MaterialWidget.h"
#include "UI/Styles/EditorStyle.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QFontDatabase>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <limits>

class MaterialSourceHighlighter : public QSyntaxHighlighter
{
public:
    explicit MaterialSourceHighlighter(QTextDocument* Document) : QSyntaxHighlighter(Document) {}
protected:
    void highlightBlock(const QString& Text) override
    {
        QTextCharFormat Keyword;
        Keyword.setForeground(EditorUi::EditorStyle::Color("--info"));
        const QRegularExpression Keywords("\\b(float[1-4]?|float[234]x[234]|int|uint|bool|void|return|if|else|for|struct|inout|out|true|false|Texture2D|SamplerState)\\b");
        auto Matches = Keywords.globalMatch(Text);
        while (Matches.hasNext())
        {
            const auto Match = Matches.next();
            setFormat(Match.capturedStart(), Match.capturedLength(), Keyword);
        }
        QTextCharFormat Comment;
        Comment.setForeground(EditorUi::EditorStyle::Color("--success"));
        const auto CommentStart = Text.indexOf("//");
        if (CommentStart >= 0)
        {
            setFormat(CommentStart, Text.size() - CommentStart, Comment);
        }
    }
};

MaterialEditorWindow::MaterialEditorWindow(Engine& InRuntime, std::shared_ptr<MaterialEditingSession> InSession, QWidget* Parent)
    : QMainWindow(Parent, Qt::Window), Runtime(InRuntime), Session(std::move(InSession))
{
    setObjectName("MaterialEditorWindow");
    EditorUi::EditorStyle::Apply(*this);
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
        | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1100, 720);
    setMinimumSize(900, 600);
    auto* Content = new QWidget(this);
    Content->setObjectName("MaterialEditorContent");
    setCentralWidget(Content);
    auto* Layout = new QVBoxLayout(Content);
    auto* FileMenu = BindEditorCaption(menuBar()->addMenu(tr("Файл")), QString::fromUtf8("Файл"));
    auto* EditMenu = BindEditorCaption(menuBar()->addMenu(tr("Правка")), QString::fromUtf8("Правка"));
    auto* ViewMenu = BindEditorCaption(menuBar()->addMenu(tr("Вид")), QString::fromUtf8("Вид"));
    auto* Toolbar = new QToolBar(tr("Документ"), this);
    Toolbar->setObjectName("MaterialDocumentToolbar");
    Toolbar->setMovable(false);
    Toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    Toolbar->setIconSize(QSize(20, 20));
    addToolBar(Toolbar);
    auto* SaveAction = FileMenu->addAction(EditorUi::MakeFrontendIcon("Save"), tr("Сохранить"));
    SaveAction->setShortcut(QKeySequence::Save);
    BindEditorProperty(SaveAction, "toolTip", QString::fromUtf8("Сохранить (Ctrl+S)"));
    Toolbar->addAction(SaveAction);
    connect(SaveAction, &QAction::triggered, this, [this]() { Save(); });
    auto* ReloadAction = BindEditorCaption(FileMenu->addAction(tr("Перечитать")), QString::fromUtf8("Перечитать"));
    connect(ReloadAction, &QAction::triggered, this, [this]()
    {
        if ((Session->GetDocument().IsDirty() || bSourcePending)
            && QMessageBox::question(this, tr("Перечитать материал"), tr("Отбросить локальные изменения?")) != QMessageBox::Yes)
        {
            return;
        }
        bSourcePending = false;
        Run(MaterialEditOperation::Reload);
    });
    auto* CloseAction = BindEditorCaption(FileMenu->addAction(tr("Закрыть")), QString::fromUtf8("Закрыть"));
    CloseAction->setShortcut(QKeySequence::Close);
    connect(CloseAction, &QAction::triggered, this, [this]() { close(); });
    auto* UndoAction = BindEditorCaption(EditMenu->addAction(tr("Отменить")), QString::fromUtf8("Отменить"));
    UndoAction->setShortcut(QKeySequence::Undo);
    connect(UndoAction, &QAction::triggered, this, [this]() { Run(MaterialEditOperation::Undo); });
    auto* RedoAction = BindEditorCaption(EditMenu->addAction(tr("Повторить")), QString::fromUtf8("Повторить"));
    RedoAction->setShortcut(QKeySequence::Redo);
    connect(RedoAction, &QAction::triggered, this, [this]() { Run(MaterialEditOperation::Redo); });
    auto* CompileAction = BindEditorCaption(ViewMenu->addAction(tr("Обновить предпросмотр")), QString::fromUtf8("Обновить предпросмотр"));
    connect(CompileAction, &QAction::triggered, this, [this]() { Run(MaterialEditOperation::Compile); });
    ViewMenu->addAction(Toolbar->toggleViewAction());
    StateLabel = new QLabel(this);
    Layout->addWidget(StateLabel);
    auto* Splitter = new QSplitter(this);
    auto* PreviewPanel = new QWidget(Splitter);
    PreviewPanel->setObjectName("MaterialPreviewPanel");
    auto* PreviewLayout = new QVBoxLayout(PreviewPanel);
    auto* Shape = new QComboBox(PreviewPanel);
    Shape->addItems({tr("Сфера"), tr("Куб"), tr("Плоскость")});
    PreviewLayout->addWidget(Shape);
    Preview = new MaterialWidget(Runtime, PreviewPanel);
    Preview->setObjectName("MaterialPreview");
    Preview->SetPreviewShape(0);
    Preview->setMinimumSize(250, 250);
    PreviewLayout->addWidget(Preview, 1);
    connect(Shape, &QComboBox::currentIndexChanged, this, [this](int Index) { Preview->SetPreviewShape(Index); });
    auto* Tabs = new QTabWidget(Splitter);
    auto* Scroll = new QScrollArea(Tabs);
    Scroll->setWidgetResizable(true);
    auto* FieldsWidget = new QWidget(Scroll);
    FieldsWidget->setObjectName("MaterialFields");
    Fields = new QFormLayout(FieldsWidget);
    Fields->setSizeConstraint(QLayout::SetMinAndMaxSize);
    Fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    Fields->setRowWrapPolicy(QFormLayout::WrapLongRows);
    Fields->setVerticalSpacing(10);
    Scroll->setWidget(FieldsWidget);
    Tabs->addTab(Scroll, tr("Параметры"));
    SourceEditor = new QPlainTextEdit(Tabs);
    SourceEditor->setObjectName("MaterialSourceEditor");
    SourceEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    SourceEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    new MaterialSourceHighlighter(SourceEditor->document());
    Tabs->addTab(SourceEditor, tr("Исходный HLSL"));
    if (Session->IsInstance())
    {
        Tabs->setTabVisible(Tabs->indexOf(SourceEditor), false);
    }
    Splitter->setSizes({420, 650});
    Layout->addWidget(Splitter, 1);
    Diagnostics = new QPlainTextEdit(this);
    Diagnostics->setObjectName("MaterialDiagnostics");
    Diagnostics->setReadOnly(true);
    Diagnostics->setMaximumHeight(120);
    Layout->addWidget(Diagnostics);
    SourceTimer = new QTimer(this);
    SourceTimer->setSingleShot(true);
    SourceTimer->setInterval(500);
    Watcher = new QFileSystemWatcher(this);
    auto* DependencyTimer = new QTimer(this);
    DependencyTimer->setSingleShot(true);
    DependencyTimer->setInterval(300);
    connect(Watcher, &QFileSystemWatcher::fileChanged, this, [DependencyTimer](const QString&) { DependencyTimer->start(); });
    connect(DependencyTimer, &QTimer::timeout, this, [this]()
    {
        if (!Session->GetDocument().HasExternalChanges() && !bSourcePending)
        {
            RebuildFields();
            Run(MaterialEditOperation::Compile);
        }
    });
    connect(SourceTimer, &QTimer::timeout, this, [this]() { FlushSource(); });
    connect(SourceEditor, &QPlainTextEdit::textChanged, this, [this]()
    {
        bSourcePending = true;
        SourceTimer->start();
    });
    auto* Timer = new QTimer(this);
    Timer->setInterval(100);
    connect(Timer, &QTimer::timeout, this, [this]()
    {
        Session->Poll(Runtime.GetAssetGpuUploader());
        Refresh();
    });
    Timer->start();
    Refresh();
    Run(MaterialEditOperation::Compile);
}

bool MaterialEditorWindow::FlushSource()
{
    if (!bSourcePending)
    {
        return true;
    }
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        Diagnostics->setPlainText(tr("Остановите воспроизведение перед изменением материала."));
        return false;
    }
    SourceTimer->stop();
    const auto Result = Session->Execute(MaterialEditOperation::Source, SourceEditor->toPlainText().toStdString());
    if (Result.HasError())
    {
        Diagnostics->setPlainText(QString::fromStdString(Result.Message));
        DisplayedRevision = 0;
        Refresh();
        return false;
    }
    bSourcePending = false;
    DisplayedRevision = Session->GetRevision();
    return true;
}

bool MaterialEditorWindow::Run(MaterialEditOperation Operation, const nlohmann::json& Value, uint64_t ExpectedRevision)
{
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        Diagnostics->setPlainText(tr("Остановите воспроизведение перед изменением материала."));
        return false;
    }
    if (!FlushSource())
    {
        return false;
    }
    const auto Result = Session->Execute(Operation, Value, ExpectedRevision);
    if (Result.HasError())
    {
        Diagnostics->setPlainText(QString::fromStdString(Result.Message));
        DisplayedRevision = 0;
        Refresh();
        return false;
    }
    Refresh();
    return true;
}

bool MaterialEditorWindow::Save()
{
    if (!Run(MaterialEditOperation::Save))
    {
        return false;
    }
    AssetThumbnails::Invalidate();
    Diagnostics->setPlainText(tr("Материал сохранён. Изменения общего ресурса используются всеми его экземплярами."));
    return true;
}

void MaterialEditorWindow::Refresh()
{
    const auto& Document = Session->GetDocument();
    AssetRegistryEntry Entry;
    Runtime.GetAssetRegistry().TryResolveKey(Document.GetAsset(), Entry);
    QString Title = tr("Материал — %1").arg(QString::fromStdString(Entry.VirtualPath));
    if (Document.IsDirty() || bSourcePending)
    {
        Title += " *";
    }
    setWindowTitle(Title);
    QString State = tr("Сохранение меняет общий ресурс. Предпросмотр использует последнюю успешную проверку.");
    if (Document.IsReadOnly())
    {
        State = tr("Ресурс движка доступен только для чтения. Создайте материал в Content проекта для редактирования.");
    }
    if (Document.HasExternalChanges())
    {
        State = tr("Файл изменён извне. Сохранение заблокировано; перечитайте ресурс после сохранения нужных правок.");
    }
    StateLabel->setText(State);
    StateLabel->setWordWrap(true);
    if (DisplayedRevision != Session->GetRevision() && !bSourcePending)
    {
        DisplayedRevision = Session->GetRevision();
        QSignalBlocker Block(SourceEditor);
        SourceEditor->setPlainText(QString::fromStdString(Document.GetSource()));
        SourceEditor->setReadOnly(Document.IsSourceReadOnly());
        RebuildFields();
    }
    if (Session->GetPreview() && DisplayedMaterial != Session->GetPreview())
    {
        DisplayedPreview = Session->GetPreviewRevision();
        DisplayedMaterial = Session->GetPreview();
        Preview->SetDynamicMaterial(DisplayedMaterial);
        if (Session->IsInstance())
        {
            QSignalBlocker Block(SourceEditor);
            SourceEditor->setPlainText(QString::fromStdString(DisplayedMaterial->GetResolvedMaterial().Definition.PreparedSource));
        }
    }
    const auto Status = QString::fromStdString(Session->GetDiagnostic());
    if (Diagnostics->property("compilationStatus").toString() != Status)
    {
        Diagnostics->setProperty("compilationStatus", Status);
        Diagnostics->setPlainText(Status);
    }
}

void MaterialEditorWindow::SetParameter(const MaterialParameterDefinition& Definition, const nlohmann::json& Value)
{
    auto Draft = Session->GetDocument().GetDocument();
    if (Session->IsInstance())
    {
        Draft["overrides"][Definition.Identifier] = {{"type", MaterialDefinitionIO::ParameterTypeName(Definition.DefaultValue.Type)}, {"value", Value}};
    }
    else
    {
        for (auto& Parameter : Draft["parameters"])
        {
            if (Parameter.at("id") == Definition.Identifier)
            {
                Parameter["default"] = Value;
            }
        }
    }
    Run(MaterialEditOperation::Document, Draft);
}

void MaterialEditorWindow::EditParameterDefinition(const std::string& Identifier)
{
    if (!FlushSource())
    {
        return;
    }
    const auto ExpectedRevision = Session->GetRevision();
    QDialog Dialog(this);
    Dialog.setObjectName("MaterialParameterDialog");
    EditorUi::EditorStyle::Apply(Dialog);
    BindEditorProperty(&Dialog, "windowTitle", QString::fromUtf8("Определение параметра"));
    auto* Form = new QFormLayout(&Dialog);
    auto Draft = Session->GetDocument().GetDocument();
    nlohmann::json Definition = nlohmann::json::object();
    for (const auto& Parameter : Draft["parameters"])
    {
        if (Parameter.at("id") == Identifier)
        {
            Definition = Parameter;
        }
    }
    auto* Name = new QLineEdit(QString::fromStdString(Identifier), &Dialog);
    Name->setReadOnly(!Identifier.empty());
    auto* DisplayName = new QLineEdit(QString::fromStdString(Definition.value("displayName", Identifier)), &Dialog);
    auto* Group = new QLineEdit(QString::fromStdString(Definition.value("group", "Parameters")), &Dialog);
    auto* Type = new QComboBox(&Dialog);
    Type->addItems({"Float", "Integer", "Boolean", "Vector2", "Vector3", "Vector4", "Color", "Texture2D", "StaticSwitch"});
    Type->setCurrentText(QString::fromStdString(Definition.value("type", "Float")));
    Type->setEnabled(Identifier.empty());
    auto* Srgb = new QCheckBox(&Dialog);
    Srgb->setChecked(Definition.value("srgb", false));
    auto* Clamp = new QCheckBox(&Dialog);
    Clamp->setChecked(Definition.value("address", "Wrap") == "Clamp");
    auto* Point = new QCheckBox(&Dialog);
    Point->setChecked(Definition.value("filter", "Linear") == "Point");
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Имя в HLSL")), Name);
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Название")), DisplayName);
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Группа")), Group);
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Тип")), Type);
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Текстура sRGB")), Srgb);
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Текстура: ограничить края")), Clamp);
    Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Текстура: точечная фильтрация")), Point);
    auto* Explanation = BindEditorCaption(new QLabel(tr("Имя используется в HLSL. После добавления измените исходный код, чтобы использовать параметр."), &Dialog), QString::fromUtf8("Имя используется в HLSL. После добавления измените исходный код, чтобы использовать параметр."));
    Explanation->setWordWrap(true);
    Form->addRow(Explanation);
    auto* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
    Form->addRow(Buttons);
    connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    if (Dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    Definition["id"] = Name->text().toStdString();
    Definition["displayName"] = DisplayName->text().toStdString();
    Definition["group"] = Group->text().toStdString();
    Definition["type"] = Type->currentText().toStdString();
    if (Identifier.empty())
    {
        const auto TypeName = Type->currentText();
        if (TypeName == "Texture2D")
        {
            Definition["default"] = nullptr;
        }
        else if (TypeName == "Boolean" || TypeName == "StaticSwitch")
        {
            Definition["default"] = false;
        }
        else if (TypeName == "Float" || TypeName == "Integer")
        {
            Definition["default"] = 0;
        }
        else
        {
            size_t Count = 4;
            if (TypeName == "Vector2")
            {
                Count = 2;
            }
            else if (TypeName == "Vector3")
            {
                Count = 3;
            }
            Definition["default"] = std::vector<float>(Count, 0.f);
        }
    }
    if (Type->currentText() == "Texture2D")
    {
        Definition["srgb"] = Srgb->isChecked();
        Definition["address"] = "Wrap";
        Definition["filter"] = "Linear";
        if (Clamp->isChecked())
        {
            Definition["address"] = "Clamp";
        }
        if (Point->isChecked())
        {
            Definition["filter"] = "Point";
        }
    }
    if (Identifier.empty())
    {
        Draft["parameters"].push_back(Definition);
    }
    else
    {
        for (auto& Parameter : Draft["parameters"])
        {
            if (Parameter.at("id") == Identifier)
            {
                Parameter = Definition;
            }
        }
    }
    Run(MaterialEditOperation::Document, Draft, ExpectedRevision);
}

QWidget* MaterialEditorWindow::MakeParameterField(const MaterialParameterDefinition& Definition, const MaterialParameterValue& Value)
{
    auto* Container = new QWidget(this);
    Container->setMinimumHeight(36);
    auto* Layout = new QHBoxLayout(Container);
    Layout->setContentsMargins(0, 0, 0, 0);
    const auto Type = Value.Type;
    if (Type == MaterialParameterType::Texture2D)
    {
        auto* Picker = new QComboBox(Container);
        Picker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        Picker->setMinimumContentsLength(8);
        Picker->addItem(tr("Без текстуры"), QString());
        AssetThumbnails::Bind(*Picker, 0, Runtime.GetAssetRegistry(), {});
        for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(TextureAssetType))
        {
            Picker->addItem(QString::fromStdString(Entry.VirtualPath), QString::fromStdString(Entry.Metadata.Guid.ToString()));
            AssetThumbnails::Bind(*Picker, Picker->count() - 1, Runtime.GetAssetRegistry(), {Entry.Metadata.Guid, std::nullopt});
        }
        const auto Selected = Picker->findData(QString::fromStdString(Value.Texture.Asset.ToString()));
        if (Value.Texture.IsValid() && Selected < 0)
        {
            Picker->addItem(tr("Текстура недоступна"), QString::fromStdString(Value.Texture.Asset.ToString()));
            Picker->setCurrentIndex(Picker->count() - 1);
        }
        else if (Selected >= 0)
        {
            Picker->setCurrentIndex(Selected);
        }
        Layout->addWidget(new AssetReferenceField(*Picker, Runtime.GetAssetRegistry()));
        connect(Picker, &QComboBox::activated, this, [this, Picker, Definition](int)
        {
            AssetKey Texture;
            Guid::TryParse(Picker->currentData().toString().toStdString(), Texture.Asset);
            SetParameter(Definition, AssetMetadataIO::AssetRefToJson(Texture));
        });
    }
    else if (Type == MaterialParameterType::Boolean || Type == MaterialParameterType::StaticSwitch)
    {
        auto* Field = new QCheckBox(Container);
        Field->setChecked(Value.bBoolean);
        Layout->addWidget(Field);
        connect(Field, &QCheckBox::clicked, this, [this, Definition](bool bChecked) { SetParameter(Definition, bChecked); });
    }
    else if (Type == MaterialParameterType::Integer)
    {
        auto* Field = new QSpinBox(Container);
        Field->setRange(std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max());
        Field->setValue(Value.Integer);
        Layout->addWidget(Field);
        connect(Field, &QSpinBox::editingFinished, this, [this, Field, Definition]() { SetParameter(Definition, Field->value()); });
    }
    else
    {
        int Count = 1;
        if (Type == MaterialParameterType::Vector2)
        {
            Count = 2;
        }
        else if (Type == MaterialParameterType::Vector3)
        {
            Count = 3;
        }
        else if (Type == MaterialParameterType::Vector4 || Type == MaterialParameterType::Color)
        {
            Count = 4;
        }
        std::vector<QDoubleSpinBox*> Components;
        auto* ComponentFields = new QWidget(Container);
        auto* ComponentLayout = new QGridLayout(ComponentFields);
        ComponentLayout->setContentsMargins(0, 0, 0, 0);
        Layout->addWidget(ComponentFields, 1);
        int ColumnCount = Count;
        if (Count == 4)
        {
            ColumnCount = 2;
            Container->setMinimumHeight(80);
        }
        for (int Index = 0; Index < Count; ++Index)
        {
            auto* Field = new QDoubleSpinBox(Container);
            Field->setDecimals(5);
            Field->setRange(-1.e9, 1.e9);
            if (Definition.bHasRange)
            {
                Field->setRange(Definition.Minimum, Definition.Maximum);
            }
            Field->setSingleStep(0.05);
            Field->setMinimumWidth(94);
            Field->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            Field->setValue((&Value.Numbers.x)[Index]);
            Components.push_back(Field);
            ComponentLayout->addWidget(Field, Index / ColumnCount, Index % ColumnCount);
        }
        for (auto* Field : Components)
        {
            connect(Field, &QDoubleSpinBox::editingFinished, this, [this, Components, Definition, Count]()
            {
                if (Count == 1)
                {
                    SetParameter(Definition, Components.front()->value());
                    return;
                }
                auto Values = nlohmann::json::array();
                for (auto* Component : Components)
                {
                    Values.push_back(Component->value());
                }
                SetParameter(Definition, Values);
            });
        }
        if (Type == MaterialParameterType::Color)
        {
            auto* SelectColor = BindEditorCaption(new QPushButton(tr("Цвет…"), Container), QString::fromUtf8("Цвет…"));
            Layout->addWidget(SelectColor);
            connect(SelectColor, &QPushButton::clicked, this, [this, Components, Definition]()
            {
                const auto Initial = QColor::fromRgbF(qBound(0.0, Components[0]->value(), 1.0),
                    qBound(0.0, Components[1]->value(), 1.0), qBound(0.0, Components[2]->value(), 1.0),
                    qBound(0.0, Components[3]->value(), 1.0));
                const auto Color = QColorDialog::getColor(Initial, this, tr("Цвет параметра"), QColorDialog::ShowAlphaChannel);
                if (Color.isValid())
                {
                    SetParameter(Definition, nlohmann::json::array({Color.redF(), Color.greenF(), Color.blueF(), Color.alphaF()}));
                }
            });
        }
    }
    if (Type == MaterialParameterType::Texture2D)
    {
        for (auto* Picker : Container->findChildren<QComboBox*>())
        {
            Picker->setEnabled(!Session->GetDocument().IsReadOnly());
        }
    }
    else
    {
        Container->setEnabled(!Session->GetDocument().IsReadOnly());
    }
    Container->setObjectName(QString::fromStdString("MaterialParameter_" + Definition.Identifier));
    return Container;
}

void MaterialEditorWindow::RebuildFields()
{
    while (Fields->rowCount() > 0)
    {
        auto Row = Fields->takeRow(0);
        for (auto* Item : {Row.labelItem, Row.fieldItem})
        {
            if (Item)
            {
                if (auto* Widget = Item->widget())
                {
                    Widget->hide();
                    Widget->deleteLater();
                }
                delete Item;
            }
        }
    }
    const auto& Document = Session->GetDocument();
    const auto Draft = Document.GetDocument();
    ResolvedMaterial Resolved;
    const auto Result = Document.BuildResolved(Resolved);
    if (Result.HasError())
    {
        Fields->addRow(new QLabel(QString::fromStdString(Result.Message), this));
        return;
    }
    QStringList WatchedFiles;
    for (const auto& File : Resolved.Definition.SourceFiles)
    {
        WatchedFiles.push_back(QString::fromStdString(File));
    }
    for (const auto& Dependency : Resolved.Definition.Dependencies)
    {
        AssetRegistryEntry Entry;
        if (Runtime.GetAssetRegistry().TryResolveKey(Dependency, Entry))
        {
            WatchedFiles.push_back(QString::fromStdString(Entry.AbsolutePath));
        }
    }
    if (!Watcher->files().isEmpty())
    {
        Watcher->removePaths(Watcher->files());
    }
    WatchedFiles.removeDuplicates();
    if (!WatchedFiles.isEmpty())
    {
        Watcher->addPaths(WatchedFiles);
    }
    QString Domain = "Surface";
    if (Resolved.Definition.Domain == MaterialDomain::PostProcess)
    {
        Domain = "PostProcess";
    }
    else if (Resolved.Definition.Domain == MaterialDomain::UserInterface)
    {
        Domain = "UserInterface";
    }
    Fields->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Область применения")), new QLabel(Domain, this));
    auto AddChoice = [this, Draft, &Document](const QString& Label, const std::string& Name, const QStringList& Choices, const QString& Default)
    {
        auto* Choice = new QComboBox(this);
        Choice->addItems(Choices);
        Choice->setCurrentText(QString::fromStdString(Draft.value(Name, Default.toStdString())));
        Choice->setEnabled(!Document.IsReadOnly());
        Fields->addRow(Label, Choice);
        connect(Choice, &QComboBox::activated, this, [this, Choice, Name](int)
        {
            auto Candidate = Session->GetDocument().GetDocument();
            Candidate[Name] = Choice->currentText().toStdString();
            Run(MaterialEditOperation::Document, Candidate);
        });
    };
    if (!Session->IsInstance())
    {
        if (Resolved.Definition.Domain == MaterialDomain::Surface)
        {
            AddChoice(tr("Освещение"), "shadingModel", {"Lit", "Unlit", "Toon"}, "Lit");
        }
        AddChoice(tr("Смешивание"), "blendMode", {"Opaque", "Masked", "Translucent", "Additive"}, "Opaque");
        if (Resolved.Definition.Domain == MaterialDomain::PostProcess)
        {
            AddChoice(tr("Стадия"), "postProcessStage", {"BeforeToneMapping", "AfterToneMapping"}, "BeforeToneMapping");
        }
        for (const auto& Setting : std::vector<std::pair<std::string, QString>>{
            {"doubleSided", tr("Двусторонний")}, {"castShadows", tr("Отбрасывает тени")},
            {"receiveShadows", tr("Принимает тени")}, {"animated", tr("Анимированный")},
            {"modifyVertex", tr("Изменяет вершины")}})
        {
            auto* Field = new QCheckBox(this);
            bool bDefault = false;
            if (Setting.first == "castShadows" || Setting.first == "receiveShadows")
            {
                bDefault = true;
            }
            Field->setChecked(Draft.value(Setting.first, bDefault));
            Field->setEnabled(!Document.IsReadOnly());
            Fields->addRow(Setting.second, Field);
            connect(Field, &QCheckBox::clicked, this, [this, Name = Setting.first](bool bChecked)
            {
                auto Candidate = Session->GetDocument().GetDocument();
                Candidate[Name] = bChecked;
                Run(MaterialEditOperation::Document, Candidate);
            });
        }
        for (const auto& Setting : std::vector<std::pair<std::string, QString>>{
            {"alphaCutoff", tr("Порог отсечения прозрачности")}, {"boundsExpansion", tr("Расширение границ геометрии")}})
        {
            auto* Field = new QDoubleSpinBox(this);
            Field->setDecimals(4);
            Field->setRange(0.0, 1000000.0);
            double Default = 0.0;
            if (Setting.first == "alphaCutoff")
            {
                Default = 0.5;
                Field->setMaximum(1.0);
            }
            Field->setValue(Draft.value(Setting.first, Default));
            Field->setEnabled(!Document.IsReadOnly());
            Fields->addRow(Setting.second, Field);
            connect(Field, &QDoubleSpinBox::editingFinished, this, [this, Field, Name = Setting.first]()
            {
                auto Candidate = Session->GetDocument().GetDocument();
                Candidate[Name] = Field->value();
                Run(MaterialEditOperation::Document, Candidate);
            });
        }
    }
    else
    {
        auto* Parent = new QComboBox(this);
        Parent->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        Parent->setMinimumContentsLength(8);
        Parent->addItem(tr("Не назначен"), QString{});
        AssetThumbnails::Bind(*Parent, 0, Runtime.GetAssetRegistry(), {});
        for (const auto& Type : {MaterialAssetType, MaterialInstanceAssetType})
        {
            for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(Type))
            {
                if (Entry.Metadata.Guid != Document.GetAsset().Asset)
                {
                    Parent->addItem(QString::fromStdString(Entry.VirtualPath), QString::fromStdString(Entry.Metadata.Guid.ToString()));
                    AssetThumbnails::Bind(*Parent, Parent->count() - 1, Runtime.GetAssetRegistry(), {Entry.Metadata.Guid, std::nullopt});
                }
            }
        }
        Parent->setCurrentIndex(Parent->findData(QString::fromStdString(Resolved.Parent.Asset.ToString())));
        Parent->setEnabled(!Document.IsReadOnly());
        Fields->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Родитель")), new AssetReferenceField(*Parent, Runtime.GetAssetRegistry()));
        connect(Parent, &QComboBox::activated, this, [this, Parent](int)
        {
            AssetKey Key;
            Guid::TryParse(Parent->currentData().toString().toStdString(), Key.Asset);
            auto Candidate = Session->GetDocument().GetDocument();
            Candidate["parent"] = AssetMetadataIO::AssetRefToJson(Key);
            Run(MaterialEditOperation::Document, Candidate);
        });
    }
    for (const auto& Definition : Resolved.Definition.Parameters)
    {
        auto* Row = new QWidget(this);
        Row->setMinimumHeight(36);
        if (Definition.DefaultValue.Type == MaterialParameterType::Color
            || Definition.DefaultValue.Type == MaterialParameterType::Vector4)
        {
            Row->setMinimumHeight(80);
        }
        auto* Layout = new QHBoxLayout(Row);
        Layout->setContentsMargins(0, 0, 0, 0);
        Layout->addWidget(MakeParameterField(Definition, Resolved.Values.at(Definition.Identifier)), 1);
        if (Session->IsInstance())
        {
            auto* Reset = BindEditorCaption(new QPushButton(tr("Наследовать"), Row), QString::fromUtf8("Наследовать"));
            Reset->setEnabled(!Document.IsReadOnly() && Draft.value("overrides", nlohmann::json::object()).contains(Definition.Identifier));
            Layout->addWidget(Reset);
            connect(Reset, &QPushButton::clicked, this, [this, Identifier = Definition.Identifier]()
            {
                auto Candidate = Session->GetDocument().GetDocument();
                Candidate["overrides"].erase(Identifier);
                Run(MaterialEditOperation::Document, Candidate);
            });
        }
        else
        {
            auto* Edit = BindEditorCaption(new QPushButton(tr("Настроить"), Row), QString::fromUtf8("Настроить"));
            auto* Remove = BindEditorCaption(new QPushButton(tr("Удалить"), Row), QString::fromUtf8("Удалить"));
            Edit->setEnabled(!Document.IsReadOnly());
            Remove->setEnabled(!Document.IsReadOnly());
            Layout->addWidget(Edit);
            Layout->addWidget(Remove);
            connect(Edit, &QPushButton::clicked, this, [this, Identifier = Definition.Identifier]() { EditParameterDefinition(Identifier); });
            connect(Remove, &QPushButton::clicked, this, [this, Identifier = Definition.Identifier]()
            {
                if (QMessageBox::question(this, tr("Удалить параметр"), tr("Удалить параметр %1? Проверьте использующий его HLSL и экземпляры.").arg(QString::fromStdString(Identifier))) != QMessageBox::Yes)
                {
                    return;
                }
                auto Candidate = Session->GetDocument().GetDocument();
                auto& Parameters = Candidate["parameters"];
                for (auto Iterator = Parameters.begin(); Iterator != Parameters.end(); ++Iterator)
                {
                    if (Iterator->at("id") == Identifier)
                    {
                        Parameters.erase(Iterator);
                        break;
                    }
                }
                Run(MaterialEditOperation::Document, Candidate);
            });
        }
        QString Label = QString::fromStdString(Definition.DisplayName);
        if (!Definition.Group.empty())
        {
            Label = QString::fromStdString(Definition.Group) + " / " + Label;
        }
        Fields->addRow(Label, Row);
    }
    for (const auto& Warning : Resolved.Warnings)
    {
        Fields->addRow(new QLabel(QString::fromStdString(Warning), this));
    }
    if (!Session->IsInstance())
    {
        auto* Add = BindEditorCaption(new QPushButton(tr("Добавить параметр"), this), QString::fromUtf8("Добавить параметр"));
        Add->setEnabled(!Document.IsReadOnly());
        Fields->addRow(Add);
        connect(Add, &QPushButton::clicked, this, [this]() { EditParameterDefinition(); });
    }
}

void MaterialEditorWindow::closeEvent(QCloseEvent* Event)
{
    if (!FlushSource())
    {
        Event->ignore();
        return;
    }
    if (Session->GetDocument().IsDirty())
    {
        const auto Answer = QMessageBox::question(this, tr("Сохранить материал"), tr("Сохранить изменения материала перед закрытием?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (Answer == QMessageBox::Cancel || (Answer == QMessageBox::Save && !Save()))
        {
            Event->ignore();
            return;
        }
    }
    Event->accept();
}

