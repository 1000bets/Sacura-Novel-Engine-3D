#pragma once

#include "Core/Types.h"
#include "Core/Object/ObjectHandle.h"
#include "Rendering/AxisAlignedBounds.h"

class QTreeWidgetItem;
inline constexpr int HierarchyObjectIdRole = 256;
inline constexpr int HierarchyObjectGenerationRole = 257;
inline constexpr float DegreesToRadians = 0.01745329251994329577f;
inline constexpr float RadiansToDegrees = 57.295779513082320876f;
void StoreObjectHandle(QTreeWidgetItem* Item, ObjectHandle Identity);
ObjectHandle LoadObjectHandle(const QTreeWidgetItem* Item);
bool HierarchyItemIsDescendant(const QTreeWidgetItem* Ancestor, const QTreeWidgetItem* Candidate);
Vector3 QuaternionToEulerDegrees(const Quaternion& Rotation);
bool RayIntersectsBounds(const Vector3& RayOrigin, const Vector3& RayDirection, const AxisAlignedBounds& Bounds, float& OutDistance);
