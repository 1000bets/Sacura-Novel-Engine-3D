#pragma once

#include "Assets/AssetTypes.h"
#include <QWidget>
#include <functional>

class AssetRegistry;
class QComboBox;
class QPushButton;

class AssetReferenceField : public QWidget
{
public:
    AssetReferenceField(QComboBox& Picker, const AssetRegistry* Registry);
    AssetReferenceField(QComboBox& Picker, const AssetRegistry& Registry) : AssetReferenceField(Picker, &Registry) {}
    void SetRegistry(const AssetRegistry* Value);
    static void SetNavigation(QObject& Owner, std::function<void(const AssetKey&, bool)> Navigate);

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    AssetKey CurrentAsset() const;
    void RefreshActions();
    void Navigate(bool bOpen);
    void ClearAsset();
    QComboBox* Picker = nullptr;
    const AssetRegistry* Registry = nullptr;
    QPushButton* Open = nullptr;
    QPushButton* Browse = nullptr;
    QPushButton* Clear = nullptr;
};
