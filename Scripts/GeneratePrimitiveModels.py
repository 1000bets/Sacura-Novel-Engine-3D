"""Regenerate built-in primitive and starter character GLBs with stable metadata."""

import json
import math
import struct
import uuid
from pathlib import Path


CONTENT = Path(__file__).resolve().parents[1] / "Root" / "Engine" / "Content" / "Models"
NAMESPACE = uuid.UUID("53c92e7e-9e94-4fc4-a097-9831f4e108ec")
SEGMENTS = 24


def sphere():
    vertices, indices = [], []
    rings = 12
    for ring in range(rings + 1):
        theta = math.pi * ring / rings
        for segment in range(SEGMENTS + 1):
            phi = 2 * math.pi * segment / SEGMENTS
            normal = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
            vertices.append((tuple(0.5 * value for value in normal), normal, (segment / SEGMENTS, ring / rings)))
    for ring in range(rings):
        for segment in range(SEGMENTS):
            a = ring * (SEGMENTS + 1) + segment
            b = a + SEGMENTS + 1
            if ring > 0:
                indices.extend((a, a + 1, b))
            if ring < rings - 1:
                indices.extend((a + 1, b + 1, b))
    return vertices, indices


def cylinder():
    vertices, indices = [], []
    for segment in range(SEGMENTS + 1):
        angle = 2 * math.pi * segment / SEGMENTS
        x, z = math.cos(angle), math.sin(angle)
        for y, v in ((-0.5, 1), (0.5, 0)):
            vertices.append(((0.5 * x, y, 0.5 * z), (x, 0, z), (segment / SEGMENTS, v)))
    for segment in range(SEGMENTS):
        bottom = 2 * segment
        indices.extend((bottom, bottom + 1, bottom + 2, bottom + 1, bottom + 3, bottom + 2))
    for y, normal in ((0.5, (0, 1, 0)), (-0.5, (0, -1, 0))):
        center = len(vertices)
        vertices.append(((0, y, 0), normal, (0.5, 0.5)))
        for segment in range(SEGMENTS + 1):
            angle = 2 * math.pi * segment / SEGMENTS
            x, z = math.cos(angle), math.sin(angle)
            vertices.append(((0.5 * x, y, 0.5 * z), normal, ((x + 1) / 2, (z + 1) / 2)))
        for segment in range(SEGMENTS):
            first = center + 1 + segment
            second = first + 1
            indices.extend((center, second, first) if y > 0 else (center, first, second))
    return vertices, indices


def starter_character():
    vertices, indices = [], []

    def cross(a, b):
        return (a[1] * b[2] - a[2] * b[1],
                a[2] * b[0] - a[0] * b[2],
                a[0] * b[1] - a[1] * b[0])

    def box(center, half):
        for normal, tangent in (((1, 0, 0), (0, 0, 1)), ((-1, 0, 0), (0, 0, 1)),
                                ((0, 1, 0), (1, 0, 0)), ((0, -1, 0), (1, 0, 0)),
                                ((0, 0, 1), (1, 0, 0)), ((0, 0, -1), (1, 0, 0))):
            bitangent = cross(normal, tangent)
            start = len(vertices)
            for u, v in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                position = tuple(center[i] + normal[i] * half[i]
                                 + tangent[i] * half[i] * u + bitangent[i] * half[i] * v
                                 for i in range(3))
                vertices.append((position, normal, ((u + 1) / 2, (v + 1) / 2)))
            indices.extend((start, start + 1, start + 2, start, start + 2, start + 3))

    box((0, 1.56, 0), (.22, .22, .19))
    box((0, 1.05, 0), (.28, .40, .15))
    for side in (-1, 1):
        box((side * .39, 1.06, 0), (.10, .35, .12))
        box((side * .14, .37, 0), (.11, .37, .13))
    return vertices, indices


def write_model(name, geometry):
    vertices, indices = geometry
    payload = bytearray()
    views, accessors = [], []
    for values, fmt, component, kind in (
        ([v[0] for v in vertices], "<fff", 5126, "VEC3"),
        ([v[1] for v in vertices], "<fff", 5126, "VEC3"),
        ([v[2] for v in vertices], "<ff", 5126, "VEC2"),
        ([(index,) for index in indices], "<I", 5125, "SCALAR"),
    ):
        offset = len(payload)
        for value in values:
            payload.extend(struct.pack(fmt, *value))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload) - offset})
        accessors.append({"bufferView": len(views) - 1, "componentType": component, "count": len(values), "type": kind})
    accessors[0]["min"] = [min(vertex[0][axis] for vertex in vertices) for axis in range(3)]
    accessors[0]["max"] = [max(vertex[0][axis] for vertex in vertices) for axis in range(3)]
    document = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": len(payload)}],
                "bufferViews": views, "accessors": accessors,
                "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}]}],
                "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    payload.extend(b"\0" * (-len(payload) % 4))
    binary = (struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(encoded) + 8 + len(payload))
              + struct.pack("<I4s", len(encoded), b"JSON") + encoded
              + struct.pack("<I4s", len(payload), b"BIN\0") + payload)
    asset = CONTENT / f"{name}.glb"
    asset.write_bytes(binary)
    fingerprint = 1469598103934665603
    for byte in binary:
        fingerprint = ((fingerprint ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    metadata = {"schemaVersion": 1, "guid": str(uuid.uuid5(NAMESPACE, name)), "assetType": "Model",
                "sourceFingerprint": f"{fingerprint:016x}", "loadSettings": {}, "importInfo": {},
                "subAssets": [{"id": str(uuid.uuid5(NAMESPACE, name + ":mesh")), "type": "StaticMesh",
                               "name": name, "selector": {"kind": "mesh", "index": 0}}]}
    (CONTENT / f"{name}.glb.meta").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    write_model("PrimitiveSphere", sphere())
    write_model("PrimitiveCylinder", cylinder())
    write_model("StarterCharacter", starter_character())
