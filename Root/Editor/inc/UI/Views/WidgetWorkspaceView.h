#pragma once
#include "Documents/WidgetDocument.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QScrollArea;
class QListWidget;
class QLabel;
class QPushButton;
class QVBoxLayout;
namespace EditorUi
{
class WidgetCanvasView;
class WidgetWorkspaceView : public QWidget
{
  public:
    explicit WidgetWorkspaceView(QWidget *Parent = nullptr);
    bool Open(const QString &ContentRoot);
    bool Save();
    bool PromptSave();
    void Undo();
    void Redo();
    void DeleteSelection();
    void FlushEdits();
    void SetLocked(bool Value);
    void SelectWidget(const QString &Id);
    void Refresh();
    WidgetDocument &GetDocument()
    {
        return Document;
    }
    QJsonObject CurrentWidget() const;
    std::function<void()> Changed, Saved;
    std::function<void(QJsonObject)> PreviewRequested;

  private:
    void QueueRefresh();
    void Inspector();
    void Mutate(const std::function<void(QJsonObject &)> &Fn);
    void EditWidget(const std::function<void(QJsonObject &)> &Fn);
    void EditElement(const std::function<void(QJsonObject &)> &Fn);
    void AddElement(const QString &Type);
    void DeleteElement();
    void Create(bool Copy);
    void DeleteWidget();
    void Reanchor(QPointF Min, QPointF Max, QPointF Pivot);
    void Import();
    void Export();
    void Error(const QString &Message);
    WidgetDocument Document;
    QString WidgetId, ElementId;
    bool Locked = false, Pending = false, Refreshing = false;
    QComboBox *Picker = nullptr;
    QComboBox *Assignments[4]{};
    QListWidget *Elements = nullptr;
    QScrollArea *Properties = nullptr;
    WidgetCanvasView *Canvas = nullptr;
    QLabel *Note = nullptr;
    QWidget *EditTools = nullptr;
    QComboBox *Add = nullptr;
    QPushButton *Preview = nullptr;
};
} // namespace EditorUi
