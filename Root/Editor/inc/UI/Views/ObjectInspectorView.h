#pragma once
#include <QWidget>
#include <array>
#include <functional>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QDoubleSpinBox;
class QLineEdit;
class QVBoxLayout;
namespace EditorUi
{
class ObjectInspectorView final : public QWidget
{
public:
    explicit ObjectInspectorView(QWidget* Parent = nullptr);
    static QString ComponentCaption(const QString& Type);
    void AddComponentSection(const QString& Identity, const QString& Type, QWidget& Fields, bool Expanded);
    void ClearComponentOverview();
    void RemoveComponentSection(const QString& Identity);
    void MoveComponentSection(const QString& Identity, int Index);
    void SetIdentity(const QString& Caption, const QString& Kind, bool Active);
    QLineEdit* Name = nullptr;
    QCheckBox* Active = nullptr;
    QComboBox* AddComponentType = nullptr;
    QComboBox* ParentObject = nullptr;
    QComboBox* AnchorObject = nullptr;
    QPushButton* AddComponent = nullptr;
    std::function<void(int)> AddComponentRequested;
    std::array<QDoubleSpinBox*, 9> TransformFields{};
    QComboBox* ComponentFilter = nullptr;
    QVBoxLayout* Components = nullptr;
    std::function<void()> FrameRequested;
    std::function<void()> ResetRequested;
    std::function<void()> DuplicateRequested;
    std::function<void()> DeleteRequested;
    void SetEditable(bool Editable);
private:
    QWidget* Actions = nullptr;
    QWidget* ComponentTools = nullptr;
    QVBoxLayout* ComponentRows = nullptr;
    QLabel* ComponentCount = nullptr;
    QLabel* EmptyComponents = nullptr;
    QLabel* ObjectKind = nullptr;
    QLabel* Selection = nullptr;
    QLineEdit* ComponentSearch = nullptr;
    QScrollArea* Scroll = nullptr;
};
}
