#include "UI/Presentation/LocalizedEditorUi.h"
#include "Localization/EditorLocalization.h"
#include <QComboBox>
#include "Shell/EditorMainWindow.h"
#include "Automation/EditorToolset.h"
#include "Automation/EditorMcpServer.h"
#include "Project/ProjectSession.h"
#include "Core/Threading/ThreadContext.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QTextCursor>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

QJsonObject EditorMainWindow::ReadProjectSettings() const
{
    const ProjectDescriptor* Project = BoundSession.GetProject();
    if (Project == nullptr)
    {
        return {};
    }
    auto RelativePath = [Project](const std::filesystem::path& Path)
    {
        if (Path.empty())
        {
            return QString();
        }
        return QString::fromStdString(Path.lexically_relative(Project->ProjectRoot).generic_string());
    };
    return {{"name", QString::fromStdString(Project->Name)},
            {"engineVersion", QString::fromStdString(Project->EngineVersion)},
            {"startupScene", RelativePath(Project->StartupScene)}, {"startupStory", RelativePath(Project->StartupStory)}};
}

QJsonObject EditorMainWindow::ApplyProjectSettings(const QJsonObject& Settings)
{
    if (BoundSession.GetProject() == nullptr)
    {
        return EditorToolset::Failure("No project is open");
    }
    ProjectDescriptor Updated = *BoundSession.GetProject();
    if (Settings.contains("name"))
    {
        Updated.Name = Settings.value("name").toString().trimmed().toStdString();
    }
    for (const QString& Key : {QString("startupScene"), QString("startupStory")})
    {
        if (!Settings.contains(Key))
        {
            continue;
        }
        const std::filesystem::path Relative(Settings.value(Key).toString().toStdWString());
        if (Relative.is_absolute() || Relative.has_root_name())
        {
            return EditorToolset::Failure("Startup paths must be project-relative");
        }
        std::filesystem::path Absolute;
        if (!Relative.empty())
        {
            Absolute = (Updated.ProjectRoot / Relative).lexically_normal();
            std::string ExpectedExtension = ".scene";
            if (Key == "startupStory")
            {
                ExpectedExtension = ".story";
            }
            if (Absolute.extension() != ExpectedExtension)
            {
                return EditorToolset::Failure("Unexpected startup document extension");
            }
        }
        if (Key == "startupScene")
        {
            Updated.StartupScene = Absolute;
        }
        else
        {
            Updated.StartupStory = Absolute;
        }
    }
    std::string Error;
    if (!BoundSession.UpdateSettings(Updated, Error))
    {
        return EditorToolset::Failure(QString::fromStdString(Error));
    }
    RefreshProjectTitle();
    PrintString("Project settings saved. Startup documents apply on the next project open.");
    return EditorToolset::Success(ReadProjectSettings());
}

QJsonObject EditorMainWindow::ReadEditorSettings() const
{
    QSettings Settings;
    return {{"language", EditorLocalization::Get().GetLanguage()}, {"cameraSpeed", Settings.value("Editor/CameraSpeed", 8.0).toDouble()},
            {"consoleLines", Settings.value("Editor/ConsoleLines", 2000).toInt()},
            {"mcpAutoStart", Settings.value("Editor/McpAutoStart", false).toBool()}};
}

QJsonObject EditorMainWindow::ApplyEditorSettings(const QJsonObject& Values)
{
    QJsonObject Updated = ReadEditorSettings();
    for (auto Entry = Values.begin(); Entry != Values.end(); ++Entry)
    {
        Updated.insert(Entry.key(), Entry.value());
    }
    const double Speed = Updated.value("cameraSpeed").toDouble();
    const double Lines = Updated.value("consoleLines").toDouble();
    if (Speed < 0.25 || Speed > 250.0 || Lines < 100 || Lines > 10000 || Lines != static_cast<int>(Lines))
    {
        return EditorToolset::Failure("Camera speed must be 0.25..250; console lines must be an integer in 100..10000");
    }
    if (!EditorLocalization::Get().SetLanguage(Updated.value("language").toString()))
    {
        return EditorToolset::Failure("Language must be en or ru and preferences must be writable");
    }
    QSettings Settings;
    Settings.setValue("Editor/CameraSpeed", Speed);
    Settings.setValue("Editor/ConsoleLines", static_cast<int>(Lines));
    Settings.setValue("Editor/McpAutoStart", Updated.value("mcpAutoStart").toBool());
    Settings.sync();
    if (Settings.status() != QSettings::NoError)
    {
        return EditorToolset::Failure("Failed to persist editor settings");
    }
    CameraSpeedSpin->setValue(Speed);
    if (ConsoleOutput != nullptr)
    {
        ConsoleOutput->setMaximumBlockCount(static_cast<int>(Lines));
    }
    return EditorToolset::Success(Updated);
}

void EditorMainWindow::ShowSettings(int Page)
{
    QDialog Dialog(this);
    Dialog.setObjectName("EditorSettingsDialog");
    BindEditorProperty(&Dialog, "windowTitle", QString::fromUtf8("Настройки"));
    Dialog.resize(760, 580);
    auto* Layout = new QVBoxLayout(&Dialog);
    auto* Tabs = new QTabWidget(&Dialog);
    Layout->addWidget(Tabs);
    auto* Status = new QLabel(&Dialog);
    Status->setWordWrap(true);
    Status->setMinimumHeight(48);
    Layout->addWidget(Status);

    auto* ProjectPage = new QWidget(Tabs);
    auto* ProjectForm = new QFormLayout(ProjectPage);
    const QJsonObject Project = ReadProjectSettings();
    auto* Name = new QLineEdit(Project.value("name").toString(), ProjectPage);
    Name->setObjectName("ProjectSettingsName");
    auto* ScenePath = new QLineEdit(Project.value("startupScene").toString(), ProjectPage);
    auto* StoryPath = new QLineEdit(Project.value("startupStory").toString(), ProjectPage);
    ProjectForm->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Название")), Name);
    ProjectForm->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Версия движка")), new QLabel(Project.value("engineVersion").toString(), ProjectPage));
    ProjectForm->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Стартовая сцена")), ScenePath);
    ProjectForm->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Стартовая история")), StoryPath);
    auto* Hint = BindEditorCaption(new QLabel(tr("Пути относительно каталога проекта. Пустая история отключает её автозапуск. Стартовые документы применяются при следующем открытии проекта."), ProjectPage), QString::fromUtf8("Пути относительно каталога проекта. Пустая история отключает её автозапуск. Стартовые документы применяются при следующем открытии проекта."));
    Hint->setWordWrap(true);
    ProjectForm->addRow(Hint);
    auto* SaveProject = BindEditorCaption(new QPushButton(tr("Сохранить настройки проекта"), ProjectPage), QString::fromUtf8("Сохранить настройки проекта"));
    SaveProject->setObjectName("SaveProjectSettings");
    ProjectForm->addRow(SaveProject);
    connect(SaveProject, &QPushButton::clicked, &Dialog, [this, Status, Name, ScenePath, StoryPath]()
    {
        const QJsonObject Result = Toolset->Call("project_settings_set", {{"name", Name->text()}, {"startupScene", ScenePath->text()}, {"startupStory", StoryPath->text()}});
        if (Result.value("isError").toBool())
        {
            Status->setText(Result.value("content").toArray().first().toObject().value("text").toString());
            return;
        }
        Status->setText(tr("Настройки проекта сохранены."));
    });
    Tabs->addTab(ProjectPage, tr("Проект"));

    auto* EditorPage = new QWidget(Tabs);
    auto* EditorForm = new QFormLayout(EditorPage);
    const QJsonObject Preferences = ReadEditorSettings();
    auto* Language = new QComboBox(EditorPage);
    Language->setObjectName("EditorLanguage");
    Language->addItem("English", "en");
    Language->addItem(QString::fromUtf8("Русский"), "ru");
    Language->setCurrentIndex(Language->findData(Preferences.value("language").toString()));
    EditorForm->addRow(BindEditorCaption(new QLabel(), "Interface language"), Language);
    auto* Speed = new QDoubleSpinBox(EditorPage);
    Speed->setRange(0.25, 250.0);
    Speed->setValue(Preferences.value("cameraSpeed").toDouble());
    auto* Lines = new QSpinBox(EditorPage);
    Lines->setRange(100, 10000);
    Lines->setValue(Preferences.value("consoleLines").toInt());
    auto* AutoStart = BindEditorCaption(new QCheckBox(tr("Включать MCP при открытии редактора"), EditorPage), QString::fromUtf8("Включать MCP при открытии редактора"));
    AutoStart->setChecked(Preferences.value("mcpAutoStart").toBool());
    EditorForm->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Скорость камеры")), Speed);
    EditorForm->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Строк в консоли")), Lines);
    EditorForm->addRow(AutoStart);
    auto* SaveEditor = BindEditorCaption(new QPushButton(tr("Сохранить настройки редактора"), EditorPage), QString::fromUtf8("Сохранить настройки редактора"));
    EditorForm->addRow(SaveEditor);
    connect(SaveEditor, &QPushButton::clicked, &Dialog, [this, Speed, Lines, AutoStart, Status, Language]()
    {
        const QJsonObject Result = Toolset->Call("editor_settings_set", {{"language", Language->currentData().toString()}, {"cameraSpeed", Speed->value()}, {"consoleLines", Lines->value()}, {"mcpAutoStart", AutoStart->isChecked()}});
        if (Result.value("isError").toBool())
        {
            Status->setText(Result.value("content").toArray().first().toObject().value("text").toString());
            return;
        }
        Status->setText(tr("Настройки редактора сохранены."));
    });
    Tabs->addTab(EditorPage, tr("Редактор"));
    EditorLocalization::Get().Bind(Tabs, [Tabs]()
    {
        Tabs->setTabText(0, EditorLocalization::Get().Caption("Project"));
        Tabs->setTabText(1, EditorLocalization::Get().Caption("Editor"));
    });

    auto* McpPage = new QWidget(Tabs);
    auto* McpLayout = new QVBoxLayout(McpPage);
    auto* ConnectionStatus = new QLabel(McpPage);
    ConnectionStatus->setWordWrap(true);
    McpLayout->addWidget(ConnectionStatus);
    auto* Toggle = new QPushButton(McpPage);
    Toggle->setObjectName("ToggleMcpServer");
    McpLayout->addWidget(Toggle);
    auto* Configuration = new QPlainTextEdit(McpPage);
    Configuration->setReadOnly(true);
    Configuration->setPlainText(QString::fromUtf8(QJsonDocument(McpServer->Configuration()).toJson(QJsonDocument::Indented)));
    McpLayout->addWidget(Configuration);
    auto UpdateConnection = [this, Toggle, ConnectionStatus]()
    {
        if (McpServer->IsRunning())
        {
            Toggle->setText(tr("Отключить MCP"));
            ConnectionStatus->setText(tr("MCP включён для этого редактора. Локальное соединение доступно только текущему пользователю."));
        }
        else
        {
            Toggle->setText(tr("Включить MCP"));
            ConnectionStatus->setText(tr("MCP отключён. Включите сервер и добавьте конфигурацию в MCP-клиент."));
        }
    };
    connect(Toggle, &QPushButton::clicked, &Dialog, [this, UpdateConnection, Status]()
    {
        if (McpServer->IsRunning())
        {
            McpServer->Stop();
        }
        else if (!McpServer->Start())
        {
            Status->setText(McpServer->Error());
        }
        UpdateConnection();
    });
    auto* Copy = BindEditorCaption(new QPushButton(tr("Сгенерировать и скопировать конфигурацию"), McpPage), QString::fromUtf8("Сгенерировать и скопировать конфигурацию"));
    Copy->setObjectName("CopyMcpConfiguration");
    McpLayout->addWidget(Copy);
    connect(Copy, &QPushButton::clicked, &Dialog, [this, Status, UpdateConnection, Configuration]()
    {
        if (!McpServer->Start())
        {
            Status->setText(McpServer->Error());
            return;
        }
        QApplication::clipboard()->setText(Configuration->toPlainText());
        Status->setText(tr("Конфигурация скопирована. Добавьте запись sakura-editor в настройки MCP-клиента. После перезапуска редактора сгенерируйте конфигурацию заново."));
        UpdateConnection();
    });
    auto* Export = BindEditorCaption(new QPushButton(tr("Сохранить конфигурацию JSON…"), McpPage), QString::fromUtf8("Сохранить конфигурацию JSON…"));
    McpLayout->addWidget(Export);
    connect(Export, &QPushButton::clicked, &Dialog, [this, Status, Configuration, UpdateConnection]()
    {
        const QString Path = QFileDialog::getSaveFileName(this, tr("Конфигурация MCP"), "sakura-mcp.json", "JSON (*.json)");
        if (Path.isEmpty())
        {
            return;
        }
        QSaveFile File(Path);
        const QByteArray Contents = Configuration->toPlainText().toUtf8();
        if (!File.open(QIODevice::WriteOnly) || File.write(Contents) != Contents.size() || !File.commit())
        {
            Status->setText(tr("Не удалось сохранить конфигурацию: %1").arg(File.errorString()));
            return;
        }
        if (!McpServer->Start())
        {
            Status->setText(McpServer->Error());
            return;
        }
        UpdateConnection();
        Status->setText(tr("Конфигурация сохранена: %1").arg(Path));
    });
    auto* Tools = new QPlainTextEdit(McpPage);
    Tools->setReadOnly(true);
    for (const QJsonValue& Tool : Toolset->List())
    {
        const QJsonObject Definition = Tool.toObject();
        Tools->appendPlainText(Definition.value("name").toString() + " — " + Definition.value("description").toString());
    }
    Tools->moveCursor(QTextCursor::Start);
    McpLayout->addWidget(Tools);
    Tabs->addTab(McpPage, "MCP Server / Toolset");
    UpdateConnection();
    Tabs->setCurrentIndex(Page);
    auto* Buttons = new QDialogButtonBox(QDialogButtonBox::Close, &Dialog);
    Buttons->button(QDialogButtonBox::Close)->setText(tr("Закрыть"));
    connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    Layout->addWidget(Buttons);
    Dialog.exec();
}
