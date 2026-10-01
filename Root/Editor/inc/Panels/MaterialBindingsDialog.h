#pragma once
#include "Assets/AssetRegistry.h"
#include <QDialog>
#include <nlohmann/json.hpp>

class QTableWidget;
class MaterialBindingsDialog : public QDialog
{
public:
    MaterialBindingsDialog(AssetRegistry& Registry, nlohmann::json Entries, bool bPostProcess, QWidget* Parent);
    nlohmann::json GetEntries() const;
private:
    void AppendRow(const nlohmann::json& Entry);
    void MoveRow(int Direction);
    AssetRegistry& BoundRegistry;
    QTableWidget* Table = nullptr;
    bool bPostProcess = false;
};
