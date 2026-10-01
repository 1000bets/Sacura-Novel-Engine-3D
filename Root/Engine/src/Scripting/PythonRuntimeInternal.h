#pragma once

#include "Scripting/ScriptingSubsystem.h"

#if defined(SAKURA_ENABLE_PYTHON)
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <deque>

namespace py = pybind11;

struct ScriptingSubsystem::ScriptInstanceRecord
{
    ScriptComponent* Component = nullptr;
    py::object Instance;
    py::object OnCreate;
    py::object OnUpdate;
    py::object OnDestroy;
    bool bDestroyInvoked = false;
};

extern std::unique_ptr<py::scoped_interpreter> EmbeddedInterpreter;
extern std::deque<std::string> OwnedAttributeStrings;
py::object FindRegisteredPythonType(const std::string& TypeIdString);
void ClearRegisteredPythonTypes();
#endif
