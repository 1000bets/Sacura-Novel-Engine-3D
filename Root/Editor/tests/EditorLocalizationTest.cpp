#include "Localization/EditorLocalization.h"
#include "UI/Presentation/LocalizedEditorUi.h"
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>

int main(int ArgumentCount, char** Arguments)
{
    QApplication Application(ArgumentCount, Arguments);
    QTemporaryDir Directory(QCoreApplication::applicationDirPath() + "/LocalizationTest-XXXXXX");
    if (!Directory.isValid())
    {
        return 1;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, Directory.path());
    auto& Localization = EditorLocalization::Get();
    Application.installTranslator(&Localization);
    int Failures = 0;
    auto Expect = [&Failures](bool bPassed, const char* Description)
    {
        std::cout << Description << ": " << bPassed << '\n';
        if (!bPassed)
        {
            ++Failures;
        }
    };
    Expect(Localization.GetLanguage() == "en", "English is the default");
    QWidget Window;
    auto* Caption = BindEditorCaption(new QLabel(&Window), "File");
    auto* Action = BindEditorCaption(new QAction(&Window), "Save");
    BindEditorProperty(&Window, "windowTitle", "Editor Settings");
    Expect(Caption->text() == "File", "English caption");
    Expect(Localization.SetLanguage("ru"), "Russian preference accepted");
    Expect(Caption->text() == QString::fromUtf8("Файл"), "Existing label updates");
    Expect(Action->text() == QString::fromUtf8("Сохранить"), "Existing action updates");
    Expect(Window.windowTitle() == QString::fromUtf8("Настройки редактора"), "Existing window title updates");
    Expect(Localization.Text("Editor.Reflection.engine.CameraComponent.field_of_view.DisplayName") == QString::fromUtf8("Угол обзора"), "Stable reflected property key");
    auto* Deleted = BindEditorCaption(new QLabel(), "File");
    delete Deleted;
    Expect(Localization.SetLanguage("en"), "Deleted binding is safe");
    QString Error;
    const QJsonObject Override{{"source", "Editor Settings"}, {"text", QString::fromUtf8("Параметры редактора")}};
    Expect(Localization.ApplyPreferences({{"language", "ru"}, {"overrides", QJsonObject{{"Editor.Settings.Title", Override}}}}, Error), "Translation override accepted");
    Expect(Localization.Text("Editor.Settings.Title") == QString::fromUtf8("Параметры редактора"), "Override resolves by key");
    const QJsonObject Before = Localization.Preferences();
    Expect(!Localization.ApplyPreferences({{"language", "fr"}}, Error), "Unknown language rejected");
    Expect(!Localization.ApplyPreferences({{"language", 1}}, Error), "Wrong language type rejected");
    Expect(!Localization.ApplyPreferences({{"overrides", QJsonObject{{"Editor.Settings.Title", QJsonObject{{"source", "Old source"}, {"text", "Changed"}}}}}}, Error), "Stale source rejected");
    Expect(Localization.Preferences() == Before, "Rejected imports preserve preferences");
    QSettings Saved("SakuraNovel", "SakuraNovel Studio");
    Expect(Saved.value("Editor/Language").toString() == "ru", "Language persists in shared settings");
    Expect(!Saved.value("Editor/TranslationOverrides").toByteArray().isEmpty(), "Overrides persist in shared settings");
    if (Failures > 0)
    {
        return 1;
    }
    return 0;
}
