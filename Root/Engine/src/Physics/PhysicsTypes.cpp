#include "Physics/PhysicsTypes.h"

#include <algorithm>
#include <cmath>

static bool IsFinitePhysicsVector(const Vector3& Value)
{
    return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
}

bool ValidatePhysicsBody(const PhysicsBodyDefinition& Definition, std::string& Error)
{
    Error.clear();
    if (Definition.MotionType < PhysicsMotionType::Static || Definition.MotionType > PhysicsMotionType::Character
        || Definition.ShapeType < PhysicsShapeType::Box || Definition.ShapeType > PhysicsShapeType::TriangleMesh)
    {
        Error = "Unknown physics motion or shape type";
        return false;
    }
    const Quaternion& Rotation = Definition.WorldTransform.Rotation;
    if (!IsFinitePhysicsVector(Definition.WorldTransform.Position) || !IsFinitePhysicsVector(Definition.Center)
        || !std::isfinite(Rotation.x) || !std::isfinite(Rotation.y) || !std::isfinite(Rotation.z)
        || !std::isfinite(Rotation.w) || Rotation.LengthSquared() < 0.000001f)
    {
        Error = "Physics body transform is not finite or has an invalid rotation";
        return false;
    }
    if (!std::isfinite(Definition.Mass) || Definition.Mass <= 0.f
        || !std::isfinite(Definition.StaticFriction) || Definition.StaticFriction < 0.f
        || !std::isfinite(Definition.DynamicFriction) || Definition.DynamicFriction < 0.f
        || !std::isfinite(Definition.Restitution) || Definition.Restitution < 0.f || Definition.Restitution > 1.f
        || !std::isfinite(Definition.LinearDamping) || Definition.LinearDamping < 0.f
        || !std::isfinite(Definition.AngularDamping) || Definition.AngularDamping < 0.f)
    {
        Error = "Physics body mass, damping or material parameters are invalid";
        return false;
    }
    if (Definition.ShapeType == PhysicsShapeType::Box
        && (!IsFinitePhysicsVector(Definition.HalfExtents) || Definition.HalfExtents.x <= 0.f
            || Definition.HalfExtents.y <= 0.f || Definition.HalfExtents.z <= 0.f))
    {
        Error = "Box half extents must be finite and positive";
        return false;
    }
    if ((Definition.ShapeType == PhysicsShapeType::Sphere || Definition.ShapeType == PhysicsShapeType::Capsule)
        && (!std::isfinite(Definition.Radius) || Definition.Radius <= 0.f))
    {
        Error = "Sphere or capsule radius must be finite and positive";
        return false;
    }
    if (Definition.ShapeType == PhysicsShapeType::Capsule
        && (!std::isfinite(Definition.CapsuleHalfHeight) || Definition.CapsuleHalfHeight <= 0.f))
    {
        Error = "Capsule cylinder half height must be finite and positive";
        return false;
    }
    if (Definition.MotionType == PhysicsMotionType::Character
        && (Definition.ShapeType != PhysicsShapeType::Capsule || Definition.bTrigger || !Definition.bSimulationEnabled
            || Definition.Center != Vector3::Zero
            || !std::isfinite(Definition.StepHeight) || Definition.StepHeight < 0.f
            || Definition.StepHeight >= 2.f * (Definition.CapsuleHalfHeight + Definition.Radius)
            || !std::isfinite(Definition.MaximumSlopeDegrees) || Definition.MaximumSlopeDegrees < 0.f
            || Definition.MaximumSlopeDegrees >= 90.f))
    {
        Error = "Character requires a solid capsule, zero center offset, valid step height and slope; object origin is at its feet";
        return false;
    }
    if (Definition.ShapeType == PhysicsShapeType::TriangleMesh)
    {
        if (Definition.MotionType == PhysicsMotionType::Dynamic || Definition.MotionType == PhysicsMotionType::Character
            || Definition.bTrigger)
        {
            Error = "Triangle mesh collision supports only static or kinematic solid bodies";
            return false;
        }
        if (Definition.TriangleMesh.Vertices.empty() || Definition.TriangleMesh.Indices.empty()
            || Definition.TriangleMesh.Indices.size() % 3 != 0)
        {
            Error = "Triangle mesh collision requires vertices and triangle indices";
            return false;
        }
        for (const Vector3& Vertex : Definition.TriangleMesh.Vertices)
        {
            if (!IsFinitePhysicsVector(Vertex))
            {
                Error = "Triangle mesh contains a non-finite vertex";
                return false;
            }
        }
        for (uint32_t VertexIndex : Definition.TriangleMesh.Indices)
        {
            if (VertexIndex >= Definition.TriangleMesh.Vertices.size())
            {
                Error = "Triangle mesh index is out of range";
                return false;
            }
        }
    }
    if (Definition.CollisionLayer == 0 || (Definition.CollisionLayer & (Definition.CollisionLayer - 1)) != 0)
    {
        Error = "Collision layer must contain exactly one bit";
        return false;
    }
    return true;
}

PhysicsTriangleMesh MakePhysicsTriangleMesh(const PhysicsBodyDefinition& Definition)
{
    PhysicsTriangleMesh Mesh;
    if (Definition.ShapeType == PhysicsShapeType::TriangleMesh)
    {
        Mesh = Definition.TriangleMesh;
    }
    else if (Definition.ShapeType == PhysicsShapeType::Box)
    {
        const Vector3& Extents = Definition.HalfExtents;
        Mesh.Vertices = {
            {-Extents.x, -Extents.y, -Extents.z}, {Extents.x, -Extents.y, -Extents.z},
            {Extents.x, -Extents.y, Extents.z}, {-Extents.x, -Extents.y, Extents.z},
            {-Extents.x, Extents.y, -Extents.z}, {Extents.x, Extents.y, -Extents.z},
            {Extents.x, Extents.y, Extents.z}, {-Extents.x, Extents.y, Extents.z}};
        Mesh.Indices = {0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,4,5, 0,5,1,
            1,5,6, 1,6,2, 2,6,7, 2,7,3, 3,7,4, 3,4,0};
    }
    else
    {
        constexpr int LongitudeCount = 24;
        constexpr int LatitudeCount = 16;
        constexpr float Pi = 3.14159265358979323846f;
        int RingCount = LatitudeCount;
        if (Definition.ShapeType == PhysicsShapeType::Capsule)
        {
            ++RingCount;
        }
        for (int Latitude = 0; Latitude <= RingCount; ++Latitude)
        {
            int SphereLatitude = Latitude;
            if (Definition.ShapeType == PhysicsShapeType::Capsule && Latitude > LatitudeCount / 2)
            {
                --SphereLatitude;
            }
            const float Angle = Pi * static_cast<float>(SphereLatitude) / LatitudeCount;
            float Vertical = Definition.Radius * std::cos(Angle);
            if (Definition.ShapeType == PhysicsShapeType::Capsule)
            {
                if (Latitude <= LatitudeCount / 2)
                {
                    Vertical += Definition.CapsuleHalfHeight;
                }
                else
                {
                    Vertical -= Definition.CapsuleHalfHeight;
                }
            }
            for (int Longitude = 0; Longitude < LongitudeCount; ++Longitude)
            {
                const float Direction = 2.f * Pi * static_cast<float>(Longitude) / LongitudeCount;
                Mesh.Vertices.emplace_back(Definition.Radius * std::sin(Angle) * std::cos(Direction),
                    Vertical, Definition.Radius * std::sin(Angle) * std::sin(Direction));
            }
        }
        for (int Latitude = 0; Latitude < RingCount; ++Latitude)
        {
            for (int Longitude = 0; Longitude < LongitudeCount; ++Longitude)
            {
                const uint32_t First = Latitude * LongitudeCount + Longitude;
                const uint32_t Second = Latitude * LongitudeCount + (Longitude + 1) % LongitudeCount;
                Mesh.Indices.insert(Mesh.Indices.end(), {First, Second, First + LongitudeCount,
                    Second, Second + LongitudeCount, First + LongitudeCount});
            }
        }
    }
    Quaternion Rotation = Definition.WorldTransform.Rotation;
    Rotation.Normalize();
    for (Vector3& Vertex : Mesh.Vertices)
    {
        Vertex = Vector3::Transform(Vertex + Definition.Center, Rotation) + Definition.WorldTransform.Position;
    }
    return Mesh;
}
