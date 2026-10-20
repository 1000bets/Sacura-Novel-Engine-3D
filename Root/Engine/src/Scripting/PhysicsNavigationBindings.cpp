#include "Scripting/PythonRuntimeInternal.h"

#if defined(SAKURA_ENABLE_PYTHON)
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/Components/NavigationAgentComponent.h"
#include "Physics/PhysicsWorld.h"
#include "Navigation/NavigationWorld.h"
#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"

#include <array>

static GameObject& ResolveSimulationObject(const py::object& Value)
{
    AssertGameThread();
    const ObjectHandle Identity{Value.attr("_id").cast<uint64_t>(), Value.attr("_generation").cast<uint32_t>()};
    GameObject* Owner = MemorySubsystem::Get()->ResolveHandle<GameObject>(Identity);
    if (Owner == nullptr || Owner->GetScene() == nullptr || Owner->IsPendingDestroy())
    {
        throw py::value_error("Simulation object is no longer available");
    }
    return *Owner;
}

static Vector3 ReadSimulationVector(const py::object& Value)
{
    if (py::hasattr(Value, "x") && py::hasattr(Value, "y") && py::hasattr(Value, "z"))
    {
        return Vector3(Value.attr("x").cast<float>(), Value.attr("y").cast<float>(), Value.attr("z").cast<float>());
    }
    const auto Coordinates = Value.cast<std::array<float, 3>>();
    return Vector3(Coordinates[0], Coordinates[1], Coordinates[2]);
}

static py::object MakeSimulationVector(const Vector3& Value)
{
    return py::module_::import("engine").attr("Vector3")(Value.x, Value.y, Value.z);
}

static py::object MakeSimulationObject(const ObjectHandle& Identity)
{
    if (!Identity.IsValid())
    {
        return py::none();
    }
    return py::module_::import("engine").attr("Object")(Identity.Id, Identity.Generation);
}

static PhysicsBodyComponent& ResolveSimulationBody(const py::object& Value)
{
    PhysicsBodyComponent* Body = ResolveSimulationObject(Value).GetComponent<PhysicsBodyComponent>();
    if (Body == nullptr || Body->IsPendingRemoval())
    {
        throw py::value_error("Object does not have a physics body");
    }
    return *Body;
}

static NavigationAgentComponent& ResolveNavigationAgent(const py::object& Value)
{
    NavigationAgentComponent* Agent = ResolveSimulationObject(Value).GetComponent<NavigationAgentComponent>();
    if (Agent == nullptr || Agent->IsPendingRemoval())
    {
        throw py::value_error("Object does not have a navigation agent");
    }
    return *Agent;
}

static py::dict MakeSimulationHit(const PhysicsHit& Hit)
{
    py::dict Result;
    Result["Object"] = MakeSimulationObject(Hit.Object);
    Result["Body"] = Hit.Body;
    Result["Position"] = MakeSimulationVector(Hit.Position);
    Result["Normal"] = MakeSimulationVector(Hit.Normal);
    Result["Distance"] = Hit.Distance;
    Result["InitialOverlap"] = Hit.bInitialOverlap;
    return Result;
}

void RegisterPhysicsNavigationBindings(py::module_& Module)
{
    py::enum_<NavigationAgentState>(Module, "NavigationAgentState")
        .value("Idle", NavigationAgentState::Idle)
        .value("WaitingForNavigation", NavigationAgentState::WaitingForNavigation)
        .value("Moving", NavigationAgentState::Moving)
        .value("Reached", NavigationAgentState::Reached)
        .value("PartialReached", NavigationAgentState::PartialReached)
        .value("WaitingForLink", NavigationAgentState::WaitingForLink)
        .value("Blocked", NavigationAgentState::Blocked)
        .value("Failed", NavigationAgentState::Failed)
        .value("Cancelled", NavigationAgentState::Cancelled);
    py::enum_<NavigationPathStatus>(Module, "NavigationPathStatus")
        .value("Complete", NavigationPathStatus::Complete)
        .value("Partial", NavigationPathStatus::Partial)
        .value("NotReady", NavigationPathStatus::NotReady)
        .value("StartOutsideNavigation", NavigationPathStatus::StartOutsideNavigation)
        .value("GoalOutsideNavigation", NavigationPathStatus::GoalOutsideNavigation)
        .value("Unreachable", NavigationPathStatus::Unreachable)
        .value("SearchLimitReached", NavigationPathStatus::SearchLimitReached)
        .value("InvalidInput", NavigationPathStatus::InvalidInput);
    py::class_<PhysicsQueryFilter>(Module, "PhysicsQueryFilter")
        .def(py::init<>())
        .def_readwrite("LayerMask", &PhysicsQueryFilter::LayerMask)
        .def_readwrite("IgnoredBody", &PhysicsQueryFilter::IgnoredBody)
        .def_readwrite("IncludeTriggers", &PhysicsQueryFilter::bIncludeTriggers);
    py::class_<NavigationQueryFilter>(Module, "NavigationQueryFilter")
        .def(py::init<>())
        .def_readwrite("AreaCosts", &NavigationQueryFilter::AreaCosts)
        .def_readwrite("IncludedAreas", &NavigationQueryFilter::IncludedAreas)
        .def_readwrite("ExcludedAreas", &NavigationQueryFilter::ExcludedAreas)
        .def_readwrite("MaximumSearchNodes", &NavigationQueryFilter::MaximumSearchNodes)
        .def_readwrite("MaximumPathPolygons", &NavigationQueryFilter::MaximumPathPolygons)
        .def_readwrite("AllowPartialPath", &NavigationQueryFilter::bAllowPartialPath);

    Module.def("MoveTo", [](const py::object& Owner, const py::object& Destination)
    {
        return ResolveNavigationAgent(Owner).MoveTo(ReadSimulationVector(Destination));
    });
    Module.def("StopNavigation", [](const py::object& Owner)
    {
        ResolveNavigationAgent(Owner).Stop();
    });
    Module.def("GetNavigationState", [](const py::object& Owner)
    {
        return ResolveNavigationAgent(Owner).GetMovementState();
    });
    Module.def("GetNavigationDiagnostic", [](const py::object& Owner)
    {
        return ResolveNavigationAgent(Owner).GetDiagnostic();
    });
    Module.def("GetPendingNavigationLinkEnd", [](const py::object& Owner)
    {
        return MakeSimulationVector(ResolveNavigationAgent(Owner).GetPendingLinkEnd());
    });
    Module.def("CompleteNavigationLink", [](const py::object& Owner, bool bSuccess)
    {
        return ResolveNavigationAgent(Owner).CompleteLinkTraversal(bSuccess);
    });
    Module.def("SetNavigationAreaCost", [](const py::object& Owner, int Area, float Cost)
    {
        return ResolveNavigationAgent(Owner).SetAreaCost(Area, Cost);
    });
    Module.def("FindNavigationPath", [](const py::object& Context, const py::object& Start,
        const py::object& Goal, const std::string& Profile, const NavigationQueryFilter& Filter)
    {
        const NavigationPath Path = ResolveSimulationObject(Context).GetScene()->GetNavigation().FindPath(
            ReadSimulationVector(Start), ReadSimulationVector(Goal), Profile, Filter);
        py::dict Result;
        Result["Status"] = Path.Status;
        Result["Generation"] = Path.Generation;
        Result["RequestedGoal"] = MakeSimulationVector(Path.RequestedGoal);
        Result["ProjectedGoal"] = MakeSimulationVector(Path.ProjectedGoal);
        Result["Diagnostic"] = Path.Diagnostic;
        py::list Points;
        for (const NavigationPathPoint& Point : Path.Points)
        {
            py::dict Item;
            Item["Position"] = MakeSimulationVector(Point.Position);
            Item["OffMeshConnection"] = Point.bOffMeshConnection;
            Item["LinkIdentity"] = Point.LinkIdentity;
            Item["LinkEnd"] = MakeSimulationVector(Point.LinkEnd);
            Points.append(Item);
        }
        Result["Points"] = Points;
        return Result;
    }, py::arg("Context"), py::arg("Start"), py::arg("Goal"), py::arg("Profile") = "Default",
        py::arg("Filter") = NavigationQueryFilter{});
    Module.def("ProjectPointToNavigation", [](const py::object& Context, const py::object& Position,
        const std::string& Profile, const py::object& Extent, const NavigationQueryFilter& Filter) -> py::object
    {
        Vector3 Projected;
        if (ResolveSimulationObject(Context).GetScene()->GetNavigation().ProjectPoint(
            ReadSimulationVector(Position), Projected, Profile, ReadSimulationVector(Extent), Filter))
        {
            return MakeSimulationVector(Projected);
        }
        return py::none();
    }, py::arg("Context"), py::arg("Position"), py::arg("Profile") = "Default",
        py::arg("Extent") = py::make_tuple(1.f, 2.f, 1.f), py::arg("Filter") = NavigationQueryFilter{});
    Module.def("AddForce", [](const py::object& Owner, const py::object& Force)
    {
        return ResolveSimulationBody(Owner).AddForce(ReadSimulationVector(Force));
    });
    Module.def("AddImpulse", [](const py::object& Owner, const py::object& Impulse)
    {
        return ResolveSimulationBody(Owner).AddImpulse(ReadSimulationVector(Impulse));
    });
    Module.def("SetPhysicsVelocity", [](const py::object& Owner, const py::object& Velocity)
    {
        return ResolveSimulationBody(Owner).SetVelocity(ReadSimulationVector(Velocity));
    });
    Module.def("AddTorque", [](const py::object& Owner, const py::object& Torque)
    {
        return ResolveSimulationBody(Owner).AddTorque(ReadSimulationVector(Torque));
    });
    Module.def("AddAngularImpulse", [](const py::object& Owner, const py::object& Impulse)
    {
        return ResolveSimulationBody(Owner).AddAngularImpulse(ReadSimulationVector(Impulse));
    });
    Module.def("SetAngularVelocity", [](const py::object& Owner, const py::object& Velocity)
    {
        return ResolveSimulationBody(Owner).SetAngularVelocity(ReadSimulationVector(Velocity));
    });
    Module.def("GetAngularVelocity", [](const py::object& Owner)
    {
        return MakeSimulationVector(ResolveSimulationBody(Owner).GetAngularVelocity());
    });
    Module.def("GetPhysicsVelocity", [](const py::object& Owner)
    {
        return MakeSimulationVector(ResolveSimulationBody(Owner).GetVelocity());
    });
    Module.def("SetCharacterVelocity", [](const py::object& Owner, const py::object& Velocity)
    {
        ResolveSimulationBody(Owner).SetCharacterVelocity(ReadSimulationVector(Velocity));
    });
    Module.def("Jump", [](const py::object& Owner, float Speed)
    {
        ResolveSimulationBody(Owner).Jump(Speed);
    });
    Module.def("IsGrounded", [](const py::object& Owner)
    {
        return ResolveSimulationBody(Owner).IsGrounded();
    });
    Module.def("Raycast", [](const py::object& Context, const py::object& Origin, const py::object& Direction,
        float Distance, const PhysicsQueryFilter& Filter) -> py::object
    {
        PhysicsHit Hit;
        if (ResolveSimulationObject(Context).GetScene()->GetPhysics().Raycast(
            ReadSimulationVector(Origin), ReadSimulationVector(Direction), Distance, Hit, Filter))
        {
            return MakeSimulationHit(Hit);
        }
        return py::none();
    }, py::arg("Context"), py::arg("Origin"), py::arg("Direction"), py::arg("Distance"), py::arg("Filter") = PhysicsQueryFilter{});
    Module.def("SphereSweep", [](const py::object& Context, const py::object& Origin, float Radius,
        const py::object& Direction, float Distance, const PhysicsQueryFilter& Filter) -> py::object
    {
        PhysicsBodyDefinition Shape;
        Shape.ShapeType = PhysicsShapeType::Sphere;
        Shape.Radius = Radius;
        Shape.WorldTransform.Position = ReadSimulationVector(Origin);
        PhysicsHit Hit;
        if (ResolveSimulationObject(Context).GetScene()->GetPhysics().Sweep(Shape, ReadSimulationVector(Direction), Distance, Hit, Filter))
        {
            return MakeSimulationHit(Hit);
        }
        return py::none();
    }, py::arg("Context"), py::arg("Origin"), py::arg("Radius"), py::arg("Direction"), py::arg("Distance"),
        py::arg("Filter") = PhysicsQueryFilter{});
    Module.def("OverlapSphere", [](const py::object& Context, const py::object& Origin, float Radius, const PhysicsQueryFilter& Filter)
    {
        PhysicsBodyDefinition Shape;
        Shape.ShapeType = PhysicsShapeType::Sphere;
        Shape.Radius = Radius;
        Shape.WorldTransform.Position = ReadSimulationVector(Origin);
        py::list Result;
        for (const PhysicsHit& Hit : ResolveSimulationObject(Context).GetScene()->GetPhysics().Overlap(Shape, Filter))
        {
            Result.append(MakeSimulationHit(Hit));
        }
        return Result;
    }, py::arg("Context"), py::arg("Origin"), py::arg("Radius"), py::arg("Filter") = PhysicsQueryFilter{});
    Module.def("GetPhysicsContacts", [](const py::object& Context)
    {
        py::list Result;
        for (const PhysicsContact& Contact : ResolveSimulationObject(Context).GetScene()->GetPhysics().GetContacts())
        {
            py::dict Item;
            Item["FirstObject"] = MakeSimulationObject(Contact.FirstObject);
            Item["SecondObject"] = MakeSimulationObject(Contact.SecondObject);
            Item["Kind"] = static_cast<int>(Contact.Kind);
            Item["Trigger"] = Contact.bTrigger;
            Item["Position"] = MakeSimulationVector(Contact.Position);
            Item["Normal"] = MakeSimulationVector(Contact.Normal);
            Item["Impulse"] = MakeSimulationVector(Contact.Impulse);
            Result.append(Item);
        }
        return Result;
    });
    Module.def("SetWorldGravity", [](const py::object& Context, const py::object& Gravity)
    {
        ResolveSimulationObject(Context).GetScene()->GetPhysics().SetGravity(ReadSimulationVector(Gravity));
    });
}
#endif
