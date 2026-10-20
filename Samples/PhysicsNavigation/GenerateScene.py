from pathlib import Path
import json
import math
import struct
import uuid

Root = Path(__file__).resolve().parent
Content = Root / "Content"
for Directory in [Content / "Meshes", Content / "Materials", Content / "Scenes", Content / "Stories", Root / "Config"]:
    Directory.mkdir(parents=True, exist_ok=True)


def Identifier(Name):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "sakura-physics-navigation/" + Name))


def WriteJson(Pathname, Document):
    Pathname.write_text(json.dumps(Document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def Metadata(Pathname, Kind, Subassets=None):
    Fingerprint = 1469598103934665603
    for Byte in Pathname.read_bytes():
        Fingerprint = ((Fingerprint ^ Byte) * 1099511628211) & 0xffffffffffffffff
    WriteJson(Path(str(Pathname) + ".meta"), {
        "schemaVersion": 1, "guid": Identifier(Pathname.name), "assetType": Kind,
        "sourceFingerprint": f"{Fingerprint:016x}", "loadSettings": {}, "importInfo": {}, "subAssets": Subassets or []})


def WriteCube():
    Positions = []
    Normals = []
    Coordinates = []
    Indices = []
    Faces = [
        ([0, 1, 0], [1, 0, 0], [0, 0, -1]), ([0, -1, 0], [1, 0, 0], [0, 0, 1]),
        ([1, 0, 0], [0, 0, 1], [0, -1, 0]), ([-1, 0, 0], [0, 0, 1], [0, 1, 0]),
        ([0, 0, 1], [1, 0, 0], [0, 1, 0]), ([0, 0, -1], [1, 0, 0], [0, -1, 0])]
    for Normal, Horizontal, Vertical in Faces:
        Offset = len(Positions) // 3
        for HorizontalOffset, VerticalOffset in [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]:
            Positions.extend([Normal[Axis] * 0.5 + Horizontal[Axis] * HorizontalOffset + Vertical[Axis] * VerticalOffset
                              for Axis in range(3)])
            Normals.extend(Normal)
            Coordinates.extend([HorizontalOffset + 0.5, VerticalOffset + 0.5])
        Indices.extend([Offset, Offset + 1, Offset + 2, Offset, Offset + 2, Offset + 3])
    Binary = bytearray()
    Views = []
    Accessors = []
    for Values, Format, Width, ComponentType, Shape in [
        (Positions, "f", 3, 5126, "VEC3"), (Normals, "f", 3, 5126, "VEC3"),
        (Coordinates, "f", 2, 5126, "VEC2"), (Indices, "I", 1, 5125, "SCALAR")]:
        Payload = struct.pack("<" + Format * len(Values), *Values)
        Views.append({"buffer": 0, "byteOffset": len(Binary), "byteLength": len(Payload)})
        Accessor = {"bufferView": len(Views) - 1, "componentType": ComponentType, "count": len(Values) // Width, "type": Shape}
        if not Accessors:
            Accessor["min"] = [-0.5, -0.5, -0.5]
            Accessor["max"] = [0.5, 0.5, 0.5]
        Accessors.append(Accessor)
        Binary.extend(Payload)
    Document = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": len(Binary)}], "bufferViews": Views,
                "accessors": Accessors, "meshes": [{"primitives": [{
                    "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}]}],
                "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}
    Encoded = json.dumps(Document).encode("utf-8")
    Encoded += b" " * ((-len(Encoded)) % 4)
    Binary += b"\0" * ((-len(Binary)) % 4)
    Payload = struct.pack("<III", 0x46546C67, 2, 28 + len(Encoded) + len(Binary))
    Payload += struct.pack("<II", len(Encoded), 0x4E4F534A) + Encoded
    Payload += struct.pack("<II", len(Binary), 0x004E4942) + Binary
    Pathname = Content / "Meshes" / "CollisionCube.glb"
    Pathname.write_bytes(Payload)
    Metadata(Pathname, "Model", [{"id": Identifier("CubeMesh"), "type": "StaticMesh", "name": "CollisionCube",
                                  "selector": {"kind": "mesh", "index": 0}}])


def Material(Name, BaseColor):
    Pathname = Content / "Materials" / (Name + ".material")
    WriteJson(Pathname, {"schemaVersion": 1, "baseColor": BaseColor, "roughness": 0.8})
    Metadata(Pathname, "Material")
    return Identifier(Pathname.name)


Objects = []


def Component(Name, Type, Properties):
    return {"id": Identifier(Name), "type": "engine." + Type, "typeVersion": 1, "bEnabled": True, "properties": Properties}


def Object(Name, Position, Scale, Components, Rotation=None):
    Objects.append({"id": Identifier(Name), "name": Name, "parent": None, "bActive": True, "bVisual": True,
                    "transform": {"position": Position, "rotation": Rotation or [0, 0, 0, 1], "scale": Scale},
                    "components": Components})


def Box(Name, Position, Scale, Surface, Motion=0, TriangleCollision=False):
    Collision = {"motion_type": Motion, "shape_type": 0, "half_extents": [0.5, 0.5, 0.5],
                 "affects_navigation": Motion == 0, "continuous_collision_detection": Motion == 1}
    if TriangleCollision:
        Collision["shape_type"] = 3
    Object(Name, Position, Scale, [
        Component(Name + "Mesh", "MeshRendererComponent", {"mesh_asset_id": Identifier("CollisionCube.glb"),
                  "mesh_sub_asset_id": Identifier("CubeMesh"), "material_asset_id": Surface}),
        Component(Name + "Body", "PhysicsBodyComponent", Collision)])


WriteCube()
FloorMaterial = Material("Floor", [0.24, 0.26, 0.29, 1])
WallMaterial = Material("Wall", [0.49, 0.45, 0.47, 1])
CharacterMaterial = Material("Character", [0.68, 0.34, 0.43, 1])
BoxMaterial = Material("DynamicBox", [0.57, 0.42, 0.21, 1])
Box("Floor", [0, -0.5, 0], [16, 1, 16], FloorMaterial, TriangleCollision=True)
Box("Wall", [0, 1, 0], [1, 2, 8], WallMaterial)
for StepIndex in range(5):
    StepHeight = (StepIndex + 1) * 0.2
    Box(f"Step{StepIndex + 1}", [4, StepHeight * 0.5, -4 - StepIndex * 0.6], [2, StepHeight, 0.6], FloorMaterial)
Box("FallingBox", [5.5, 4, 3], [0.8, 0.8, 0.8], BoxMaterial, Motion=1)
Box("FirstPlatform", [-3, 2, -11], [3, 1, 3], FloorMaterial)
Box("SecondPlatform", [3, 2, -11], [3, 1, 3], FloorMaterial)
Object("NavMeshBounds", [0, 2, -2], [1, 1, 1], [Component("Bounds", "NavMeshBoundsComponent", {
    "half_extents": [10, 5, 13], "generation_mode": 1, "minimum_region_area": 0})])
Object("CostArea", [-4, 0.5, -3], [1, 1, 1], [Component("CostAreaModifier", "NavigationModifierComponent", {
    "half_extents": [1.5, 1, 2], "area": 2})])
Object("PlatformLink", [0, 0, 0], [1, 1, 1], [Component("PlatformConnection", "NavigationLinkComponent", {
    "start": [-2, 2.5, -11], "end": [2, 2.5, -11], "radius": 0.8, "bidirectional": True})])
Object("Walker", [-4, 0.1, 0], [1, 1, 1], [
    Component("WalkerController", "CharacterControllerComponent", {}),
    Component("WalkerNavigation", "NavigationAgentComponent", {}),
    {"id": Identifier("WalkerTraversal"), "type": "game.LinkTraversal", "typeVersion": 1, "bEnabled": True, "properties": {}}])
Object("WalkerVisual", [-4, 1, 0], [0.5, 1.8, 0.5], [Component("WalkerMesh", "MeshRendererComponent", {
    "mesh_asset_id": Identifier("CollisionCube.glb"), "mesh_sub_asset_id": Identifier("CubeMesh"),
    "material_asset_id": CharacterMaterial})])
Objects[-1]["parent"] = Identifier("Walker")
Objects[-1]["transform"]["position"] = [0, 0.9, 0]
Pitch = -math.atan2(12, 18)
Object("Camera", [0, 12, 18], [1, 1, 1], [Component("CameraComponent", "CameraComponent", {
    "field_of_view": 65, "near_plane": 0.1, "far_plane": 200, "primary": True})],
       [math.sin(Pitch * 0.5), 0, 0, math.cos(Pitch * 0.5)])
Object("Sun", [0, 8, 4], [1, 1, 1], [Component("SunComponent", "LightComponent", {
    "light_type": 0, "intensity": 3, "light_color": [1, 0.95, 0.9, 1], "cast_shadows": True})],
       [-0.38268343, 0, 0, 0.92387953])
ScenePath = Content / "Scenes" / "Navigation.scene"
WriteJson(ScenePath, {"format": "sakura.scene", "formatVersion": 1, "name": "Physics and navigation", "objects": Objects})
Metadata(ScenePath, "Scene")
StoryPath = Content / "Stories" / "NavigationDemo.story"
WriteJson(StoryPath, {"format": "sakura.story", "formatVersion": 1, "name": "Navigation validation", "startNodeId": "start", "nodes": [
    {"id": "start", "kind": "Line", "speaker": "Проверка навигации",
     "text": "Продолжите, чтобы персонаж обошёл стену и подошёл к точке справа.", "nextNodeId": "walk"},
    {"id": "walk", "kind": "Action", "actions": [{"kind": "NavigateTo", "targetObjectId": Identifier("Walker"),
     "position": [4, 0, 0], "durationSeconds": 30}], "nextNodeId": "arrived"},
    {"id": "arrived", "kind": "Line", "speaker": "Проверка навигации", "text": "Персонаж достиг цели. Далее проверяется подъём по ступеням.",
     "nextNodeId": "stairs"},
    {"id": "stairs", "kind": "Action", "actions": [{"kind": "NavigateTo", "targetObjectId": Identifier("Walker"),
     "position": [4, 1, -6.4], "durationSeconds": 30}], "nextNodeId": "stairsArrived"},
    {"id": "stairsArrived", "kind": "Line", "speaker": "Проверка навигации", "text": "Персонаж поднялся по ступеням. Далее проверяется переход между платформами.",
     "nextNodeId": "platforms"},
    {"id": "platforms", "kind": "Action", "actions": [
        {"kind": "MoveTo", "targetObjectId": Identifier("Walker"), "position": [-3, 2.6, -11], "durationSeconds": 0},
        {"kind": "CameraCut", "position": [0, 8, -3], "rotation": [-0.31622777, 0, 0, 0.9486833], "durationSeconds": 0},
        {"kind": "NavigateTo", "targetObjectId": Identifier("Walker"), "position": [3, 2.5, -11], "durationSeconds": 30}],
     "nextNodeId": "end"}, {"id": "end", "kind": "End"}]})
Metadata(StoryPath, "Story")
WriteJson(Root / "PhysicsNavigation.project", {"name": "PhysicsNavigation", "engineVersion": "0.1",
    "startupScene": "Content/Scenes/Navigation.scene", "startupStory": "Content/Stories/NavigationDemo.story"})
print(f"Generated {len(Objects)} objects in {Root}")
