#pragma once

#include "Reflection/PropertyDescriptor.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <vector>

class Object;

struct ReflectionInspectorDrawResult
{
    bool bDrawn = false;
    bool bChanged = false;
    bool bRejected = false;
    ReflectionDiagnostic Diagnostic = ReflectionDiagnostic::Ok();
};

class ReflectionInspector
{
public:
    static void DrawObject(Object* Instance, const char* PanelTitle = "Properties");
    static ReflectionInspectorDrawResult DrawProperty(Object* Instance, const PropertyDescriptor& Descriptor);
    static ReflectionDiagnostic ResetPropertyToDefault(Object* Instance, const PropertyId& Property);
    static ReflectionDiagnostic ResetAllToDefault(Object* Instance);
    static std::vector<const PropertyDescriptor*> ListEditableProperties(Object* Instance);
};
