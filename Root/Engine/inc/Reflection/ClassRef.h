#pragma once

#include "Reflection/Class.h"
#include "Reflection/TypeId.h"

template <typename TBase>
struct ClassRef
{
    TypeId Id{};

    bool IsValid() const { return Id.IsValid(); }

    Class* Resolve() const;

    bool IsA(const Class* Candidate) const
    {
        if (Candidate == nullptr)
        {
            return false;
        }
        Class* Resolved = Resolve();
        if (Resolved == nullptr)
        {
            return false;
        }
        return Candidate->IsA(Resolved);
    }

    bool IsA(const TypeId& CandidateTypeId) const
    {
        Class* Resolved = Resolve();
        if (Resolved == nullptr)
        {
            return false;
        }
        return Resolved->IsA(CandidateTypeId);
    }

    bool operator==(const ClassRef& Other) const { return Id == Other.Id; }
    bool operator!=(const ClassRef& Other) const { return Id != Other.Id; }
};
