import json
import math
from pathlib import Path
import struct
import uuid


OutputDirectory = Path(__file__).resolve().parents[1] / "Root/Engine/Content/BasicShapes"
MaterialIdentifier = "42b33bd1-5bf7-5462-920b-297de12b0f18"


def WriteMesh(Name, Positions, Normals, Coordinates, Triangles, Collision):
    Indices = []
    for First, Second, Third in Triangles:
        EdgeFirst = [Positions[Second][Axis] - Positions[First][Axis] for Axis in range(3)]
        EdgeSecond = [Positions[Third][Axis] - Positions[First][Axis] for Axis in range(3)]
        Cross = [EdgeFirst[1] * EdgeSecond[2] - EdgeFirst[2] * EdgeSecond[1],
                 EdgeFirst[2] * EdgeSecond[0] - EdgeFirst[0] * EdgeSecond[2],
                 EdgeFirst[0] * EdgeSecond[1] - EdgeFirst[1] * EdgeSecond[0]]
        if sum(Value * Value for Value in Cross) < 1e-16:
            continue
        AverageNormal = [Normals[First][Axis] + Normals[Second][Axis] + Normals[Third][Axis] for Axis in range(3)]
        if sum(Cross[Axis] * AverageNormal[Axis] for Axis in range(3)) < 0:
            Second, Third = Third, Second
        Indices.extend([First, Second, Third])
    Binary = bytearray()
    Views = []
    Accessors = []
    for Values, Format, Kind, ComponentType, Target in [
        (Positions, "3f", "VEC3", 5126, 34962), (Normals, "3f", "VEC3", 5126, 34962),
        (Coordinates, "2f", "VEC2", 5126, 34962), ([(Index,) for Index in Indices], "I", "SCALAR", 5125, 34963)]:
        Offset = len(Binary)
        for Value in Values:
            Binary.extend(struct.pack("<" + Format, *Value))
        Accessor = {"bufferView": len(Views), "componentType": ComponentType, "count": len(Values), "type": Kind}
        if not Views:
            Accessor["min"] = [min(Position[Axis] for Position in Positions) for Axis in range(3)]
            Accessor["max"] = [max(Position[Axis] for Position in Positions) for Axis in range(3)]
        Views.append({"buffer": 0, "byteOffset": Offset, "byteLength": len(Binary) - Offset, "target": Target})
        Accessors.append(Accessor)
    Document = {"asset": {"version": "2.0", "generator": "Sakura Basic Shapes"},
                "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": Name, "mesh": 0}],
                "meshes": [{"name": Name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}]}],
                "buffers": [{"byteLength": len(Binary)}], "bufferViews": Views, "accessors": Accessors}
    Encoded = json.dumps(Document, separators=(",", ":")).encode("utf-8")
    Encoded += b" " * (-len(Encoded) % 4)
    Binary += b"\0" * (-len(Binary) % 4)
    Content = struct.pack("<III", 0x46546C67, 2, 28 + len(Encoded) + len(Binary))
    Content += struct.pack("<II", len(Encoded), 0x4E4F534A) + Encoded
    Content += struct.pack("<II", len(Binary), 0x004E4942) + Binary
    Fingerprint = 1469598103934665603
    for Byte in Content:
        Fingerprint = ((Fingerprint ^ Byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    AssetIdentifier = str(uuid.uuid5(uuid.NAMESPACE_URL, "sakura:/Engine/BasicShapes/" + Name))
    SubassetIdentifier = str(uuid.uuid5(uuid.NAMESPACE_URL, "sakura:/Engine/BasicShapes/" + Name + "/mesh"))
    Element = {"shape": Collision, "center": [0, 0, 0], "rotation": [0, 0, 0],
               "halfExtents": [0.5, 0.5, 0.5], "radius": 0.5, "capsuleHalfHeight": 0.5}
    if Name == "Plane":
        Element["center"] = [0, -0.005, 0]
        Element["halfExtents"] = [0.5, 0.005, 0.5]
    Metadata = {"schemaVersion": 1, "guid": AssetIdentifier, "assetType": "Model",
                "sourceFingerprint": format(Fingerprint, "016x"), "importInfo": {"generator": "Scripts/GenerateBasicShapes.py"},
                "subAssets": [{"id": SubassetIdentifier, "name": Name, "type": "StaticMesh", "selector": {"kind": "mesh", "index": 0}}],
                "loadSettings": {"staticMeshDefaults": {SubassetIdentifier: {
                    "materials": [{"slot": 0, "material": {"assetId": MaterialIdentifier, "subAssetId": None}}],
                    "collision": {"enabled": True, "elements": [Element]}}}}}
    (OutputDirectory / (Name + ".glb")).write_bytes(Content)
    (OutputDirectory / (Name + ".glb.meta")).write_text(json.dumps(Metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {Name}: {len(Positions)} vertices, {len(Indices) // 3} triangles")


def Lathe(Name, Profile, Collision):
    Positions, Normals, Coordinates, Triangles = [], [], [], []
    Segments = 32
    for Row, (Radius, Height, NormalRadius, NormalHeight) in enumerate(Profile):
        for Segment in range(Segments + 1):
            Angle = Segment * math.tau / Segments
            Positions.append((Radius * math.cos(Angle), Height, Radius * math.sin(Angle)))
            Normals.append((NormalRadius * math.cos(Angle), NormalHeight, NormalRadius * math.sin(Angle)))
            if NormalRadius == 0:
                Coordinates.append((0.5 + Radius * math.cos(Angle), 0.5 + Radius * math.sin(Angle)))
            else:
                Coordinates.append((Segment / Segments, Row / (len(Profile) - 1)))
            if Row > 0 and Segment > 0:
                Current = Row * (Segments + 1) + Segment
                Previous = Current - Segments - 1
                Triangles.extend([(Previous - 1, Previous, Current), (Previous - 1, Current, Current - 1)])
    WriteMesh(Name, Positions, Normals, Coordinates, Triangles, Collision)


def Generate():
    OutputDirectory.mkdir(parents=True, exist_ok=True)
    Positions, Normals, Coordinates, Triangles = [], [], [], []
    for Axis in range(3):
        for Sign in [-1, 1]:
            Start = len(Positions)
            for Horizontal, Vertical in [(-1, -1), (1, -1), (1, 1), (-1, 1)]:
                Position, Normal = [0, 0, 0], [0, 0, 0]
                Position[Axis], Normal[Axis] = Sign * 0.5, Sign
                Position[(Axis + 1) % 3] = Horizontal * 0.5
                Position[(Axis + 2) % 3] = Vertical * 0.5
                Positions.append(Position)
                Normals.append(Normal)
                Coordinates.append(((Horizontal + 1) * 0.5, (Vertical + 1) * 0.5))
            Triangles.extend([(Start, Start + 1, Start + 2), (Start, Start + 2, Start + 3)])
    WriteMesh("Cube", Positions, Normals, Coordinates, Triangles, 0)
    WriteMesh("Plane", [(-0.5, 0, -0.5), (0.5, 0, -0.5), (0.5, 0, 0.5), (-0.5, 0, 0.5)],
              [(0, 1, 0)] * 4, [(0, 0), (1, 0), (1, 1), (0, 1)], [(0, 1, 2), (0, 2, 3)], 0)
    SphereProfile = []
    for Ring in range(17):
        Latitude = -math.pi / 2 + math.pi * Ring / 16
        SphereProfile.append((0.5 * math.cos(Latitude), 0.5 * math.sin(Latitude), math.cos(Latitude), math.sin(Latitude)))
    Lathe("Sphere", SphereProfile, 1)
    CapsuleProfile = []
    for Hemisphere in [-1, 1]:
        for Ring in range(9):
            Latitude = math.pi * Ring / 16
            if Hemisphere < 0:
                Latitude -= math.pi / 2
            CapsuleProfile.append((0.5 * math.cos(Latitude), Hemisphere * 0.5 + 0.5 * math.sin(Latitude),
                                   math.cos(Latitude), math.sin(Latitude)))
    Lathe("Capsule", CapsuleProfile, 2)
    Lathe("Cylinder", [(0, -0.5, 0, -1), (0.5, -0.5, 0, -1), (0.5, -0.5, 1, 0),
                       (0.5, 0.5, 1, 0), (0.5, 0.5, 0, 1), (0, 0.5, 0, 1)], 3)
    Lathe("Cone", [(0, -0.5, 0, -1), (0.5, -0.5, 0, -1),
                   (0.5, -0.5, 2 / math.sqrt(5), 1 / math.sqrt(5)), (0, 0.5, 2 / math.sqrt(5), 1 / math.sqrt(5))], 3)


if __name__ == "__main__":
    Generate()
