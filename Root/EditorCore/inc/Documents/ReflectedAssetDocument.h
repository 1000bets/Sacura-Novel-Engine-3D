#pragma once
#include "Assets/AssetRegistry.h"
#include "Reflection/ReflectedValue.h"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

class ReflectedAssetDocument
{
public:
    virtual ~ReflectedAssetDocument() = default;
    virtual bool Open(AssetRegistry& Registry, const AssetRegistryEntry& Entry, std::string& Error) = 0;
    virtual bool Save(std::string& Error) = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    virtual bool IsDirty() const = 0;
    virtual bool IsReadOnly() const = 0;
    virtual TypeId GetRootType() const = 0;
    virtual ReflectionDiagnostic ReadValue(ReflectedValue& Value) const = 0;
    virtual bool ApplyValue(const ReflectedValue& Value, std::string& Error) = 0;
    virtual bool CanUndo() const = 0;
    virtual bool CanRedo() const = 0;
    bool Edit(const nlohmann::json& Change, std::string& Error);
    nlohmann::json GetSchema() const;
    nlohmann::json GetValues() const;
    using Factory = std::function<std::unique_ptr<ReflectedAssetDocument>()>;
    static void Register(const AssetType& Type, Factory Create);
    static std::unique_ptr<ReflectedAssetDocument> Create(const AssetType& Type);
};
