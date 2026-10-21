#pragma once

#include "Authoring/OperationTask.h"

#include <functional>
#include <memory>

namespace Authoring
{
using TaskFactory = std::function<std::unique_ptr<OperationTask>(const Arguments&, OperationContext&)>;

struct RegisteredOperation
{
    OperationDefinition Definition;
    TaskFactory CreateTask;
};

class OperationRegistry
{
public:
    Diagnostic Register(OperationDefinition Definition, TaskFactory Factory);
    std::shared_ptr<const RegisteredOperation> Find(const std::string& Id, uint32_t Version) const;
    std::vector<OperationDefinition> GetDefinitions() const;
    Diagnostic Prepare(const OperationCall& Call, Arguments& OutParameters) const;

private:
    using Key = std::pair<std::string, uint32_t>;
    std::map<Key, std::shared_ptr<const RegisteredOperation>> Entries;
};
}
