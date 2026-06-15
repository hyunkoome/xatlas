#!/usr/bin/env python3
"""의존성 없이 테스트용 UV-sphere GLB를 생성한다 (단일 머티리얼, POSITION+NORMAL+인덱스).

사용:  python3 scripts/gen_test_glb.py out.glb [segments] [rings]
실데이터 도착 전 파이프라인 스모크 테스트용.
"""
import json, struct, sys, math


def uv_sphere(segments=64, rings=32, radius=1.0):
    positions, normals = [], []
    for r in range(rings + 1):
        v = r / rings
        theta = v * math.pi
        for s in range(segments + 1):
            u = s / segments
            phi = u * 2.0 * math.pi
            x = math.sin(theta) * math.cos(phi)
            y = math.cos(theta)
            z = math.sin(theta) * math.sin(phi)
            positions.append((x * radius, y * radius, z * radius))
            normals.append((x, y, z))
    indices = []
    stride = segments + 1
    for r in range(rings):
        for s in range(segments):
            a = r * stride + s
            b = a + stride
            indices.extend([a, b, a + 1, a + 1, b, b + 1])
    return positions, normals, indices


def build_glb(path, segments, rings):
    positions, normals, indices = uv_sphere(segments, rings)

    pos_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    nrm_bytes = b"".join(struct.pack("<3f", *n) for n in normals)
    idx_bytes = b"".join(struct.pack("<I", i) for i in indices)

    def pad4(b, fill=b"\x00"):
        return b + fill * ((4 - len(b) % 4) % 4)

    pos_off = 0
    nrm_off = len(pos_bytes)
    idx_off = nrm_off + len(nrm_bytes)
    bin_blob = pad4(pos_bytes + nrm_bytes + idx_bytes)

    pmin = [min(p[i] for p in positions) for i in range(3)]
    pmax = [max(p[i] for p in positions) for i in range(3)]

    gltf = {
        "asset": {"version": "2.0", "generator": "gen_test_glb.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1},
            "indices": 2, "material": 0, "mode": 4}]}],
        "materials": [{"pbrMetallicRoughness": {
            "baseColorFactor": [0.8, 0.3, 0.2, 1.0],
            "metallicFactor": 0.0, "roughnessFactor": 0.7}}],
        "buffers": [{"byteLength": len(bin_blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": pos_off, "byteLength": len(pos_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": nrm_off, "byteLength": len(nrm_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": idx_off, "byteLength": len(idx_bytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions),
             "type": "VEC3", "min": pmin, "max": pmax},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
        ],
    }

    json_blob = pad4(json.dumps(gltf, separators=(",", ":")).encode("utf-8"), b" ")

    def chunk(data, ctype):
        return struct.pack("<I", len(data)) + ctype + data

    body = chunk(json_blob, b"JSON") + chunk(bin_blob, b"BIN\x00")
    header = struct.pack("<III", 0x46546C67, 2, 12 + len(body))
    with open(path, "wb") as f:
        f.write(header + body)
    print(f"wrote {path}: {len(positions)} verts, {len(indices)//3} tris")


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "test_sphere.glb"
    seg = int(sys.argv[2]) if len(sys.argv) > 2 else 96
    rng = int(sys.argv[3]) if len(sys.argv) > 3 else 48
    build_glb(out, seg, rng)
