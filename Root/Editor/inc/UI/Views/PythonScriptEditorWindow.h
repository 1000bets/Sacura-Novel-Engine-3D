#pragma once
#include "Documents/PythonScriptDocument.h"
#include <QMainWindow>
#include <memory>

class Engine;
class QPlainTextEdit;
class PythonScriptEditorWindow : public QMainWindow
{
public:
    PythonScriptEditorWindow(Engine& InRuntime, std::shared_ptr<PythonScriptDocument> InDocument, QWidget* Parent);
    PythonScriptDocument& GetDocument() { return *Document; }
    uint64_t GetRevision() const { return Revision; }
    bool Execute(const QString& Operation, const std::string& Source, std::string& Error);
protected:
    void closeEvent(QCloseEvent* Event) override;
private:
    void Refresh();
    Engine& Runtime;
    std::shared_ptr<PythonScriptDocument> Document;
    QPlainTextEdit* SourceEditor = nullptr;
    uint64_t Revision = 1;
};
