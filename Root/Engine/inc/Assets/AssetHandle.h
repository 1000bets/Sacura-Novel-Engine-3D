#pragma once

#include <memory>

template <typename ResourceType>
class AssetHandle
{
public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<const ResourceType> Resource)
        : Resource(std::move(Resource))
    {
    }

    bool IsValid() const { return Resource != nullptr; }
    const ResourceType* Get() const { return Resource.get(); }
    const ResourceType& operator*() const { return *Resource; }
    const ResourceType* operator->() const { return Resource.get(); }

    void Reset() { Resource.reset(); }

private:
    std::shared_ptr<const ResourceType> Resource;
};
