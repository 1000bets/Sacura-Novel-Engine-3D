#include "Materials/MaterialBindings.h"

bool MaterialSlotOverrides::operator==(const MaterialSlotOverrides& Other) const
{
    if (Entries.size() != Other.Entries.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Entries.size(); ++Index)
    {
        if (Entries[Index].Slot != Other.Entries[Index].Slot || Entries[Index].Material != Other.Entries[Index].Material)
        {
            return false;
        }
    }
    return true;
}

bool MaterialPostProcessEffects::operator==(const MaterialPostProcessEffects& Other) const
{
    if (Entries.size() != Other.Entries.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Entries.size(); ++Index)
    {
        const auto& First = Entries[Index];
        const auto& Second = Other.Entries[Index];
        if (First.Material != Second.Material || First.bEnabled != Second.bEnabled || First.Intensity != Second.Intensity)
        {
            return false;
        }
    }
    return true;
}
