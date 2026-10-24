#pragma once

#include "Reflection/PropertyDescriptor.h"
#include "Core/Object/ObjectHandle.h"
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

struct StoryDocument;
struct StoryNode;

struct NarrativePin
{
    PropertyDescriptor Property;
    nlohmann::json Default;
    bool bHasDefault = false;
};

struct NarrativeNodeDefinition
{
    std::string Type;
    std::string Label;
    bool bPure = false;
    std::vector<NarrativePin> Inputs;
    std::vector<NarrativePin> Outputs;
    std::vector<std::string> Exits;
    std::string ComponentType;
    std::string ComponentMethod;
    std::string ComponentTargetInput;
};

std::vector<NarrativeNodeDefinition> GetNarrativeDefinitions();
bool FindNarrativeDefinition(const std::string& Type, NarrativeNodeDefinition& Definition);
bool ValidateNarrativeDocument(const StoryDocument& Document, std::string& Error);

class NarrativeRuntime
{
public:
    NarrativeRuntime();
    ~NarrativeRuntime();
    void Reset(ObjectHandle World);
    void Stop();
    void CancelAction();
    void BeginTransition();
    bool Start(const StoryDocument& Document, const StoryNode& Node, std::string& Error);
    bool Tick(float DeltaTime, bool& Completed, std::string& Exit, std::string& Error);
    const std::map<std::string, nlohmann::json>& GetVariables() const;

private:
    struct State;
    std::shared_ptr<State> Session;
};
