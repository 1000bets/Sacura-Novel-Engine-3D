#include "UI/Views/InputWorkspaceView.h"
#include "Engine.h"
#include "Documents/InputPackage.h"
#include "Story/StoryDocumentIO.h"
#include <QFileDialog>
#include <algorithm>
#include "UI/GameInputAdapter.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

class InputKeyCapture : public QPushButton
{
public:
    InputKeyCapture(const QString& Label, QWidget* Parent = nullptr) : QPushButton(Label, Parent)
    {
        connect(this, &QPushButton::clicked, this, [this]()
        {
            bListening = true;
            setText(QString::fromUtf8("Нажмите клавишу…"));
            setFocus();
        });
    }
    std::function<void(const std::string&)> Captured;
protected:
    void keyPressEvent(QKeyEvent* Event) override
    {
        if (bListening)
        {
            bListening = false;
            const auto Control = GameInputAdapter::KeyControl(*Event);
            setText(QString::fromStdString(Control));
            if (Captured)
            {
                Captured(Control);
            }
            Event->accept();
            return;
        }
        QPushButton::keyPressEvent(Event);
    }
private:
    bool bListening = false;
};

static QComboBox* InputChoice(QFormLayout* Form, const QString& Label, const QStringList& Choices,
    int Current, const std::function<void(int)>& Changed)
{
    auto* Choice = new QComboBox;
    Choice->addItems(Choices);
    Choice->setCurrentIndex(Current);
    Form->addRow(Label, Choice);
    QObject::connect(Choice, &QComboBox::activated, Choice, Changed);
    return Choice;
}

static void InputNumber(QFormLayout* Form, const QString& Label, double Value, double Minimum,
    double Maximum, const std::function<void(double)>& Changed)
{
    auto* Number = new QDoubleSpinBox;
    Number->setRange(Minimum, Maximum);
    Number->setDecimals(3);
    Number->setSingleStep(0.1);
    Number->setValue(Value);
    Form->addRow(Label, Number);
    QObject::connect(Number, &QDoubleSpinBox::editingFinished, Number, [Number, Changed]() { Changed(Number->value()); });
}

InputWorkspaceView::InputWorkspaceView(Engine& InRuntime, QWidget* Parent)
    : QDialog(Parent), Runtime(InRuntime)
{
    setWindowTitle(tr("Ввод"));
    setObjectName("InputWorkspace");
    setProperty("uiRole", "workspaceFloating");
    resize(960, 720);
    auto* Layout = new QVBoxLayout(this);
    auto* Toolbar = new QGridLayout;
    Layout->addLayout(Toolbar);
    int ButtonIndex = 0;
    const auto Button = [&](const QString& Label, const std::function<void()>& Callback)
    {
        auto* Control = new QPushButton(Label, this);
        Toolbar->addWidget(Control, ButtonIndex / 4, ButtonIndex % 4);
        ++ButtonIndex;
        connect(Control, &QPushButton::clicked, this, Callback);
    };
    Button(tr("Действие +"), [this]() { CreateAsset(false); });
    Button(tr("Контекст +"), [this]() { CreateAsset(true); });
    Button(tr("Сохранить"), [this]() { Save(); });
    Button(tr("Отменить"), [this]() { Document.Undo(); RefreshEditor(); });
    Button(tr("Повторить"), [this]() { Document.Redo(); RefreshEditor(); });
    Button(tr("Начальные контексты"), [this]() { ShowSettings(); });
    Button(tr("Профиль"), [this]() { ShowProfile(); });
    Button(tr("События"), [this]() { ShowStoryBindings(); });
    auto* Exchange = new QHBoxLayout;
    Layout->addLayout(Exchange);
    auto* Export = new QPushButton(tr("Экспортировать с зависимостями"));
    auto* Import = new QPushButton(tr("Импортировать пакет"));
    Exchange->addWidget(Export);
    Exchange->addWidget(Import);
    Exchange->addStretch();
    connect(Export, &QPushButton::clicked, this, &InputWorkspaceView::ExportPackage);
    connect(Import, &QPushButton::clicked, this, &InputWorkspaceView::ImportPackage);
    auto* Content = new QHBoxLayout;
    Layout->addLayout(Content, 1);
    Library = new QListWidget(this);
    Library->setObjectName("InputAssetLibrary");
    Library->setMaximumWidth(180);
    Content->addWidget(Library);
    auto* Scroll = new QScrollArea(this);
    Scroll->setWidgetResizable(true);
    Editor = new QWidget;
    Editor->setProperty("uiRole", "workspaceFloating");
    EditorLayout = new QVBoxLayout(Editor);
    Scroll->setWidget(Editor);
    Content->addWidget(Scroll, 1);
    Diagnostic = new QLabel(this);
    Diagnostic->setWordWrap(true);
    Layout->addWidget(Diagnostic);
    auto* Monitor = new QLabel(this);
    Monitor->setWordWrap(true);
    Layout->addWidget(Monitor);
    auto* Timer = new QTimer(this);
    Timer->setInterval(100);
    connect(Timer, &QTimer::timeout, this, [this, Monitor]()
    {
        QString Text;
        if (auto* Session = Runtime.GetInputSession())
        {
            Text = tr("Активные контексты: ");
            for (const auto& Activation : Session->GetContexts())
            {
                for (const auto& Context : Session->GetConfiguration().Contexts)
                {
                    if (Context.Identifier == Activation.Context)
                    {
                        Text += QString::fromStdString(Context.Name) + " ";
                    }
                }
            }
            for (const auto& Action : Session->GetConfiguration().Actions)
            {
                const auto Value = Session->GetValue(Action.Identifier);
                if (Value.Magnitude() > 0.f)
                {
                    Text += QString("\n%1: %2, %3, %4").arg(QString::fromStdString(Action.Name)).arg(Value.Horizontal).arg(Value.Vertical).arg(Value.Depth);
                }
            }
        }
        else
        {
            Text = tr("Запустите игру для проверки ввода. Изменения применяются при следующем запуске.");
        }
        Monitor->setText(Text);
    });
    Timer->start();
    connect(Library, &QListWidget::itemActivated, this, [this](QListWidgetItem* Item)
    {
        AssetRegistryEntry Entry;
        if (Runtime.GetAssetRegistry().TryGetByPath(Item->data(Qt::UserRole).toString().toStdString(), Entry))
        {
            OpenAsset(Entry);
        }
    });
    auto* SaveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(SaveShortcut, &QShortcut::activated, this, [this]() { Save(); });
    auto* UndoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(UndoShortcut, &QShortcut::activated, this, [this]() { Document.Undo(); RefreshEditor(); });
    auto* RedoShortcut = new QShortcut(QKeySequence::Redo, this);
    connect(RedoShortcut, &QShortcut::activated, this, [this]() { Document.Redo(); RefreshEditor(); });
    RefreshLibrary();
    RefreshEditor();
}

bool InputWorkspaceView::ConfirmDiscard()
{
    if (!bOpen || !Document.IsDirty())
    {
        return true;
    }
    const auto Answer = QMessageBox::question(this, tr("Ввод"), tr("Сохранить изменения?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (Answer == QMessageBox::Save)
    {
        return Save();
    }
    return Answer == QMessageBox::Discard;
}

void InputWorkspaceView::closeEvent(QCloseEvent* Event)
{
    if (ConfirmDiscard())
    {
        Event->accept();
    }
    else
    {
        Event->ignore();
    }
}

void InputWorkspaceView::RefreshLibrary()
{
    Library->clear();
    for (const auto& Type : {InputActionAssetType, InputMappingContextAssetType})
    {
        for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(Type))
        {
            std::string Name = std::filesystem::path(Entry.RelativePath).stem().u8string();
            nlohmann::json Data;
            std::string Error;
            if (InputDocument::Read(Entry.AbsolutePath, Data, Error))
            {
                std::string Field = "Action";
                if (Type == InputMappingContextAssetType)
                {
                    Field = "Context";
                }
                if (Data.contains(Field) && Data[Field].is_object() && Data[Field].value("Name", nlohmann::json()).is_string())
                {
                    Name = Data[Field]["Name"].get<std::string>();
                }
            }
            auto* Item = new QListWidgetItem(QString::fromStdString(Name), Library);
            Item->setData(Qt::UserRole, QString::fromStdString(Entry.VirtualPath));
            Item->setToolTip(QString::fromStdString(Entry.VirtualPath));
        }
    }
}

void InputWorkspaceView::OpenAsset(const AssetRegistryEntry& Entry)
{
    if (!ConfirmDiscard())
    {
        return;
    }
    std::string Error;
    if (!Document.Open(Runtime.GetAssetRegistry(), Entry, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    bOpen = true;
    RefreshEditor();
}

void InputWorkspaceView::CreateAsset(bool bContext)
{
    if (!ConfirmDiscard())
    {
        return;
    }
    bool bAccepted = false;
    const auto Name = QInputDialog::getText(this, tr("Новый ресурс ввода"), tr("Название"), QLineEdit::Normal, {}, &bAccepted);
    if (!bAccepted)
    {
        return;
    }
    AssetRegistryEntry Entry;
    std::string Error;
    if (!InputAssetDocument::Create(Runtime.GetAssetRegistry(), Name.trimmed().toStdString(), bContext, Entry, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    bOpen = false;
    RefreshLibrary();
    OpenAsset(Entry);
}

void InputWorkspaceView::Edit(const std::function<void(nlohmann::json&)>& Change)
{
    auto Candidate = Document.GetData();
    Change(Candidate);
    std::string Error;
    if (!Document.Apply(std::move(Candidate), Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    Diagnostic->clear();
    RefreshEditor();
}

bool InputWorkspaceView::Save()
{
    if (auto* Focused = focusWidget())
    {
        Focused->clearFocus();
    }
    if (!bOpen)
    {
        return true;
    }
    std::string Error;
    if (!Document.Save(Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return false;
    }
    Diagnostic->setText(tr("Сохранено"));
    RefreshLibrary();
    RefreshEditor();
    return true;
}

void InputWorkspaceView::RefreshEditor()
{
    while (auto* Item = EditorLayout->takeAt(0))
    {
        if (Item->widget())
        {
            Item->widget()->hide();
            Item->widget()->deleteLater();
        }
        delete Item;
    }
    if (!bOpen)
    {
        EditorLayout->addWidget(new QLabel(tr("Создайте действие и контекст или откройте ресурс двойным щелчком.")));
        EditorLayout->addStretch();
        return;
    }
    auto* Panel = new QWidget;
    Panel->setEnabled(!Document.IsReadOnly());
    EditorLayout->addWidget(Panel);
    auto* Form = new QFormLayout(Panel);
    Form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const bool bAction = Document.GetEntry().Metadata.Type == InputActionAssetType;
    std::string Root = "Context";
    if (bAction)
    {
        Root = "Action";
    }
    const auto& Data = Document.GetData().at(Root);
    auto* Name = new QLineEdit(QString::fromStdString(Data.value("Name", std::string{})));
    Name->setObjectName("InputAssetName");
    Form->addRow(tr("Название"), Name);
    connect(Name, &QLineEdit::editingFinished, this, [this, Name, Root]()
    {
        Edit([&](auto& Candidate) { Candidate[Root]["Name"] = Name->text().toStdString(); });
    });
    QString Suffix;
    if (Document.IsDirty())
    {
        Suffix = " *";
    }
    setWindowTitle(tr("Ввод — %1%2").arg(Name->text(), Suffix));
    InputConfiguration Configuration;
    std::string Error;
    InputDocument::LoadConfiguration(Runtime.GetAssetRegistry(), Configuration, Error, Runtime.GetProjectFile());
    if (bAction)
    {
        const auto Action = Data.get<InputAction>();
        InputChoice(Form, tr("Тип значения"), {tr("Кнопка"), tr("Ось 1D"), tr("Оси 2D"), tr("Оси 3D")}, static_cast<int>(Action.ValueType),
            [this](int Value) { Edit([&](auto& Candidate) { Candidate["Action"]["ValueType"] = Value; }); });
        InputChoice(Form, tr("Срабатывание"), {tr("Нажатие"), tr("Отпускание"), tr("Удержание"), tr("Пока активно")}, static_cast<int>(Action.Trigger),
            [this](int Value) { Edit([&](auto& Candidate) { Candidate["Action"]["Trigger"] = Value; }); });
        if (Action.Trigger == InputTrigger::Hold)
        {
            InputNumber(Form, tr("Удержание, сек"), Action.HoldSeconds, 0, 60,
                [this](double Value) { Edit([&](auto& Candidate) { Candidate["Action"]["HoldSeconds"] = Value; }); });
        }
        InputChoice(Form, tr("Объединение привязок"), {tr("Сумма с ограничением"), tr("Наибольшее значение")}, static_cast<int>(Action.Accumulation),
            [this](int Value) { Edit([&](auto& Candidate) { Candidate["Action"]["Accumulation"] = Value; }); });
        auto* Chord = new QComboBox;
        Chord->addItem(tr("Не требуется"), "");
        for (const auto& Other : Configuration.Actions)
        {
            if (Other.Identifier != Document.GetEntry().Metadata.Guid.ToString())
            {
                Chord->addItem(QString::fromStdString(Other.Name), QString::fromStdString(Other.Identifier));
            }
        }
        Chord->setCurrentIndex(Chord->findData(QString::fromStdString(Action.ChordAction)));
        Form->addRow(tr("Удерживать другое действие"), Chord);
        connect(Chord, &QComboBox::activated, this, [this, Chord]()
        {
            Edit([&](auto& Candidate) { Candidate["Action"]["ChordAction"] = Chord->currentData().toString().toStdString(); });
        });
        auto* Paused = new QCheckBox(tr("Работает во время паузы"));
        Paused->setChecked(Action.bExecuteWhenPaused);
        Form->addRow(Paused);
        connect(Paused, &QCheckBox::toggled, this, [this](bool bChecked)
        {
            Edit([&](auto& Candidate) { Candidate["Action"]["bExecuteWhenPaused"] = bChecked; });
        });
        InputNumber(Form, tr("Порог кнопки"), Action.Threshold, 0.001, 1,
            [this](double Value) { Edit([&](auto& Candidate) { Candidate["Action"]["Threshold"] = Value; }); });
        InputNumber(Form, tr("Мёртвая зона действия"), Action.Modifiers.DeadZone, 0, 0.99,
            [this](double Value) { Edit([&](auto& Candidate) { Candidate["Action"]["Modifiers"]["DeadZone"] = Value; }); });
        auto* Normalize = new QCheckBox(tr("Нормализовать значение"));
        Normalize->setChecked(Action.Modifiers.bNormalize);
        Form->addRow(Normalize);
        connect(Normalize, &QCheckBox::toggled, this, [this](bool bChecked)
        {
            Edit([&](auto& Candidate) { Candidate["Action"]["Modifiers"]["bNormalize"] = bChecked; });
        });
        for (const std::string Axis : {"Horizontal", "Vertical", "Depth"})
        {
            InputNumber(Form, tr("Масштаб %1").arg(QString::fromStdString(Axis)), nlohmann::json(Action.Modifiers.Scale).at(Axis).get<double>(), -100, 100,
                [this, Axis](double Value) { Edit([&](auto& Candidate) { Candidate["Action"]["Modifiers"]["Scale"][Axis] = Value; }); });
        }
    }
    else
    {
        const auto Context = Data.get<InputMappingContext>();
        for (size_t Index = 0; Index < Context.Bindings.size(); ++Index)
        {
            const auto Binding = Context.Bindings[Index];
            auto* Group = new QWidget;
            auto* BindingForm = new QFormLayout(Group);
            Form->addRow(Group);
            auto* ActionChoice = new QComboBox;
            for (const auto& Action : Configuration.Actions)
            {
                ActionChoice->addItem(QString::fromStdString(Action.Name), QString::fromStdString(Action.Identifier));
            }
            ActionChoice->setCurrentIndex(ActionChoice->findData(QString::fromStdString(Binding.Action)));
            BindingForm->addRow(tr("Действие"), ActionChoice);
            connect(ActionChoice, &QComboBox::activated, this, [this, Index, ActionChoice]()
            {
                Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"][Index]["Action"] = ActionChoice->currentData().toString().toStdString(); });
            });
            const QStringList Devices{"Keyboard", "Mouse", "Gamepad", "Touch", "External"};
            InputChoice(BindingForm, tr("Устройство"), {tr("Клавиатура"), tr("Мышь"), tr("Геймпад"), tr("Экранное"), tr("Внешнее")}, Devices.indexOf(QString::fromStdString(Binding.Device)),
                [this, Index, Devices](int Value)
                {
                    Edit([&](auto& Candidate)
                    {
                        Candidate["Context"]["Bindings"][Index]["Device"] = Devices[Value].toStdString();
                        const std::array<std::string, 5> Defaults{"KeyE", "Primary", "South", "Button", "Button"};
                        Candidate["Context"]["Bindings"][Index]["Control"] = Defaults[Value];
                    });
                });
            if (Binding.Device == "Keyboard")
            {
                auto* Capture = new InputKeyCapture(QString::fromStdString(Binding.Control));
                BindingForm->addRow(tr("Клавиша"), Capture);
                Capture->Captured = [this, Index](const std::string& Control)
                {
                    Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"][Index]["Control"] = Control; });
                };
            }
            else
            {
                auto* Control = new QComboBox;
                if (Binding.Device == "Mouse")
                {
                    Control->addItems({"Primary", "Secondary", "Middle", "Delta", "Wheel"});
                }
                else if (Binding.Device == "Gamepad")
                {
                    Control->addItems({"South", "East", "West", "North", "Start", "Back", "LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger", "LeftShoulder", "RightShoulder", "LeftStick", "RightStick", "DpadUp", "DpadDown", "DpadLeft", "DpadRight"});
                }
                else
                {
                    Control->setEditable(true);
                    Control->addItem(QString::fromStdString(Binding.Control));
                }
                Control->setCurrentText(QString::fromStdString(Binding.Control));
                BindingForm->addRow(tr("Кнопка или ось"), Control);
                connect(Control, &QComboBox::currentTextChanged, this, [this, Index](const QString& Value)
                {
                    Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"][Index]["Control"] = Value.toStdString(); });
                });
            }
            int Direction = 3;
            if (Binding.Modifiers.AxisOrder == std::array<int, 3>{1, 0, 2})
            {
                Direction = 1;
            }
            else if (Binding.Modifiers.AxisOrder == std::array<int, 3>{2, 1, 0})
            {
                Direction = 2;
            }
            InputChoice(BindingForm, tr("Направить значение"), {tr("По X"), tr("По Y"), tr("По Z"), tr("Сохранить оси")}, Direction,
                [this, Index](int Value)
                {
                    Edit([&](auto& Candidate)
                    {
                        auto& Modifiers = Candidate["Context"]["Bindings"][Index]["Modifiers"];
                        Modifiers["AxisOrder"] = std::array<int, 3>{0, 1, 2};
                        if (Value == 1)
                        {
                            Modifiers["AxisOrder"] = std::array<int, 3>{1, 0, 2};
                        }
                        else if (Value == 2)
                        {
                            Modifiers["AxisOrder"] = std::array<int, 3>{2, 1, 0};
                        }
                    });
                });
            for (const std::string Axis : {"Horizontal", "Vertical", "Depth"})
            {
                InputNumber(BindingForm, tr("Масштаб %1").arg(QString::fromStdString(Axis)), nlohmann::json(Binding.Modifiers.Scale).at(Axis).get<double>(), -100, 100,
                    [this, Index, Axis](double Value) { Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"][Index]["Modifiers"]["Scale"][Axis] = Value; }); });
            }
            InputNumber(BindingForm, tr("Мёртвая зона"), Binding.Modifiers.DeadZone, 0, 0.99,
                [this, Index](double Value) { Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"][Index]["Modifiers"]["DeadZone"] = Value; }); });
            auto* Consume = new QCheckBox(tr("Перехватывать ввод"));
            Consume->setChecked(Binding.bConsume);
            BindingForm->addRow(Consume);
            connect(Consume, &QCheckBox::toggled, this, [this, Index](bool bChecked)
            {
                Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"][Index]["bConsume"] = bChecked; });
            });
            auto* Remove = new QPushButton(tr("Удалить привязку"));
            BindingForm->addRow(Remove);
            connect(Remove, &QPushButton::clicked, this, [this, Index]()
            {
                Edit([&](auto& Candidate) { Candidate["Context"]["Bindings"].erase(Index); });
            });
        }
        auto* Add = new QPushButton(tr("Добавить привязку"));
        Form->addRow(Add);
        connect(Add, &QPushButton::clicked, this, [this, Configuration]()
        {
            if (Configuration.Actions.empty())
            {
                return;
            }
            Edit([&](auto& Candidate)
            {
                InputBinding Binding;
                Binding.Identifier = Guid::Generate().ToString();
                Binding.Action = Configuration.Actions.front().Identifier;
                auto Context = Candidate["Context"].template get<InputMappingContext>();
                Context.Bindings.push_back(Binding);
                Candidate["Context"] = Context;
            });
        });
    }
    EditorLayout->addStretch();
}

void InputWorkspaceView::ShowSettings()
{
    InputConfiguration Configuration;
    std::string Error;
    if (!InputDocument::LoadConfiguration(Runtime.GetAssetRegistry(), Configuration, Error, Runtime.GetProjectFile()))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    QDialog Settings(this);
    Settings.setWindowTitle(tr("Начальные контексты проекта"));
    auto* Form = new QFormLayout(&Settings);
    std::vector<std::pair<QCheckBox*, QSpinBox*>> Controls;
    for (const auto& Context : Configuration.Contexts)
    {
        auto* Enabled = new QCheckBox(QString::fromStdString(Context.Name));
        auto* Priority = new QSpinBox;
        Priority->setRange(-10000, 10000);
        for (const auto& Activation : Configuration.StartupContexts)
        {
            if (Activation.Context == Context.Identifier)
            {
                Enabled->setChecked(true);
                Priority->setValue(Activation.Priority);
            }
        }
        Form->addRow(Enabled, Priority);
        Controls.push_back({Enabled, Priority});
    }
    auto* Apply = new QPushButton(tr("Сохранить"));
    Form->addRow(new QLabel(tr("Число справа — приоритет контекста.")));
    Form->addRow(Apply);
    connect(Apply, &QPushButton::clicked, &Settings, &QDialog::accept);
    if (Settings.exec() != QDialog::Accepted)
    {
        return;
    }
    std::vector<InputContextActivation> Activations;
    for (size_t Index = 0; Index < Controls.size(); ++Index)
    {
        if (Controls[Index].first->isChecked())
        {
            Activations.push_back({Configuration.Contexts[Index].Identifier, Controls[Index].second->value()});
        }
    }
    if (!InputDocument::SaveStartupContexts(Runtime.GetProjectFile(), Activations, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
    }
}

void InputWorkspaceView::ShowProfile()
{
    InputConfiguration Configuration;
    std::string Error;
    if (!InputDocument::LoadConfiguration(Runtime.GetAssetRegistry(), Configuration, Error, Runtime.GetProjectFile()))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    QDialog Settings(this);
    Settings.setWindowTitle(tr("Пользовательские переназначения"));
    Settings.resize(520, 500);
    auto* Layout = new QVBoxLayout(&Settings);
    auto* Scroll = new QScrollArea;
    Scroll->setWidgetResizable(true);
    auto* Panel = new QWidget;
    auto* Form = new QFormLayout(Panel);
    Scroll->setWidget(Panel);
    Layout->addWidget(Scroll);
    auto Overrides = Configuration.Overrides;
    for (const auto& Context : Configuration.Contexts)
    {
        for (const auto& Binding : Context.Bindings)
        {
            InputBindingOverride Override{Context.Identifier, Binding.Identifier, Binding.Device, Binding.Control};
            for (const auto& Existing : Overrides)
            {
                if (Existing.Context == Context.Identifier && Existing.Binding == Binding.Identifier)
                {
                    Override = Existing;
                }
            }
            const auto Change = [&Overrides, Override](const std::string& Control, double Sensitivity)
            {
                auto Replacement = Override;
                Replacement.Control = Control;
                Replacement.Sensitivity = {static_cast<float>(Sensitivity), static_cast<float>(Sensitivity), static_cast<float>(Sensitivity)};
                Overrides.erase(std::remove_if(Overrides.begin(), Overrides.end(), [&](const auto& Existing)
                {
                    return Existing.Context == Replacement.Context && Existing.Binding == Replacement.Binding;
                }), Overrides.end());
                Overrides.push_back(Replacement);
            };
            QString Label = QString::fromStdString(Context.Name);
            for (const auto& Action : Configuration.Actions)
            {
                if (Action.Identifier == Binding.Action)
                {
                    Label += " / " + QString::fromStdString(Action.Name);
                }
            }
            if (Override.Device == "Keyboard")
            {
                auto* Capture = new InputKeyCapture(QString::fromStdString(Override.Control));
                Capture->Captured = [Change, Override](const std::string& Control) { Change(Control, Override.Sensitivity.Horizontal); };
                Form->addRow(Label, Capture);
            }
            else
            {
                auto* Control = new QComboBox;
                Control->setEditable(true);
                if (Override.Device == "Mouse")
                {
                    Control->addItems({"Primary", "Secondary", "Middle", "Delta", "Wheel"});
                }
                else if (Override.Device == "Gamepad")
                {
                    Control->addItems({"South", "East", "West", "North", "Start", "Back", "LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger", "LeftShoulder", "RightShoulder", "LeftStick", "RightStick", "DpadUp", "DpadDown", "DpadLeft", "DpadRight"});
                }
                Control->setCurrentText(QString::fromStdString(Override.Control));
                Form->addRow(Label, Control);
                auto SensitivityValue = std::make_shared<double>(Override.Sensitivity.Horizontal);
                connect(Control, &QComboBox::currentTextChanged, &Settings, [Change, SensitivityValue](const QString& Value)
                {
                    Change(Value.toStdString(), *SensitivityValue);
                });
                InputNumber(Form, Label + tr(" — чувствительность"), Override.Sensitivity.Horizontal, -100, 100,
                    [Change, Control, SensitivityValue](double Sensitivity)
                    {
                        *SensitivityValue = Sensitivity;
                        Change(Control->currentText().toStdString(), Sensitivity);
                    });
            }
        }
    }
    auto* Reset = new QPushButton(tr("Сбросить переназначения"));
    Layout->addWidget(Reset);
    connect(Reset, &QPushButton::clicked, &Settings, [&Overrides, &Settings]() { Overrides.clear(); Settings.accept(); });
    auto* SaveButton = new QPushButton(tr("Сохранить профиль"));
    Layout->addWidget(SaveButton);
    connect(SaveButton, &QPushButton::clicked, &Settings, &QDialog::accept);
    if (Settings.exec() == QDialog::Accepted
        && !InputDocument::SaveProfile(Runtime.GetAssetRegistry().GetGameContentRoot(), Overrides, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
    }
}

void InputWorkspaceView::ExportPackage()
{
    if (!bOpen || !Save())
    {
        return;
    }
    const auto Filename = QFileDialog::getSaveFileName(this, tr("Экспорт ввода"), {}, tr("Пакет ввода (*.inputpackage)"));
    if (Filename.isEmpty())
    {
        return;
    }
    std::string Error;
    if (!InputPackage::Export(Runtime.GetAssetRegistry(), Document.GetEntry(), std::filesystem::path(Filename.toStdWString()), Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
    }
    else
    {
        Diagnostic->setText(tr("Пакет сохранён с зависимостями."));
    }
}

void InputWorkspaceView::ShowStoryBindings()
{
    InputConfiguration Configuration;
    StoryDocument Story;
    std::string Error;
    const auto Loaded = StoryDocumentIO::LoadFromFile(Runtime.GetStartupStory(), Story);
    if (!Loaded.bOk || !InputDocument::LoadConfiguration(Runtime.GetAssetRegistry(), Configuration, Error, Runtime.GetProjectFile()))
    {
        Diagnostic->setText(tr("Сначала назначьте начальную историю проекта и создайте в ней события. ")
            + QString::fromStdString(Loaded.Error + Error));
        return;
    }
    if (Story.Events.empty())
    {
        Diagnostic->setText(tr("В начальной истории пока нет событий."));
        return;
    }
    const auto Path = Runtime.GetProjectFile();
    std::vector<InputStoryBinding> Bindings;
    nlohmann::json SettingsData;
    if (std::filesystem::exists(Path))
    {
        nlohmann::json Data;
        if (!InputDocument::ReadProjectInput(Path, Data, Error))
        {
            Diagnostic->setText(QString::fromStdString(Error));
            return;
        }
        try
        {
            SettingsData = Data;
            Bindings = Data.value("StoryBindings", std::vector<InputStoryBinding>{});
        }
        catch (const std::exception& Exception)
        {
            Diagnostic->setText(QString::fromUtf8(Exception.what()));
            return;
        }
    }
    QDialog Settings(this);
    Settings.setWindowTitle(tr("Действия ввода → события истории"));
    auto* Layout = new QVBoxLayout(&Settings);
    auto* List = new QListWidget;
    Layout->addWidget(List);
    const auto Refresh = [&]()
    {
        List->clear();
        for (const auto& Binding : Bindings)
        {
            QString ActionName = tr("Удалённое действие");
            QString EventName = tr("Удалённое событие");
            for (const auto& Action : Configuration.Actions)
            {
                if (Action.Identifier == Binding.Action)
                {
                    ActionName = QString::fromStdString(Action.Name);
                }
            }
            for (const auto& Event : Story.Events)
            {
                if (Event.Id == Binding.Event)
                {
                    EventName = QString::fromStdString(Event.Name);
                }
            }
            List->addItem(ActionName + " → " + EventName + " (" + QString::fromUtf8(InputPhaseName(Binding.Phase)) + ")");
        }
    };
    Refresh();
    auto* Form = new QFormLayout;
    Layout->addLayout(Form);
    auto* Action = new QComboBox;
    for (const auto& Definition : Configuration.Actions)
    {
        Action->addItem(QString::fromStdString(Definition.Name), QString::fromStdString(Definition.Identifier));
    }
    auto* Phase = new QComboBox;
    Phase->addItems({"Started", "Ongoing", "Triggered", "Completed", "Canceled"});
    Phase->setCurrentIndex(2);
    auto* Event = new QComboBox;
    for (const auto& Definition : Story.Events)
    {
        Event->addItem(QString::fromStdString(Definition.Name), QString::fromStdString(Definition.Id));
    }
    Form->addRow(tr("Действие"), Action);
    Form->addRow(tr("Фаза"), Phase);
    Form->addRow(tr("Событие"), Event);
    auto* Add = new QPushButton(tr("Добавить подписку"));
    auto* Remove = new QPushButton(tr("Удалить выбранную"));
    auto* SaveButton = new QPushButton(tr("Сохранить"));
    Layout->addWidget(Add);
    Layout->addWidget(Remove);
    Layout->addWidget(SaveButton);
    connect(Add, &QPushButton::clicked, &Settings, [&]()
    {
        Bindings.push_back({Action->currentData().toString().toStdString(), static_cast<InputPhase>(Phase->currentIndex()),
            Event->currentData().toString().toStdString()});
        Refresh();
    });
    connect(Remove, &QPushButton::clicked, &Settings, [&]()
    {
        const int Index = List->currentRow();
        if (Index >= 0)
        {
            Bindings.erase(Bindings.begin() + Index);
            Refresh();
        }
    });
    connect(SaveButton, &QPushButton::clicked, &Settings, &QDialog::accept);
    if (Settings.exec() == QDialog::Accepted)
    {
        SettingsData["StoryBindings"] = Bindings;
        if (!InputDocument::SaveProjectInput(Path, SettingsData, Error))
        {
            Diagnostic->setText(QString::fromStdString(Error));
        }
    }
}

void InputWorkspaceView::ImportPackage()
{
    if (!ConfirmDiscard())
    {
        return;
    }
    const auto Filename = QFileDialog::getOpenFileName(this, tr("Импорт ввода"), {}, tr("Пакет ввода (*.inputpackage)"));
    if (Filename.isEmpty())
    {
        return;
    }
    nlohmann::json Package;
    std::string Error;
    if (!InputDocument::Read(std::filesystem::path(Filename.toStdWString()), Package, Error) || !InputPackage::Validate(Package, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    QString Preview = tr("Будут созданы независимые копии:\n");
    for (const auto& Asset : Package.at("Assets"))
    {
        std::string Root = "Action";
        if (Asset.at("Type") == "InputMappingContext")
        {
            Root = "Context";
        }
        Preview += QString::fromStdString(Asset.at("Document").at(Root).value("Name", std::string{})) + "\n";
    }
    Preview += tr("Имена файлов и идентификаторы будут новыми. После импорта включите нужные начальные контексты.");
    if (QMessageBox::question(this, tr("Состав пакета"), Preview) != QMessageBox::Yes)
    {
        return;
    }
    std::vector<AssetRegistryEntry> Imported;
    if (!InputPackage::Import(Runtime.GetAssetRegistry(), Package, Imported, Error))
    {
        Diagnostic->setText(QString::fromStdString(Error));
        return;
    }
    bOpen = false;
    RefreshLibrary();
    if (!Imported.empty())
    {
        OpenAsset(Imported.front());
    }
}
