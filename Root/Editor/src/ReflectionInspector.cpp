#include "ReflectionInspector.h"

#include "Gameplay/Object.h"
#include "Reflection/Class.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectedValue.h"

#include <imgui.h>
#include <cstring>
#include <string>

std::vector<const PropertyDescriptor*> ReflectionInspector::ListEditableProperties(Object* Instance)
{
    std::vector<const PropertyDescriptor*> Result;
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return Result;
    }

    for (const PropertyDescriptor& Descriptor : Instance->GetClass()->GetProperties())
    {
        if (HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorVisible)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorReadOnly))
        {
            Result.push_back(&Descriptor);
        }
    }
    return Result;
}

ReflectionDiagnostic ReflectionInspector::ResetPropertyToDefault(Object* Instance, const PropertyId& Property)
{
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }

    Object* DefaultObject = Instance->GetClass()->GetClassDefaultObject();
    if (DefaultObject == nullptr)
    {
        return ReflectionDiagnostic::Fail("Class has no CDO");
    }

    ReflectedValue DefaultValue;
    ReflectionDiagnostic Got = PropertyAccess::GetProperty(DefaultObject, Property, DefaultValue);
    if (!Got.bOk)
    {
        return Got;
    }
    return PropertyAccess::SetProperty(Instance, Property, DefaultValue, PropertyAccessContext::Inspector);
}

ReflectionDiagnostic ReflectionInspector::ResetAllToDefault(Object* Instance)
{
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }

    for (const PropertyDescriptor* Descriptor : ListEditableProperties(Instance))
    {
        if (Descriptor == nullptr || Descriptor->bReadOnly)
        {
            continue;
        }
        if (!HasPropertyFlag(Descriptor->Attributes.Flags, PropertyFlags::EditorEditable))
        {
            continue;
        }
        ReflectionDiagnostic Reset = ResetPropertyToDefault(Instance, Descriptor->Id);
        if (!Reset.bOk)
        {
            return Reset;
        }
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionInspectorDrawResult ReflectionInspector::DrawProperty(Object* Instance, const PropertyDescriptor& Descriptor)
{
    ReflectionInspectorDrawResult Result;
    if (Instance == nullptr)
    {
        Result.Diagnostic = ReflectionDiagnostic::Fail("Instance is null");
        return Result;
    }

    ReflectedValue CurrentValue;
    ReflectionDiagnostic Got = PropertyAccess::GetProperty(
        Instance,
        Descriptor,
        CurrentValue,
        PropertyAccessContext::Inspector);
    if (!Got.bOk)
    {
        Result.Diagnostic = Got;
        return Result;
    }

    const char* Label = Descriptor.Attributes.DisplayName != nullptr
        ? Descriptor.Attributes.DisplayName
        : Descriptor.Id.Value.c_str();
    const bool bEditable = HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable)
        && !Descriptor.bReadOnly;

    ImGui::PushID(Descriptor.Id.Value.c_str());

    if (Descriptor.ValueTypeId.Value == "engine.bool")
    {
        bool Value = CurrentValue.BoolValue;
        if (!bEditable)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Checkbox(Label, &Value))
        {
            ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
                Instance,
                Descriptor,
                ReflectedValue::MakeBool(Value),
                PropertyAccessContext::Inspector);
            Result.bChanged = SetResult.bOk;
            Result.bRejected = !SetResult.bOk;
            Result.Diagnostic = SetResult;
        }
        if (!bEditable)
        {
            ImGui::EndDisabled();
        }
        Result.bDrawn = true;
    }
    else if (Descriptor.ValueTypeId.Value == "engine.float" || Descriptor.ValueTypeId.Value == "engine.double")
    {
        float Value = static_cast<float>(CurrentValue.Float64Value);
        if (!bEditable)
        {
            ImGui::BeginDisabled();
        }
        bool bEdited = false;
        if (Descriptor.Attributes.bHasRange)
        {
            bEdited = ImGui::SliderFloat(
                Label,
                &Value,
                static_cast<float>(Descriptor.Attributes.RangeMinimum),
                static_cast<float>(Descriptor.Attributes.RangeMaximum));
        }
        else
        {
            bEdited = ImGui::DragFloat(Label, &Value, 0.1f);
        }
        if (bEdited)
        {
            ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
                Instance,
                Descriptor,
                ReflectedValue::MakeFloat(Value),
                PropertyAccessContext::Inspector);
            Result.bChanged = SetResult.bOk;
            Result.bRejected = !SetResult.bOk;
            Result.Diagnostic = SetResult;
        }
        if (!bEditable)
        {
            ImGui::EndDisabled();
        }
        Result.bDrawn = true;
    }
    else if (Descriptor.ValueTypeId.Value == "engine.string")
    {
        std::string Value = CurrentValue.StringValue;
        char Buffer[512] = {};
        const size_t CopyCount = Value.size() < sizeof(Buffer) - 1 ? Value.size() : sizeof(Buffer) - 1;
        std::memcpy(Buffer, Value.data(), CopyCount);
        if (!bEditable)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
        {
            ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
                Instance,
                Descriptor,
                ReflectedValue::MakeString(Buffer),
                PropertyAccessContext::Inspector);
            Result.bChanged = SetResult.bOk;
            Result.bRejected = !SetResult.bOk;
            Result.Diagnostic = SetResult;
        }
        if (!bEditable)
        {
            ImGui::EndDisabled();
        }
        Result.bDrawn = true;
    }
    else if (Descriptor.ValueTypeId.Value == "engine.Vector3")
    {
        float Values[3] = {};
        if (CurrentValue.ValueKind == ReflectedValue::Kind::Bytes && CurrentValue.BytesValue.size() == sizeof(float) * 3)
        {
            std::memcpy(Values, CurrentValue.BytesValue.data(), sizeof(Values));
        }
        if (!bEditable)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::DragFloat3(Label, Values, 0.1f))
        {
            ReflectedValue Updated = ReflectedValue::MakeEmpty();
            Updated.ValueKind = ReflectedValue::Kind::Bytes;
            Updated.BytesValue.resize(sizeof(Values));
            std::memcpy(Updated.BytesValue.data(), Values, sizeof(Values));
            ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
                Instance,
                Descriptor,
                Updated,
                PropertyAccessContext::Inspector);
            Result.bChanged = SetResult.bOk;
            Result.bRejected = !SetResult.bOk;
            Result.Diagnostic = SetResult;
        }
        if (!bEditable)
        {
            ImGui::EndDisabled();
        }
        Result.bDrawn = true;
    }
    else if (Descriptor.ValueTypeId.Value == "engine.Color")
    {
        float Values[4] = {1.f, 1.f, 1.f, 1.f};
        if (CurrentValue.ValueKind == ReflectedValue::Kind::Bytes && CurrentValue.BytesValue.size() == sizeof(float) * 4)
        {
            std::memcpy(Values, CurrentValue.BytesValue.data(), sizeof(Values));
        }
        if (!bEditable)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::ColorEdit4(Label, Values))
        {
            ReflectedValue Updated = ReflectedValue::MakeEmpty();
            Updated.ValueKind = ReflectedValue::Kind::Bytes;
            Updated.BytesValue.resize(sizeof(Values));
            std::memcpy(Updated.BytesValue.data(), Values, sizeof(Values));
            ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
                Instance,
                Descriptor,
                Updated,
                PropertyAccessContext::Inspector);
            Result.bChanged = SetResult.bOk;
            Result.bRejected = !SetResult.bOk;
            Result.Diagnostic = SetResult;
        }
        if (!bEditable)
        {
            ImGui::EndDisabled();
        }
        Result.bDrawn = true;
    }

    if (bEditable && Result.bDrawn)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset"))
        {
            ReflectionDiagnostic Reset = ResetPropertyToDefault(Instance, Descriptor.Id);
            Result.bChanged = Reset.bOk;
            Result.bRejected = !Reset.bOk;
            Result.Diagnostic = Reset;
        }
    }

    ImGui::PopID();
    return Result;
}

void ReflectionInspector::DrawObject(Object* Instance, const char* PanelTitle)
{
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return;
    }

    if (!ImGui::Begin(PanelTitle))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(Instance->GetClass()->GetTypeId().Value.c_str());
    if (ImGui::Button("Reset All"))
    {
        ResetAllToDefault(Instance);
    }

    for (const PropertyDescriptor* Descriptor : ListEditableProperties(Instance))
    {
        if (Descriptor != nullptr)
        {
            DrawProperty(Instance, *Descriptor);
        }
    }

    ImGui::End();
}
