#include "UI/Views/PythonScriptEditorWindow.h"
#include "UI/Presentation/LocalizedEditorUi.h"
#include "UI/Styles/EditorStyle.h"
#include "Engine.h"
#include <QAction>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class PythonSourceHighlighter : public QSyntaxHighlighter
{
public:
    explicit PythonSourceHighlighter(QTextDocument* Document) : QSyntaxHighlighter(Document) {}
protected:
    void highlightBlock(const QString& Text) override
    {
        const QRegularExpression Tokens(R"Python((#[^\n]*)|("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')|\b(False|None|True|and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield)\b)Python");
        auto Matches = Tokens.globalMatch(Text);
        while (Matches.hasNext())
        {
            const auto Match = Matches.next();
            QTextCharFormat Format;
            Format.setForeground(EditorUi::EditorStyle::Color("--accent"));
            if (!Match.captured(1).isEmpty())
            {
                Format.setForeground(EditorUi::EditorStyle::Color("--success"));
            }
            else if (!Match.captured(2).isEmpty())
            {
                Format.setForeground(EditorUi::EditorStyle::Color("--warning"));
            }
            setFormat(Match.capturedStart(), Match.capturedLength(), Format);
        }
    }
};

PythonScriptEditorWindow::PythonScriptEditorWindow(Engine& InRuntime,
    std::shared_ptr<PythonScriptDocument> InDocument, QWidget* Parent)
    : QMainWindow(Parent, Qt::Window), Runtime(InRuntime), Document(std::move(InDocument))
{
    setObjectName("PythonScriptEditorWindow");
    EditorUi::EditorStyle::Apply(*this);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 650);
    SourceEditor = new QPlainTextEdit(this);
    SourceEditor->setObjectName("PythonSourceEditor");
    SourceEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    SourceEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    SourceEditor->setTabStopDistance(SourceEditor->fontMetrics().horizontalAdvance(' ') * 4);
    new PythonSourceHighlighter(SourceEditor->document());
    SourceEditor->setReadOnly(Document->GetEntry().Mount != AssetMount::Game);
    setCentralWidget(SourceEditor);
    auto* FileMenu = BindEditorCaption(menuBar()->addMenu("File"), "File");
    auto* EditMenu = BindEditorCaption(menuBar()->addMenu("Edit"), "Edit");
    auto* ViewMenu = BindEditorCaption(menuBar()->addMenu("View"), "View");
    auto* Toolbar = addToolBar("Document");
    auto AddAction = [&](QMenu* Menu, const QString& Caption, const QKeySequence& Shortcut, const QString& Operation)
    {
        auto* Action = BindEditorCaption(Menu->addAction(Caption), Caption);
        Action->setShortcut(Shortcut);
        Toolbar->addAction(Action);
        connect(Action, &QAction::triggered, this, [this, Operation]()
        {
            std::string Error;
            if (!Execute(Operation, {}, Error))
            {
                QMessageBox::warning(this, "Python", QString::fromStdString(Error));
            }
        });
    };
    AddAction(FileMenu, "Save", QKeySequence::Save, "save");
    AddAction(FileMenu, "Reload from Disk", {}, "reload");
    AddAction(EditMenu, "Undo", QKeySequence::Undo, "undo");
    AddAction(EditMenu, "Redo", QKeySequence::Redo, "redo");
    ViewMenu->addAction(Toolbar->toggleViewAction());
    connect(SourceEditor, &QPlainTextEdit::textChanged, this, [this]()
    {
        std::string Error;
        Execute("source", SourceEditor->toPlainText().toStdString(), Error);
        if (!Error.empty())
        {
            statusBar()->showMessage(QString::fromStdString(Error));
            Refresh();
        }
    });
    statusBar()->showMessage(EditorLocalization::Get().Caption("Reopen the project to apply Python changes."));
    Refresh();
}

bool PythonScriptEditorWindow::Execute(const QString& Operation, const std::string& Source, std::string& Error)
{
    if (Runtime.GetPlaySession().IsSimulating() || Runtime.IsGameRunning())
    {
        Error = "Stop Play before editing scripts";
        return false;
    }
    if (Document->GetEntry().Mount != AssetMount::Game)
    {
        Error = "Engine scripts are read-only";
        return false;
    }
    if (Operation == "source")
    {
        if (!Document->Edit(Source, Error))
        {
            return false;
        }
    }
    else if (Operation == "save")
    {
        if (!Document->Save(Error))
        {
            return false;
        }
    }
    else if (Operation == "undo")
    {
        Document->Undo();
    }
    else if (Operation == "redo")
    {
        Document->Redo();
    }
    else if (Operation == "reload")
    {
        if (Document->IsDirty())
        {
            Error = "Save or undo script changes before reloading";
            return false;
        }
        if (!Document->Open(AssetKey{Document->GetEntry().Metadata.Guid, {}}, Error))
        {
            return false;
        }
    }
    else
    {
        Error = "Unknown Python document operation";
        return false;
    }
    ++Revision;
    Refresh();
    return true;
}

void PythonScriptEditorWindow::Refresh()
{
    const auto Source = QString::fromStdString(Document->GetSource());
    if (SourceEditor->toPlainText() != Source)
    {
        QSignalBlocker Block(SourceEditor);
        SourceEditor->setPlainText(Source);
    }
    QString Title = QString::fromStdString(Document->GetEntry().VirtualPath);
    if (Document->IsDirty())
    {
        Title += " *";
    }
    setWindowTitle(Title + " — Python");
}

void PythonScriptEditorWindow::closeEvent(QCloseEvent* Event)
{
    if (Document->IsDirty())
    {
        const auto Choice = QMessageBox::question(this, "Python", EditorLocalization::Get().Caption("Save changes?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (Choice == QMessageBox::Cancel)
        {
            Event->ignore();
            return;
        }
        if (Choice == QMessageBox::Save)
        {
            std::string Error;
            if (!Execute("save", {}, Error))
            {
                QMessageBox::warning(this, "Python", QString::fromStdString(Error));
                Event->ignore();
                return;
            }
        }
    }
    QMainWindow::closeEvent(Event);
}
