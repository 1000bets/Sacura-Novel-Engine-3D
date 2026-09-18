#include "Reflection/PendingRegistry.h"

PendingRegistry& PendingRegistry::Get() noexcept
{
    static PendingRegistry Instance;
    return Instance;
}

void PendingRegistry::AddClassRecipe(ClassRecipe& Recipe) noexcept
{
    Recipe.Next = ClassHead;
    ClassHead = &Recipe;
}

void PendingRegistry::AddStructRecipe(StructRecipe& Recipe) noexcept
{
    Recipe.Next = StructHead;
    StructHead = &Recipe;
}

void PendingRegistry::AddFieldRecipe(FieldRecipe& Recipe) noexcept
{
    Recipe.Next = FieldHead;
    FieldHead = &Recipe;
}

void PendingRegistry::AddPropertyRecipe(PropertyRecipe& Recipe) noexcept
{
    Recipe.Next = PropertyHead;
    PropertyHead = &Recipe;
}

void PendingRegistry::AddEnumRecipe(EnumRecipe& Recipe) noexcept
{
    Recipe.Next = EnumHead;
    EnumHead = &Recipe;
}
