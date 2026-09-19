#include "ProjectBrowserDialog.h"

#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Project/ProjectGenerator.h"
#include "TemplateCardWidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
QPixmap LoadStudioImage(const QString& FileName)
{
    const QString ResourcePath = QStringLiteral(":/studio/") + FileName;
    QPixmap FromResource(ResourcePath);
    if (!FromResource.isNull())
    {
        return FromResource;
    }

    const QString Candidates[] = {
#if defined(SAKURA_EDITOR_RESOURCES_DIR)
        QString(SAKURA_EDITOR_RESOURCES_DIR) + "/" + FileName,
#endif
        QCoreApplication::applicationDirPath() + "/studio/" + FileName,
        QCoreApplication::applicationDirPath() + "/../Editor/resources/studio/" + FileName,
    };

    for (const QString& Candidate : Candidates)
    {
        QPixmap FromDisk(Candidate);
        if (!FromDisk.isNull())
        {
            return FromDisk;
        }
    }

    return {};
}

QPushButton* MakeNavButton(const QString& Text, QWidget* Parent)
{
    auto* Button = new QPushButton(Text, Parent);
    Button->setCursor(Qt::PointingHandCursor);
    Button->setCheckable(true);
    Button->setFixedHeight(44);
    Button->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding-left: 18px;"
        "  border: none;"
        "  border-radius: 12px;"
        "  color: #C8C8D4;"
        "  background: transparent;"
        "  font-size: 13px;"
        "}"
        "QPushButton:checked {"
        "  color: #FFFFFF;"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #FF7EB6, stop:0.55 #D06BFF, stop:1 #9B5CFF);"
        "}"
        "QPushButton:hover:!checked {"
        "  background: #222230;"
        "}");
    return Button;
}

QLabel* MakeSectionTitle(const QString& Text, QWidget* Parent)
{
    auto* Label = new QLabel(Text, Parent);
    Label->setStyleSheet("color: #B8B8C8; font-size: 12px; font-weight: 600;");
    return Label;
}

QPushButton* MakeQuickStartButton(const QString& Icon, const QString& Title, const QString& Subtitle, QWidget* Parent)
{
    auto* Button = new QPushButton(Parent);
    Button->setCursor(Qt::PointingHandCursor);
    Button->setFixedHeight(62);
    Button->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding: 8px 14px;"
        "  background: #1A1A24;"
        "  border: 1px solid #2A2A38;"
        "  border-radius: 12px;"
        "  color: #F0F0F8;"
        "}"
        "QPushButton:hover {"
        "  border-color: #FF7EB6;"
        "  background: #201824;"
        "}");
    Button->setText(Icon + "  " + Title + "\n      " + Subtitle);
    return Button;
}
}

ProjectBrowserDialog::ProjectBrowserDialog(QWidget* Parent)
    : QDialog(Parent)
{
    setWindowTitle("SakuraNovel Studio");
    setMinimumSize(1280, 760);
    resize(1360, 800);
    setObjectName("StudioLauncher");

    BuildUi();
    ApplyTheme();
    ShowPage(1);
    RefreshRecentProjects();
}

std::filesystem::path ProjectBrowserDialog::SelectedProjectFile() const
{
    return ChosenProjectFile;
}

bool ProjectBrowserDialog::CaptureScreenshot(const std::filesystem::path& OutputFile)
{
    show();
    raise();
    activateWindow();
    QApplication::processEvents();
    const QPixmap Shot = grab();
    return Shot.save(QString::fromStdString(OutputFile.string()), "PNG");
}

void ProjectBrowserDialog::BuildUi()
{
    auto* Root = new QHBoxLayout(this);
    Root->setContentsMargins(0, 0, 0, 0);
    Root->setSpacing(0);

    LeftNav = new QWidget(this);
    LeftNav->setObjectName("LeftNav");
    LeftNav->setFixedWidth(220);
    auto* LeftLayout = new QVBoxLayout(LeftNav);
    LeftLayout->setContentsMargins(18, 22, 18, 18);
    LeftLayout->setSpacing(10);

    auto* BrandRow = new QHBoxLayout();
    auto* Blossom = new QLabel(QString::fromUtf8("\xF0\x9F\x8C\xB8"), LeftNav);
    Blossom->setStyleSheet("font-size: 22px;");
    auto* BrandColumn = new QVBoxLayout();
    BrandColumn->setSpacing(0);
    auto* BrandTitle = new QLabel("SakuraNovel Studio", LeftNav);
    BrandTitle->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: 700;");
    auto* BrandTag = new QLabel("Stories come to life.", LeftNav);
    BrandTag->setStyleSheet("color: #9A9AAB; font-size: 11px;");
    BrandColumn->addWidget(BrandTitle);
    BrandColumn->addWidget(BrandTag);
    BrandRow->addWidget(Blossom);
    BrandRow->addLayout(BrandColumn, 1);
    LeftLayout->addLayout(BrandRow);
    LeftLayout->addSpacing(24);

    HomeButton = MakeNavButton(QString::fromUtf8("  \xF0\x9F\x8F\xA0  Home"), LeftNav);
    NewProjectButton = MakeNavButton(QString::fromUtf8("  \xE2\x9C\xA6  New Project"), LeftNav);
    OpenProjectButton = MakeNavButton(QString::fromUtf8("  \xF0\x9F\x93\x81  Open Project"), LeftNav);
    LeftLayout->addWidget(HomeButton);
    LeftLayout->addWidget(NewProjectButton);
    LeftLayout->addWidget(OpenProjectButton);
    LeftLayout->addStretch(1);

    auto* FooterQuote = new QLabel("Small stories.\nBrighter tomorrows.", LeftNav);
    FooterQuote->setStyleSheet("color: #8A8A9A; font-size: 11px;");
    LeftLayout->addWidget(FooterQuote);
    auto* FooterIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x8C\xB8"), LeftNav);
    FooterIcon->setStyleSheet("font-size: 14px; color: #FF7EB6;");
    LeftLayout->addWidget(FooterIcon);

    CenterPanel = new QWidget(this);
    CenterPanel->setObjectName("CenterPanel");
    auto* CenterLayout = new QVBoxLayout(CenterPanel);
    CenterLayout->setContentsMargins(36, 28, 28, 24);
    CenterLayout->setSpacing(0);

    CenterStack = new QStackedWidget(CenterPanel);

    auto* HomePage = new QWidget(CenterStack);
    auto* HomeLayout = new QVBoxLayout(HomePage);
    auto* HomeTitle = new QLabel("Welcome back", HomePage);
    HomeTitle->setStyleSheet("color: #FFE4F1; font-size: 34px; font-weight: 700;");
    auto* HomeSub = new QLabel("Create a new story or continue where you left off.", HomePage);
    HomeSub->setStyleSheet("color: #9A9AAB; font-size: 14px;");
    HomeLayout->addWidget(HomeTitle);
    HomeLayout->addWidget(HomeSub);
    HomeLayout->addStretch(1);

    auto* CreatePage = new QWidget(CenterStack);
    auto* CreateLayout = new QVBoxLayout(CreatePage);
    CreateLayout->setSpacing(18);

    auto* CreateTitle = new QLabel("Create New Project", CreatePage);
    CreateTitle->setStyleSheet(
        "color: #FF9AC8; font-size: 38px; font-weight: 700;"
        "letter-spacing: 0.2px;");
    auto* CreateSub = new QLabel(QStringLiteral("Start a new story. It's easier than you think."), CreatePage);
    CreateSub->setStyleSheet("color: #9A9AAB; font-size: 14px; margin-bottom: 8px;");
    CreateLayout->addWidget(CreateTitle);
    CreateLayout->addWidget(CreateSub);

    auto* StepOne = new QHBoxLayout();
    auto* StepOneBadge = new QLabel("1", CreatePage);
    StepOneBadge->setFixedSize(22, 22);
    StepOneBadge->setAlignment(Qt::AlignCenter);
    StepOneBadge->setStyleSheet(
        "background: #FF7EB6; color: #1A1A24; border-radius: 11px; font-weight: 700; font-size: 12px;");
    auto* StepOneLabel = new QLabel("Choose Template", CreatePage);
    StepOneLabel->setStyleSheet("color: #E8E8F0; font-size: 14px; font-weight: 600;");
    StepOne->addWidget(StepOneBadge);
    StepOne->addWidget(StepOneLabel);
    StepOne->addStretch(1);
    CreateLayout->addLayout(StepOne);

    auto* CardsRow = new QHBoxLayout();
    CardsRow->setSpacing(14);
    VisualNovelCard = new TemplateCardWidget(
        "VisualNovel",
        "Visual Novel",
        "Story, characters and choices",
        LoadStudioImage("template_visual_novel.png"),
        false,
        CreatePage);
    KineticNovelCard = new TemplateCardWidget(
        "KineticNovel",
        "Kinetic Novel",
        "Story with no choices",
        LoadStudioImage("template_kinetic_novel.png"),
        false,
        CreatePage);
    EmptyProjectCard = new TemplateCardWidget(
        "Empty",
        "Empty Project",
        "A clean starting point",
        QPixmap(),
        true,
        CreatePage);
    CardsRow->addWidget(VisualNovelCard);
    CardsRow->addWidget(KineticNovelCard);
    CardsRow->addWidget(EmptyProjectCard);
    CardsRow->addStretch(1);
    CreateLayout->addLayout(CardsRow);

    auto* StepTwo = new QHBoxLayout();
    auto* StepTwoBadge = new QLabel("2", CreatePage);
    StepTwoBadge->setFixedSize(22, 22);
    StepTwoBadge->setAlignment(Qt::AlignCenter);
    StepTwoBadge->setStyleSheet(
        "background: #FF7EB6; color: #1A1A24; border-radius: 11px; font-weight: 700; font-size: 12px;");
    auto* StepTwoLabel = new QLabel("Project Details", CreatePage);
    StepTwoLabel->setStyleSheet("color: #E8E8F0; font-size: 14px; font-weight: 600;");
    StepTwo->addWidget(StepTwoBadge);
    StepTwo->addWidget(StepTwoLabel);
    StepTwo->addStretch(1);
    CreateLayout->addLayout(StepTwo);

    auto* DetailsFrame = new QFrame(CreatePage);
    DetailsFrame->setObjectName("DetailsFrame");
    DetailsFrame->setStyleSheet(
        "QFrame#DetailsFrame {"
        "  background: #17171F;"
        "  border: 1px solid #2A2A38;"
        "  border-radius: 14px;"
        "}");
    auto* DetailsLayout = new QVBoxLayout(DetailsFrame);
    DetailsLayout->setContentsMargins(18, 16, 18, 16);
    DetailsLayout->setSpacing(12);

    auto* NameRow = new QHBoxLayout();
    auto* NameLabel = new QLabel("Project Name", DetailsFrame);
    NameLabel->setFixedWidth(110);
    NameLabel->setStyleSheet("color: #C8C8D4; font-size: 12px;");
    ProjectNameEdit = new QLineEdit(DetailsFrame);
    ProjectNameEdit->setPlaceholderText("My Novel");
    ProjectNameEdit->setText("My Novel");
    NameRow->addWidget(NameLabel);
    NameRow->addWidget(ProjectNameEdit, 1);

    auto* LocationRow = new QHBoxLayout();
    auto* LocationLabel = new QLabel("Location", DetailsFrame);
    LocationLabel->setFixedWidth(110);
    LocationLabel->setStyleSheet("color: #C8C8D4; font-size: 12px;");
    LocationEdit = new QLineEdit(DetailsFrame);
    LocationEdit->setText(DefaultProjectsDirectory());
    auto* BrowseButton = new QPushButton("Browse", DetailsFrame);
    BrowseButton->setFixedWidth(88);
    BrowseButton->setCursor(Qt::PointingHandCursor);
    BrowseButton->setStyleSheet(
        "QPushButton {"
        "  background: #222230;"
        "  color: #E8E8F0;"
        "  border: 1px solid #3A3A4A;"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "}"
        "QPushButton:hover { border-color: #FF7EB6; }");
    LocationRow->addWidget(LocationLabel);
    LocationRow->addWidget(LocationEdit, 1);
    LocationRow->addWidget(BrowseButton);

    DetailsLayout->addLayout(NameRow);
    DetailsLayout->addLayout(LocationRow);
    CreateLayout->addWidget(DetailsFrame);

    CreateLayout->addStretch(1);

    auto* ActionRow = new QHBoxLayout();
    auto* OpenExistingButton = new QPushButton("  Open Existing", CreatePage);
    OpenExistingButton->setCursor(Qt::PointingHandCursor);
    OpenExistingButton->setFixedHeight(44);
    OpenExistingButton->setStyleSheet(
        "QPushButton {"
        "  background: #1A1A24;"
        "  color: #E8E8F0;"
        "  border: 1px solid #3A3A4A;"
        "  border-radius: 12px;"
        "  padding: 0 18px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { border-color: #FF7EB6; }");

    auto* CreateButton = new QPushButton(QString::fromUtf8("\xF0\x9F\x8C\xB8  Create Project  \xE2\x86\x92"), CreatePage);
    CreateButton->setCursor(Qt::PointingHandCursor);
    CreateButton->setFixedHeight(44);
    CreateButton->setMinimumWidth(180);
    CreateButton->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #FF7EB6, stop:1 #C85CFF);"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 12px;"
        "  padding: 0 22px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #FF93C4, stop:1 #D47AFF);"
        "}");

    ActionRow->addWidget(OpenExistingButton);
    ActionRow->addStretch(1);
    ActionRow->addWidget(CreateButton);
    CreateLayout->addLayout(ActionRow);

    CenterStack->addWidget(HomePage);
    CenterStack->addWidget(CreatePage);
    CenterLayout->addWidget(CenterStack);

    RightPanel = new QWidget(this);
    RightPanel->setObjectName("RightPanel");
    RightPanel->setFixedWidth(300);
    auto* RightLayout = new QVBoxLayout(RightPanel);
    RightLayout->setContentsMargins(16, 28, 20, 20);
    RightLayout->setSpacing(14);

    RightLayout->addWidget(MakeSectionTitle("Quick Start", RightPanel));
    auto* TutorialRow = MakeQuickStartButton(QString::fromUtf8("\xF0\x9F\x8E\x93"), "Open Tutorial", "Learn the basics in 5 minutes", RightPanel);
    auto* DocsRow = MakeQuickStartButton(QString::fromUtf8("\xF0\x9F\x93\x84"), "Read Documentation", "Project setup, scenes, dialogue and scripting", RightPanel);
    auto* SampleRow = MakeQuickStartButton(QString::fromUtf8("\xF0\x9F\x93\x81"), "Open Sample Project", "Explore a ready-made example", RightPanel);
    RightLayout->addWidget(TutorialRow);
    RightLayout->addWidget(DocsRow);
    RightLayout->addWidget(SampleRow);

    RightLayout->addSpacing(8);
    RightLayout->addWidget(MakeSectionTitle(QStringLiteral("What's New"), RightPanel));
    auto* NewsFrame = new QFrame(RightPanel);
    NewsFrame->setStyleSheet(
        "QFrame { background: #17171F; border: 1px solid #2A2A38; border-radius: 12px; }");
    auto* NewsLayout = new QVBoxLayout(NewsFrame);
    NewsLayout->setContentsMargins(12, 10, 12, 10);
    const QStringList NewsItems = {
        "1.1.2: Improved project templates",
        "New: Python scripting support",
        "Fixed: Scene loading stability",
    };
    for (const QString& Item : NewsItems)
    {
        auto* Bullet = new QLabel(QString::fromUtf8("\xE2\x80\xA2  ") + Item, NewsFrame);
        Bullet->setStyleSheet("color: #B0B0BE; font-size: 11px;");
        NewsLayout->addWidget(Bullet);
    }
    RightLayout->addWidget(NewsFrame);

    RightLayout->addSpacing(8);
    RightLayout->addWidget(MakeSectionTitle("Recent Projects", RightPanel));
    RecentScroll = new QScrollArea(RightPanel);
    RecentScroll->setObjectName("RecentScroll");
    RecentScroll->setWidgetResizable(true);
    RecentScroll->setFrameShape(QFrame::NoFrame);
    RecentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    RecentScroll->setStyleSheet(
        "QScrollArea#RecentScroll {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QScrollArea#RecentScroll > QWidget {"
        "  background: transparent;"
        "}"
        "QScrollArea#RecentScroll QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 8px;"
        "  margin: 0;"
        "}"
        "QScrollArea#RecentScroll QScrollBar::handle:vertical {"
        "  background: #3A3A4A;"
        "  border-radius: 4px;"
        "  min-height: 24px;"
        "}"
        "QScrollArea#RecentScroll QScrollBar::add-line:vertical,"
        "QScrollArea#RecentScroll QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}");
    RecentHost = new QWidget();
    RecentHost->setObjectName("RecentHost");
    RecentHost->setAttribute(Qt::WA_StyledBackground, true);
    RecentHost->setStyleSheet("QWidget#RecentHost { background: transparent; }");
    RecentListLayout = new QVBoxLayout(RecentHost);
    RecentListLayout->setContentsMargins(0, 0, 0, 0);
    RecentListLayout->setSpacing(8);
    RecentListLayout->addStretch(1);
    RecentScroll->setWidget(RecentHost);
    if (RecentScroll->viewport() != nullptr)
    {
        RecentScroll->viewport()->setAutoFillBackground(false);
        RecentScroll->viewport()->setStyleSheet("background: transparent;");
    }
    RightLayout->addWidget(RecentScroll, 1);

    Root->addWidget(LeftNav);
    Root->addWidget(CenterPanel, 1);
    Root->addWidget(RightPanel);

    connect(HomeButton, &QPushButton::clicked, this, &ProjectBrowserDialog::OnNavHome);
    connect(NewProjectButton, &QPushButton::clicked, this, &ProjectBrowserDialog::OnNavNewProject);
    connect(OpenProjectButton, &QPushButton::clicked, this, &ProjectBrowserDialog::OnNavOpenProject);
    connect(BrowseButton, &QPushButton::clicked, this, &ProjectBrowserDialog::OnBrowseLocation);
    connect(CreateButton, &QPushButton::clicked, this, &ProjectBrowserDialog::OnCreateProject);
    connect(OpenExistingButton, &QPushButton::clicked, this, &ProjectBrowserDialog::OnOpenExisting);
    connect(VisualNovelCard, &TemplateCardWidget::Clicked, this, &ProjectBrowserDialog::OnTemplateClicked);
    connect(KineticNovelCard, &TemplateCardWidget::Clicked, this, &ProjectBrowserDialog::OnTemplateClicked);
    connect(EmptyProjectCard, &TemplateCardWidget::Clicked, this, &ProjectBrowserDialog::OnTemplateClicked);
    connect(SampleRow, &QPushButton::clicked, this, &ProjectBrowserDialog::OnOpenSampleProject);

    OnTemplateClicked("VisualNovel");
}

void ProjectBrowserDialog::ApplyTheme()
{
    setStyleSheet(
        "QDialog#StudioLauncher {"
        "  background-color: #12121B;"
        "  color: #FFFFFF;"
        "  font-family: \"Segoe UI\";"
        "}"
        "QWidget#LeftNav {"
        "  background-color: rgba(14, 12, 20, 235);"
        "  border-right: 1px solid #2A2434;"
        "}"
        "QWidget#CenterPanel {"
        "  background-color: transparent;"
        "}"
        "QWidget#RightPanel {"
        "  background-color: rgba(12, 10, 18, 230);"
        "  border-left: 1px solid #2A2434;"
        "}"
        "QLineEdit {"
        "  background: #1A1A24;"
        "  color: #F0F0F8;"
        "  border: 1px solid #2F2F40;"
        "  border-radius: 8px;"
        "  padding: 9px 12px;"
        "  selection-background-color: #FF7EB6;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #FF7EB6;"
        "}");
}

void ProjectBrowserDialog::ShowPage(int PageIndex)
{
    CenterStack->setCurrentIndex(PageIndex);
    UpdateNavStyles(PageIndex);
}

void ProjectBrowserDialog::UpdateNavStyles(int ActiveIndex)
{
    HomeButton->setChecked(ActiveIndex == 0);
    NewProjectButton->setChecked(ActiveIndex == 1);
    OpenProjectButton->setChecked(ActiveIndex == 2);
}

void ProjectBrowserDialog::OnNavHome()
{
    ShowPage(0);
}

void ProjectBrowserDialog::OnNavNewProject()
{
    ShowPage(1);
}

void ProjectBrowserDialog::OnNavOpenProject()
{
    OnOpenExisting();
    UpdateNavStyles(2);
}

void ProjectBrowserDialog::OnBrowseLocation()
{
    const QString Selected = QFileDialog::getExistingDirectory(
        this,
        "Choose project location",
        LocationEdit->text());
    if (!Selected.isEmpty())
    {
        LocationEdit->setText(Selected);
    }
}

void ProjectBrowserDialog::OnTemplateClicked(const QString& TemplateId)
{
    SelectedTemplateId = TemplateId;
    VisualNovelCard->SetSelected(TemplateId == "VisualNovel");
    KineticNovelCard->SetSelected(TemplateId == "KineticNovel");
    EmptyProjectCard->SetSelected(TemplateId == "Empty");
}

void ProjectBrowserDialog::OnCreateProject()
{
    const QString ProjectName = ProjectNameEdit->text().trimmed();
    const QString ParentDirectory = LocationEdit->text().trimmed();
    if (ProjectName.isEmpty())
    {
        QMessageBox::warning(this, "Create Project", "Project name is required.");
        return;
    }
    if (ParentDirectory.isEmpty())
    {
        QMessageBox::warning(this, "Create Project", "Location is required.");
        return;
    }

    ProjectGeneratorRequest Request{};
    Request.ParentDirectory = ParentDirectory.toStdString();
    Request.ProjectName = ProjectName.toStdString();
    Request.TemplateId = SelectedTemplateId.toStdString();
    if (EnginePaths::IsInitialized())
    {
        Request.EngineVersion = EnginePaths::EngineVersion();
    }

    ProjectDescriptor Descriptor{};
    std::string Error;
    if (!ProjectGenerator::CreateProject(Request, Descriptor, Error))
    {
        QMessageBox::warning(this, "Create Project Failed", QString::fromStdString(Error));
        return;
    }

    ChosenProjectFile = Descriptor.ProjectFile;
    AddRecentProject(ChosenProjectFile);
    accept();
}

void ProjectBrowserDialog::OnOpenExisting()
{
    const QString Selected = QFileDialog::getOpenFileName(
        this,
        "Open Project",
        LocationEdit != nullptr ? LocationEdit->text() : QString(),
        "Sakura Project (*.project)");
    if (Selected.isEmpty())
    {
        return;
    }

    ChosenProjectFile = Selected.toStdString();
    AddRecentProject(ChosenProjectFile);
    accept();
}

void ProjectBrowserDialog::OnOpenSampleProject()
{
    std::vector<std::filesystem::path> Candidates;
#if defined(SAKURA_SAMPLE_PROJECT_FILE)
    Candidates.emplace_back(SAKURA_SAMPLE_PROJECT_FILE);
#endif
    Candidates.emplace_back(std::filesystem::current_path() / "Samples" / "SampleProject" / "SampleProject.project");
    Candidates.emplace_back(std::filesystem::current_path() / ".." / ".." / ".." / "Samples" / "SampleProject" / "SampleProject.project");
    if (EnginePaths::IsInitialized())
    {
        Candidates.emplace_back(EnginePaths::Root() / ".." / ".." / "Samples" / "SampleProject" / "SampleProject.project");
        Candidates.emplace_back(EnginePaths::Root().parent_path() / "Samples" / "SampleProject" / "SampleProject.project");
    }

    for (const std::filesystem::path& Candidate : Candidates)
    {
        std::error_code Error;
        if (std::filesystem::exists(Candidate, Error))
        {
            ChosenProjectFile = std::filesystem::weakly_canonical(Candidate, Error);
            AddRecentProject(ChosenProjectFile);
            accept();
            return;
        }
    }

    QMessageBox::information(this, "Sample Project", "Sample project was not found.");
}

void ProjectBrowserDialog::OnRecentClicked()
{
    auto* Button = qobject_cast<QPushButton*>(sender());
    if (Button == nullptr)
    {
        return;
    }

    ChosenProjectFile = Button->property("ProjectFile").toString().toStdString();
    if (!std::filesystem::exists(ChosenProjectFile))
    {
        QMessageBox::warning(this, "Missing Project", "The selected recent project no longer exists.");
        RefreshRecentProjects();
        return;
    }

    AddRecentProject(ChosenProjectFile);
    accept();
}

void ProjectBrowserDialog::RefreshRecentProjects()
{
    while (QLayoutItem* Item = RecentListLayout->takeAt(0))
    {
        if (QWidget* Widget = Item->widget())
        {
            Widget->deleteLater();
        }
        delete Item;
    }

    std::vector<RecentProjectEntry> Recent = LoadRecentProjects();
    const std::size_t CountBeforePrune = Recent.size();
    PruneMissingRecentProjects(Recent);
    if (Recent.size() != CountBeforePrune)
    {
        SaveRecentProjects(Recent);
    }

    if (Recent.empty())
    {
        auto* EmptyLabel = new QLabel("No recent projects yet.\nCreate or open a project to see it here.", RecentHost);
        EmptyLabel->setAlignment(Qt::AlignCenter);
        EmptyLabel->setWordWrap(true);
        EmptyLabel->setStyleSheet(
            "color: #7A7A8A; font-size: 11px; padding: 18px;"
            "background: #17171F; border: 1px dashed #2A2A38; border-radius: 10px;");
        RecentListLayout->addWidget(EmptyLabel);
        RecentListLayout->addStretch(1);
        return;
    }

    for (const RecentProjectEntry& Entry : Recent)
    {
        auto* Row = new QPushButton(RecentHost);
        Row->setCursor(Qt::PointingHandCursor);
        Row->setFixedHeight(56);
        Row->setProperty("ProjectFile", QString::fromStdString(Entry.ProjectFile.generic_string()));

        const QString Name = QString::fromStdString(Entry.ProjectFile.stem().string());
        const QString PathText = QString::fromStdString(Entry.ProjectFile.parent_path().generic_string());
        const QString DateText = FormatRecentDate(Entry.LastOpenedIso, Entry.ProjectFile);
        Row->setText(
            QString::fromUtf8("\xF0\x9F\x8C\xB8  ") + Name + "\n     " + PathText + "          " + DateText);
        Row->setStyleSheet(
            "QPushButton {"
            "  text-align: left;"
            "  padding: 8px 10px;"
            "  background: #1A1A24;"
            "  border: 1px solid #2A2A38;"
            "  border-radius: 10px;"
            "  color: #E8E8F0;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover { border-color: #FF7EB6; background: #201824; }");
        connect(Row, &QPushButton::clicked, this, &ProjectBrowserDialog::OnRecentClicked);
        RecentListLayout->addWidget(Row);
    }

    RecentListLayout->addStretch(1);
}

void ProjectBrowserDialog::AddRecentProject(const std::filesystem::path& ProjectFile)
{
    std::error_code Error;
    const std::filesystem::path Canonical = std::filesystem::weakly_canonical(ProjectFile, Error);
    const std::filesystem::path Normalized = Error ? std::filesystem::absolute(ProjectFile) : Canonical;

    std::vector<RecentProjectEntry> Recent = LoadRecentProjects();
    Recent.erase(
        std::remove_if(
            Recent.begin(),
            Recent.end(),
            [&](const RecentProjectEntry& Entry)
            {
                return Entry.ProjectFile == Normalized;
            }),
        Recent.end());

    RecentProjectEntry NewEntry{};
    NewEntry.ProjectFile = Normalized;
    NewEntry.LastOpenedIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    Recent.insert(Recent.begin(), NewEntry);
    if (Recent.size() > 20)
    {
        Recent.resize(20);
    }

    SaveRecentProjects(Recent);
    RefreshRecentProjects();
}

void ProjectBrowserDialog::PruneMissingRecentProjects(std::vector<RecentProjectEntry>& Entries) const
{
    Entries.erase(
        std::remove_if(
            Entries.begin(),
            Entries.end(),
            [](const RecentProjectEntry& Entry)
            {
                return !std::filesystem::exists(Entry.ProjectFile);
            }),
        Entries.end());
}

std::filesystem::path ProjectBrowserDialog::RecentProjectsFile() const
{
    const QString AppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!AppData.isEmpty())
    {
        return std::filesystem::path(AppData.toStdString()) / "RecentProjects.json";
    }

    if (EnginePaths::IsInitialized())
    {
        return EnginePaths::Config() / "RecentProjects.json";
    }

    return std::filesystem::current_path() / "RecentProjects.json";
}

std::vector<RecentProjectEntry> ProjectBrowserDialog::LoadRecentProjects() const
{
    std::vector<RecentProjectEntry> Result;
    const std::filesystem::path FilePath = RecentProjectsFile();
    std::ifstream Input(FilePath);
    if (!Input)
    {
        return Result;
    }

    try
    {
        nlohmann::json Document;
        Input >> Document;
        if (!Document.is_array())
        {
            return Result;
        }

        for (const nlohmann::json& Entry : Document)
        {
            RecentProjectEntry Parsed{};
            if (Entry.is_string())
            {
                Parsed.ProjectFile = Entry.get<std::string>();
            }
            else if (Entry.is_object() && Entry.contains("path") && Entry["path"].is_string())
            {
                Parsed.ProjectFile = Entry["path"].get<std::string>();
                if (Entry.contains("lastOpened") && Entry["lastOpened"].is_string())
                {
                    Parsed.LastOpenedIso = Entry["lastOpened"].get<std::string>();
                }
            }
            else
            {
                continue;
            }

            if (!Parsed.ProjectFile.empty())
            {
                Result.push_back(Parsed);
            }
        }
    }
    catch (...)
    {
        return {};
    }

    return Result;
}

void ProjectBrowserDialog::SaveRecentProjects(const std::vector<RecentProjectEntry>& Projects) const
{
    const std::filesystem::path FilePath = RecentProjectsFile();
    std::error_code Error;
    std::filesystem::create_directories(FilePath.parent_path(), Error);

    nlohmann::json Document = nlohmann::json::array();
    for (const RecentProjectEntry& Entry : Projects)
    {
        nlohmann::json Item;
        Item["path"] = Entry.ProjectFile.generic_string();
        Item["lastOpened"] = Entry.LastOpenedIso.empty()
            ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()
            : Entry.LastOpenedIso;
        Document.push_back(Item);
    }

    std::ofstream Output(FilePath);
    if (Output)
    {
        Output << Document.dump(2);
    }
}

QString ProjectBrowserDialog::FormatRecentDate(const std::string& IsoTimestamp, const std::filesystem::path& ProjectFile)
{
    if (!IsoTimestamp.empty())
    {
        const QDateTime Parsed = QDateTime::fromString(QString::fromStdString(IsoTimestamp), Qt::ISODate);
        if (Parsed.isValid())
        {
            return Parsed.toLocalTime().toString("MMM d");
        }
    }

    const QFileInfo Info(QString::fromStdString(ProjectFile.string()));
    if (Info.exists())
    {
        return Info.lastModified().toString("MMM d");
    }

    return {};
}

QString ProjectBrowserDialog::DefaultProjectsDirectory() const
{
    const QString Documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (Documents.isEmpty())
    {
        return QString::fromStdString(std::filesystem::current_path().string());
    }
    return Documents + "/SakuraNovel Projects";
}
