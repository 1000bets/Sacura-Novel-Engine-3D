#pragma once
#include <QWidget>
#include <array>
#include <functional>

class QComboBox;
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
    QLineEdit* Name = nullptr;
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
};
}
