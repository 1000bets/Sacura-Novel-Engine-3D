#include "UI/Adapters/StoryEventAdapter.h"

namespace EditorUi
{
StoryGraphAction StoryEventAdapter::ToView(const StoryActionDefinition& Action)
{
    StoryGraphAction Value;
    Value.Identity = QString::fromStdString(Action.Id);
    Value.Kind = static_cast<int>(Action.Kind);
    Value.Duration = Action.DurationSeconds;
    Value.Target = QString::fromStdString(Action.TargetObjectId);
    Value.LegacyTarget = QString::fromStdString(Action.TargetObjectName);
    Value.AnimationId = QString::fromStdString(Action.AnimationId);
    Value.Position = {Action.TargetPosition.x, Action.TargetPosition.y, Action.TargetPosition.z};
    Value.Rotation = {Action.TargetRotation.x, Action.TargetRotation.y, Action.TargetRotation.z,
                      Action.TargetRotation.w};
    Value.bHasRotation = Action.bHasTargetRotation;
    Value.bCopyTarget = Action.bUseTargetObjectTransform;
    const auto Angles = Action.TargetRotation.ToEuler() * (180.f / 3.14159265358979323846f);
    Value.RotationDegrees = {Angles.x, Angles.y, Angles.z};
    return Value;
}
StoryActionDefinition StoryEventAdapter::ToDefinition(const StoryGraphAction& Action)
{
    StoryActionDefinition Value;
    Value.Id = Action.Identity.toStdString();
    Value.Kind = static_cast<StoryActionKind>(Action.Kind);
    Value.DurationSeconds = static_cast<float>(Action.Duration);
    Value.TargetObjectId = Action.Target.toStdString();
    Value.TargetObjectName = Action.LegacyTarget.toStdString();
    Value.AnimationId = Action.AnimationId.toStdString();
    Value.TargetPosition = Vector3(static_cast<float>(Action.Position[0]), static_cast<float>(Action.Position[1]),
                                   static_cast<float>(Action.Position[2]));
    Value.TargetRotation = Quaternion(static_cast<float>(Action.Rotation[0]), static_cast<float>(Action.Rotation[1]),
                                      static_cast<float>(Action.Rotation[2]), static_cast<float>(Action.Rotation[3]));
    if (Action.bRotationEdited)
    {
        constexpr float Radians = 3.14159265358979323846f / 180.f;
        Value.TargetRotation =
            Quaternion::CreateFromYawPitchRoll(static_cast<float>(Action.RotationDegrees[1]) * Radians,
                                               static_cast<float>(Action.RotationDegrees[0]) * Radians,
                                               static_cast<float>(Action.RotationDegrees[2]) * Radians);
    }
    Value.bHasTargetRotation = Action.bHasRotation;
    Value.bUseTargetObjectTransform = Action.bCopyTarget;
    return Value;
}
StoryGraphEvent StoryEventAdapter::ToView(const StoryEventDefinition& Event)
{
    StoryGraphEvent Value;
    Value.Identity = QString::fromStdString(Event.Id);
    Value.Name = QString::fromStdString(Event.Name);
    Value.Description = QString::fromStdString(Event.Description);
    for (const auto& Group : Event.Groups)
    {
        StoryGraphEventGroup Item{QString::fromStdString(Group.Id), QString::fromStdString(Group.Name)};
        for (const auto& Action : Group.Actions)
        {
            Item.Actions.push_back(ToView(Action));
        }
        Value.Groups.push_back(std::move(Item));
    }
    return Value;
}
StoryEventDefinition StoryEventAdapter::ToDefinition(const StoryGraphEvent& Event)
{
    StoryEventDefinition Value;
    Value.Id = Event.Identity.toStdString();
    Value.Name = Event.Name.toStdString();
    Value.Description = Event.Description.toStdString();
    for (const auto& Group : Event.Groups)
    {
        StoryEventGroup Item{Group.Identity.toStdString(), Group.Name.toStdString()};
        for (const auto& Action : Group.Actions)
        {
            Item.Actions.push_back(ToDefinition(Action));
        }
        Value.Groups.push_back(std::move(Item));
    }
    return Value;
}
} // namespace EditorUi
