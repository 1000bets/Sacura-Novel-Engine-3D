from pathlib import Path
import json
import math
import struct
import uuid
import zlib

Root = Path(__file__).resolve().parent
Content = Root / "Content"
for Directory in [Content / "Meshes", Content / "Materials", Content / "Textures", Content / "Scenes", Root / "Scripts", Root / "Config"]:
    Directory.mkdir(parents=True, exist_ok=True)

def Identifier(Name):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "sakura-render-validation/" + Name))

def WriteJson(Pathname, Document):
    Pathname.write_text(json.dumps(Document, indent=2) + "\n", encoding="utf-8")

def Metadata(Pathname, Kind, Subassets=None):
    Hash = 1469598103934665603
    for Byte in Pathname.read_bytes():
        Hash = ((Hash ^ Byte) * 1099511628211) & 0xffffffffffffffff
    WriteJson(Path(str(Pathname) + ".meta"), {
        "schemaVersion": 1, "guid": Identifier(Pathname.name), "assetType": Kind,
        "sourceFingerprint": f"{Hash:016x}", "loadSettings": {}, "importInfo": {}, "subAssets": Subassets or []})

def WriteMesh(Name, Positions, Normals, Coordinates, Indices):
    Binary = bytearray()
    Views = []
    Accessors = []
    for Values, Format, Components, ComponentType, Shape in [
        (Positions, "f", 3, 5126, "VEC3"), (Normals, "f", 3, 5126, "VEC3"),
        (Coordinates, "f", 2, 5126, "VEC2"), (Indices, "I", 1, 5125, "SCALAR")]:
        Payload = struct.pack("<" + Format * len(Values), *Values)
        Views.append({"buffer": 0, "byteOffset": len(Binary), "byteLength": len(Payload)})
        Accessor = {"bufferView": len(Views) - 1, "componentType": ComponentType, "count": len(Values) // Components, "type": Shape}
        if not Accessors:
            Accessor["min"] = [min(Positions[Axis::3]) for Axis in range(3)]
            Accessor["max"] = [max(Positions[Axis::3]) for Axis in range(3)]
        Accessors.append(Accessor)
        Binary.extend(Payload)
    Document = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": len(Binary)}], "bufferViews": Views,
        "accessors": Accessors, "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}
    Json = json.dumps(Document).encode()
    Json += b" " * ((-len(Json)) % 4)
    Binary += b"\0" * ((-len(Binary)) % 4)
    Payload = struct.pack("<III", 0x46546c67, 2, 28 + len(Json) + len(Binary))
    Payload += struct.pack("<II", len(Json), 0x4e4f534a) + Json + struct.pack("<II", len(Binary), 0x004e4942) + Binary
    Pathname = Content / "Meshes" / (Name + ".glb")
    Pathname.write_bytes(Payload)
    Metadata(Pathname, "Model", [{"id": Identifier(Name + "-mesh"), "type": "StaticMesh", "name": Name, "selector": {"kind": "mesh", "index": 0}}])

Positions, Normals, Coordinates, Indices = [], [], [], []
for Row in range(17):
    Polar = math.pi * Row / 16
    for Column in range(33):
        Azimuth = 2 * math.pi * Column / 32
        Normal = [math.sin(Polar) * math.cos(Azimuth), math.cos(Polar), math.sin(Polar) * math.sin(Azimuth)]
        Positions.extend([Value * 0.45 for Value in Normal])
        Normals.extend(Normal)
        Coordinates.extend([Column / 32, Row / 16])
for Row in range(16):
    for Column in range(32):
        First = Row * 33 + Column
        Second = First + 33
        Indices.extend([First, First + 1, Second, First + 1, Second + 1, Second])
WriteMesh("Sphere", Positions, Normals, Coordinates, Indices)
WriteMesh("Plane", [-0.5, -0.5, 0, 0.5, -0.5, 0, 0.5, 0.5, 0, -0.5, 0.5, 0], [0, 0, 1] * 4, [0, 1, 1, 1, 1, 0, 0, 0], [0, 1, 2, 0, 2, 3])

def Chunk(Kind, Data):
    return struct.pack(">I", len(Data)) + Kind + Data + struct.pack(">I", zlib.crc32(Kind + Data) & 0xffffffff)
Pixels = bytearray()
for Row in range(32):
    Pixels.append(0)
    for Column in range(32):
        Alpha = 255
        if ((Row // 4) + (Column // 4)) % 2:
            Alpha = 0
        Pixels.extend([255, 255, 255, Alpha])
TexturePath = Content / "Textures" / "Cutout.png"
TexturePath.write_bytes(b"\x89PNG\r\n\x1a\n" + Chunk(b"IHDR", struct.pack(">IIBBBBB", 32, 32, 8, 6, 0, 0, 0)) + Chunk(b"IDAT", zlib.compress(Pixels)) + Chunk(b"IEND", b""))
Metadata(TexturePath, "Texture")

def Material(Name, **Properties):
    Pathname = Content / "Materials" / (Name + ".material")
    WriteJson(Pathname, {"schemaVersion": 1, **Properties})
    Metadata(Pathname, "Material")
    return Identifier(Pathname.name)

Objects = []
def Object(Name, Position, Scale, Rotation, ComponentType, Properties):
    ObjectId = Identifier(Name)
    Objects.append({"id": ObjectId, "name": Name, "parent": None, "bActive": True, "bVisual": True,
        "transform": {"position": Position, "rotation": Rotation, "scale": Scale},
        "components": [{"id": Identifier(Name + "-component"), "type": ComponentType, "typeVersion": 1, "bEnabled": True, "properties": Properties}]})

def Mesh(Name, Shape, Surface, Position, Scale=[1, 1, 1], Rotation=[0, 0, 0, 1]):
    Object(Name, Position, Scale, Rotation, "engine.MeshRendererComponent", {
        "mesh_asset_id": Identifier(Shape + ".glb"), "mesh_sub_asset_id": Identifier(Shape + "-mesh"), "material_asset_id": Surface, "visible": True})

for Row in range(2):
    for Column in range(5):
        Name = f"PBR-{Row}-{Column}"
        Surface = Material(Name, baseColor=[0.85, 0.48, 0.12, 1], metallic=Row, roughness=0.05 + Column * 0.23)
        Mesh(Name, "Sphere", Surface, [(Column - 2) * 1.05, 0.55 + Row * 1.05, 0])
Floor = Material("Floor", baseColor=[0.45, 0.45, 0.48, 1], roughness=0.8, doubleSided=True)
Mesh("Floor", "Plane", Floor, [0, -0.1, 0], [12, 12, 1], [-0.70710678, 0, 0, 0.70710678])
GlassRed = Material("GlassRed", baseColor=[1, 0.1, 0.08, 0.35], alphaMode="BLEND", roughness=0.2, doubleSided=True)
GlassBlue = Material("GlassBlue", baseColor=[0.08, 0.3, 1, 0.55], alphaMode="BLEND", roughness=0.3, doubleSided=True)
Mesh("IntersectingRed", "Plane", GlassRed, [-0.6, 0.7, 1.1], [2.4, 1.3, 1], [0, 0.38268343, 0, 0.92387953])
Mesh("IntersectingBlue", "Plane", GlassBlue, [0.6, 0.7, 1.1], [2.4, 1.3, 1], [0, -0.38268343, 0, 0.92387953])
Cutout = Material("Cutout", baseColor=[0.1, 0.8, 0.3, 1], alphaMode="MASK", alphaCutoff=0.5, doubleSided=True,
    baseColorTexture={"assetId": Identifier("Cutout.png"), "subAssetId": None})
Mesh("Cutout", "Plane", Cutout, [2.8, 1.1, -0.5], [1.2, 1.8, 1])
Emission = Material("Emission", baseColor=[1, 0.2, 0.1, 1], emissive=[12, 2, 0.3])
Mesh("Emission", "Sphere", Emission, [-2.8, 1.1, -0.5], [0.6, 0.6, 0.6])
for Index in range(64):
    Mesh(f"Offscreen-{Index}", "Sphere", Floor, [100 + Index, 0, 0])
Object("Camera", [0, 2, 7], [1, 1, 1], [0, 0, 0, 1], "engine.CameraComponent",
    {"field_of_view": 60, "near_plane": 0.1, "far_plane": 200, "aspect_ratio": 1.7777778, "primary": True})
Object("Sun", [0, 5, 3], [1, 1, 1], [-0.38268343, 0, 0, 0.92387953], "engine.LightComponent",
    {"light_type": 0, "light_color": [1, 0.95, 0.85, 1], "intensity": 3, "cast_shadows": True})
Object("Point", [-2, 2.5, 2], [1, 1, 1], [0, 0, 0, 1], "engine.LightComponent",
    {"light_type": 1, "light_color": [0.3, 0.5, 1, 1], "intensity": 12, "range": 8, "cast_shadows": True})
Object("Spot", [2, 3, 2], [1, 1, 1], [-0.258819, 0, 0, 0.9659258], "engine.LightComponent",
    {"light_type": 2, "light_color": [1, 0.3, 0.1, 1], "intensity": 15, "range": 8, "inner_cone_angle": 15, "outer_cone_angle": 35, "cast_shadows": True})
WriteJson(Content / "Scenes" / "Forward.scene", {"format": "sakura.scene", "formatVersion": 1, "name": "Forward validation", "objects": Objects})
WriteJson(Root / "RenderValidation.project", {"name": "RenderValidation", "engineVersion": "0.1", "startupScene": "Content/Scenes/Forward.scene"})
print(f"Generated {len(Objects)} objects and {len(list(Content.rglob('*.meta')))} assets")
