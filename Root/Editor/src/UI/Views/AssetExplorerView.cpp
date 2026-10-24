#include "UI/Presentation/LocalizedEditorUi.h"
#include "UI/Views/AssetExplorerView.h"
#include "UI/Styles/StudioIcons.h"

#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>

namespace EditorUi
{
AssetExplorerView::AssetExplorerView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("AssetExplorer");
    // The horizontal toolbar must not prevent the resize that switches it to
    // two rows (or icon-only actions) in a narrow dock.
    setMinimumWidth(300);
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* Toolbar = new QWidget(this);
    Toolbar->setObjectName("ContentBrowserToolbar");
    ToolbarLayout = new QBoxLayout(QBoxLayout::LeftToRight, Toolbar);
    ToolbarLayout->setContentsMargins(12, 8, 12, 8);
    ToolbarLayout->setSpacing(8);

    auto* Scroll = new QScrollArea(Toolbar);
    Scroll->setObjectName("ExplorerBreadcrumbScroll");
    Scroll->setWidgetResizable(true);
    Scroll->setFrameShape(QFrame::NoFrame);
    Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Scroll->setMinimumWidth(90);
    Scroll->setFixedHeight(32);
    Breadcrumbs = new QWidget(Scroll);
    Breadcrumbs->setObjectName("ExplorerBreadcrumbs");
    BreadcrumbLayout = new QHBoxLayout(Breadcrumbs);
    BreadcrumbLayout->setContentsMargins(0, 0, 0, 0);
    BreadcrumbLayout->setSpacing(3);
    Scroll->setWidget(Breadcrumbs);
    ToolbarLayout->addWidget(Scroll, 1);

    auto* Actions = new QWidget(Toolbar);
    auto* ActionLayout = new QHBoxLayout(Actions);
    ActionLayout->setContentsMargins(0, 0, 0, 0);
    ActionLayout->setSpacing(8);
    Search = new QLineEdit(Actions);
    Search->setObjectName("ContentBrowserSearch");
    BindEditorProperty(Search, "placeholderText", QString::fromUtf8("Поиск файлов…"));
    Search->setAccessibleName(tr("Поиск файлов в папке"));
    Search->setMinimumWidth(100);
    Search->setMaximumWidth(240);
    Search->addAction(MakeFrontendIcon("Search", "--text-secondary"), QLineEdit::LeadingPosition);
    ActionLayout->addWidget(Search, 1);
    auto MakeButton = [&](const char* Name, const QString& Text, const char* Icon)
    {
        auto* Button = new QToolButton(Actions);
        Button->setObjectName(Name);
        Button->setText(Text);
        Button->setToolTip(Text);
        Button->setAccessibleName(Text);
        Button->setIcon(MakeFrontendIcon(QString::fromLatin1(Icon), "--text-secondary"));
        Button->setIconSize({16, 16});
        Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        ActionLayout->addWidget(Button);
        return Button;
    };
    NewFolder = MakeButton("ContentBrowserNewFolder", tr("Папка"), "FolderPlus");
    ImportFiles = MakeButton("ContentBrowserImport", tr("Импорт файлов"), "Upload");
    ImportFolder = MakeButton("ContentBrowserImportFolder", tr("Импорт папки"), "FolderInput");
    ToolbarLayout->addWidget(Actions);
    Layout->addWidget(Toolbar);
    Body = new QSplitter(Qt::Horizontal, this);
    Body->setObjectName("ExplorerBody");
    Body->setHandleWidth(1);
    Body->setChildrenCollapsible(false);
    Layout->addWidget(Body, 1);
    EmptyState = new QLabel(this);
    EmptyState->setObjectName("ExplorerEmptyState");
    EmptyState->setAlignment(Qt::AlignCenter);
    EmptyState->setWordWrap(true);
    EmptyState->setAttribute(Qt::WA_TransparentForMouseEvents);
    EmptyState->hide();
    SetFolder("/Game", false);
}

void AssetExplorerView::SetFolder(const QString& VirtualFolder, bool Writable)
{
    while (auto* Item = BreadcrumbLayout->takeAt(0))
    {
        if (Item->widget())
        {
            Item->widget()->hide();
            Item->widget()->deleteLater();
        }
        delete Item;
    }
    const QStringList Parts = VirtualFolder.split('/', Qt::SkipEmptyParts);
    QString Path;
    for (int Index = 0; Index < Parts.size(); ++Index)
    {
        Path += "/" + Parts[Index];
        if (Index > 0)
        {
            auto* Separator = new QLabel(Breadcrumbs);
            Separator->setPixmap(MakeFrontendIcon("ChevronRight").pixmap(12, 12));
            BreadcrumbLayout->addWidget(Separator);
        }
        auto* Button = new QToolButton(Breadcrumbs);
        Button->setText(Index == 0 ? (Parts[Index] == "Game" ? "Assets" : tr("Ресурсы движка")) : Parts[Index]);
        Button->setToolTip(Path);
        Button->setProperty("folderPath", Path);
        if (Index == 0)
        {
            Button->setIcon(MakeFrontendIcon("FolderOpen", "--text-secondary"));
            Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        }
        BreadcrumbLayout->addWidget(Button);
        connect(Button, &QToolButton::clicked, this, [this, Path]() { emit FolderRequested(Path); });
    }
    BreadcrumbLayout->addStretch(1);
    for (auto* Button : {NewFolder, ImportFiles, ImportFolder})
    {
        Button->setEnabled(Writable);
        Button->setToolTip(Writable ? Button->text() : tr("Ресурсы движка доступны только для чтения"));
    }

}

void AssetExplorerView::SetEmptyState(bool Empty, bool Searching)
{
    EmptyState->setText(Searching ? tr("Файлы не найдены\nПопробуйте другое имя или откройте Assets.")
                                : tr("Папка пуста\nПеретащите файлы сюда или используйте импорт."));
    EmptyState->setVisible(Empty);
    EmptyState->raise();
}

void AssetExplorerView::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    ToolbarLayout->setDirection(width() < 820 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    for (auto* Button : {NewFolder, ImportFiles, ImportFolder})
    {
        Button->setToolButtonStyle(width() < 560 ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    }
}
}
