#pragma once

#include "Reflection/PropertyDescriptor.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"
#include "Assets/AssetTypes.h"

#include <QWidget>
#include <functional>
#include <vector>
#include <unordered_map>

class Object;
class AssetRegistry;
class QFormLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

class ReflectionInspector : public QWidget
{
    Q_OBJECT

public:
    using PropertyCommitCallback = std::function<bool(Object* Instance, const PropertyId& Property, const ReflectedValue& NewValue)>;
    using PropertyResetCallback = std::function<bool(Object* Instance, const PropertyId& Property)>;
    using AssetCommitCallback = std::function<bool(
        Object* Instance,
        const PropertyId& Property,
        const PropertyId& CompanionProperty,
        const AssetKey& Key)>;

    explicit ReflectionInspector(QWidget* Parent = nullptr);

    void SetInspectedObject(Object* Instance);
    Object* GetInspectedObject() const;

    void SetPropertyCommitCallback(PropertyCommitCallback Callback);
    void SetPropertyResetCallback(PropertyResetCallback Callback);
    void SetAssetRegistry(AssetRegistry* Registry);
    void SetAssetCommitCallback(AssetCommitCallback Callback);
    void SetTypeLabelVisible(bool bVisible);
    void SetMeshPresentation(bool Enabled) { if (bMeshPresentation != Enabled) { bMeshPresentation = Enabled; Rebuild(); } }

    void Rebuild();
    void RefreshValues();

    static ReflectionDiagnostic ResetPropertyToDefault(Object* Instance, const PropertyId& Property);
    static ReflectionDiagnostic ResetAllToDefault(Object* Instance);
    static std::vector<const PropertyDescriptor*> ListEditableProperties(Object* Instance);

signals:
    void PropertyChanged();

private:
    void ClearPropertyEditors();
    void AddPropertyEditor(const PropertyDescriptor& Descriptor);
    bool BuildMeshPresentation();
    void ApplyBoolValue(const PropertyId& Property, bool Value);
    void ApplyFloatValue(const PropertyId& Property, double Value);
    void ApplyStringValue(const PropertyId& Property, const QString& Value);
    void ApplyVector3Value(const PropertyId& Property, float X, float Y, float Z);
    void ApplyAssetValue(const PropertyDescriptor& Descriptor, const AssetKey& Key);
    void OnResetPropertyClicked(const PropertyId Property);
    void OnResetAllClicked();
    bool CommitProperty(const PropertyId& Property, const ReflectedValue& NewValue);
    bool CommitReset(const PropertyId& Property);

    ObjectHandle InspectedIdentity;
    std::vector<std::pair<PropertyId, ReflectedValue>> DisplayedValues;
    std::unordered_map<std::string, std::function<bool(const ReflectedValue&)>> PropertyRefreshers;
    bool bValuesDirty = false;
    bool bRefreshQueued = false;
    PropertyCommitCallback CommitCallback;
    PropertyResetCallback ResetCallback;
    AssetCommitCallback AssetCallback;
    AssetRegistry* Registry = nullptr;
    QLabel* TypeLabel = nullptr;
    QPushButton* ResetAllButton = nullptr;
    QFormLayout* PropertiesLayout = nullptr;
    QVBoxLayout* RootLayout = nullptr;
    bool bMeshPresentation = false;
    bool bMeshDetailsExpanded = false;
    int SelectedMaterialSlot = -1;
};
