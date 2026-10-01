#pragma once

#include <QDialog>
#include <filesystem>
#include <string>
#include <vector>

class QLineEdit;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QScrollArea;
class QWidget;
class TemplateCardWidget;

struct RecentProjectEntry
{
    std::filesystem::path ProjectFile;
    std::string LastOpenedIso;
};

class ProjectBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectBrowserDialog(QWidget* Parent = nullptr);

    std::filesystem::path SelectedProjectFile() const;
    bool CaptureScreenshot(const std::filesystem::path& OutputFile);

private slots:
    void OnNavHome();
    void OnNavNewProject();
    void OnNavOpenProject();
    void OnBrowseLocation();
    void OnCreateProject();
    void OnOpenExisting();
    void OnOpenSampleProject();
    void OnTemplateClicked(const QString& TemplateId);
    void OnRecentClicked();

private:
    void BuildUi();
    void ApplyTheme();
    void ShowPage(int PageIndex);
    void RefreshRecentProjects();
    void AddRecentProject(const std::filesystem::path& ProjectFile);
    void PruneMissingRecentProjects(std::vector<RecentProjectEntry>& Entries) const;
    std::filesystem::path RecentProjectsFile() const;
    std::vector<RecentProjectEntry> LoadRecentProjects() const;
    void SaveRecentProjects(const std::vector<RecentProjectEntry>& Projects) const;
    QString DefaultProjectsDirectory() const;
    void UpdateNavStyles(int ActiveIndex);
    static QString FormatRecentDate(const std::string& IsoTimestamp, const std::filesystem::path& ProjectFile);

    QWidget* LeftNav = nullptr;
    QWidget* CenterPanel = nullptr;
    QWidget* RightPanel = nullptr;
    QStackedWidget* CenterStack = nullptr;

    QPushButton* HomeButton = nullptr;
    QPushButton* NewProjectButton = nullptr;
    QPushButton* OpenProjectButton = nullptr;

    TemplateCardWidget* VisualNovelCard = nullptr;
    TemplateCardWidget* KineticNovelCard = nullptr;
    TemplateCardWidget* EmptyProjectCard = nullptr;

    QLineEdit* ProjectNameEdit = nullptr;
    QLineEdit* LocationEdit = nullptr;
    QScrollArea* RecentScroll = nullptr;
    QWidget* RecentHost = nullptr;
    QVBoxLayout* RecentListLayout = nullptr;

    QString SelectedTemplateId = "VisualNovel";
    std::filesystem::path ChosenProjectFile;
};
