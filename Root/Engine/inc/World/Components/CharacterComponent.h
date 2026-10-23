#pragma once

#include "World/Components/MeshRendererComponent.h"
#include "Characters/CharacterDefinition.h"
#include "Assets/AssetManager.h"

class CharacterComponent : public MeshRendererComponent
{
    ENGINE_CLASS(CharacterComponent, MeshRendererComponent, "engine.CharacterComponent")
public:
    std::string Description;
    ENGINE_REFLECT_FIELD(CharacterComponent, Description, "description", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Description"))
    AssetKey Character;
    std::string Animation;
    bool bAutomaticWalk = true;
    ENGINE_REFLECT_FIELD(CharacterComponent, Character, "character", RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_EDITOR_EDITABLE, RF_ASSET_TYPE("Character"))
    ENGINE_REFLECT_FIELD(CharacterComponent, Animation, "animation", RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_EDITOR_EDITABLE)
    ENGINE_REFLECT_FIELD(CharacterComponent, bAutomaticWalk, "automatic_walk", RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_EDITOR_EDITABLE)
    std::string Diagnostic;
    std::string GetDiagnostic() const { return Diagnostic; }
    ENGINE_REFLECT_READONLY(CharacterComponent, DiagnosticProperty, "character_diagnostic", std::string, GetDiagnostic, RF_EDITOR_READONLY)

    void Resolve(AssetManager& Assets);
    void Tick(float DeltaTime) override;
    void BeginPlay() override;
    void EndPlay() override;
    void SetPreview(std::shared_ptr<const CharacterDefinition> Definition);
    void AdvancePreview(float DeltaTime);
    bool PlayAnimation(const std::string& Identifier);
    const CharacterDefinition* GetDefinition() const { return Definition.get(); }

private:
    void UpdatePose(float DeltaTime);
    AssetManager* Assets = nullptr;
    AssetKey LoadedCharacter;
    std::shared_ptr<const CharacterDefinition> Definition;
    std::string PlayingAnimation;
    float AnimationTime = 0.f;
    Vector3 PreviousPosition{};
    bool bHasPreviousPosition = false;
    bool bPreview = false;
    bool bPoseDirty = true;
};
ENGINE_CLASS_END(CharacterComponent)
ENGINE_IMPLEMENT_FIELD(CharacterComponent, Character)
ENGINE_IMPLEMENT_FIELD(CharacterComponent, Animation)
ENGINE_IMPLEMENT_FIELD(CharacterComponent, bAutomaticWalk)
ENGINE_IMPLEMENT_READONLY(CharacterComponent, DiagnosticProperty, std::string, GetDiagnostic)

ENGINE_IMPLEMENT_FIELD(CharacterComponent, Description)
