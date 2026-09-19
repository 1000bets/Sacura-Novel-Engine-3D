#pragma once

#include "Reflection/PropertyDescriptor.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <QWidget>
#include <vector>

class Object;
class QFormLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

class ReflectionInspector : public QWidget
{
    Q_OBJECT

public:
    explicit ReflectionInspector(QWidget* Parent = nullptr);

    void SetInspectedObject(Object* Instance);
    Object* GetInspectedObject() const;

    void Rebuild();

    static ReflectionDiagnostic ResetPropertyToDefault(Object* Instance, const PropertyId& Property);
    static ReflectionDiagnostic ResetAllToDefault(Object* Instance);
    static std::vector<const PropertyDescriptor*> ListEditableProperties(Object* Instance);

signals:
    void PropertyChanged();

private:
    void ClearPropertyEditors();
    void AddPropertyEditor(const PropertyDescriptor& Descriptor);
    void ApplyBoolValue(const PropertyId& Property, bool Value);
    void ApplyFloatValue(const PropertyId& Property, double Value);
    void ApplyStringValue(const PropertyId& Property, const QString& Value);
    void ApplyVector3Value(const PropertyId& Property, float X, float Y, float Z);
    void ApplyColorValue(const PropertyId& Property, float R, float G, float B, float A);
    void OnResetPropertyClicked(const PropertyId Property);
    void OnResetAllClicked();

    Object* InspectedObject = nullptr;
    QLabel* TypeLabel = nullptr;
    QPushButton* ResetAllButton = nullptr;
    QFormLayout* PropertiesLayout = nullptr;
    QVBoxLayout* RootLayout = nullptr;
};
