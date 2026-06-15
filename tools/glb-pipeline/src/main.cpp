// glb-pipeline (skeleton)
//
// Pipeline scope (v1): load GLB -> weld + simplify (meshoptimizer)
//                      -> UV unwrap + pack (xatlas) -> save GLB.
// Baking (normal + base color transfer via Embree) is added in a later step.
//
// This skeleton wires every dependency end-to-end on the FIRST triangulated
// primitive and prints stats, so we can confirm the toolchain builds/runs on
// both Ubuntu and Windows before fleshing out geometry rebuild + baking.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "tiny_gltf.h"
#include "meshoptimizer.h"
#include "xatlas.h"

namespace {

struct Mesh {
    std::vector<float>    positions; // 3 per vertex
    std::vector<uint32_t> indices;
    size_t vertexCount() const { return positions.size() / 3; }
    size_t triangleCount() const { return indices.size() / 3; }
};

// Read the first triangulated primitive's POSITION + indices into `out`.
bool extractFirstPrimitive(const tinygltf::Model& model, Mesh& out, std::string& err) {
    for (const auto& mesh : model.meshes) {
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
            auto itPos = prim.attributes.find("POSITION");
            if (itPos == prim.attributes.end()) continue;

            const tinygltf::Accessor&   posAcc = model.accessors[itPos->second];
            const tinygltf::BufferView&  posBv = model.bufferViews[posAcc.bufferView];
            const tinygltf::Buffer&     posBuf = model.buffers[posBv.buffer];
            const size_t posStride = posAcc.ByteStride(posBv);
            const unsigned char* posBase =
                posBuf.data.data() + posBv.byteOffset + posAcc.byteOffset;

            out.positions.resize(posAcc.count * 3);
            for (size_t i = 0; i < posAcc.count; ++i) {
                const float* p = reinterpret_cast<const float*>(posBase + i * posStride);
                out.positions[i * 3 + 0] = p[0];
                out.positions[i * 3 + 1] = p[1];
                out.positions[i * 3 + 2] = p[2];
            }

            if (prim.indices < 0) {
                // Unindexed: synthesize a trivial index buffer.
                out.indices.resize(posAcc.count);
                for (uint32_t i = 0; i < posAcc.count; ++i) out.indices[i] = i;
            } else {
                const tinygltf::Accessor&  idxAcc = model.accessors[prim.indices];
                const tinygltf::BufferView& idxBv = model.bufferViews[idxAcc.bufferView];
                const tinygltf::Buffer&    idxBuf = model.buffers[idxBv.buffer];
                const unsigned char* idxBase =
                    idxBuf.data.data() + idxBv.byteOffset + idxAcc.byteOffset;
                out.indices.resize(idxAcc.count);
                for (size_t i = 0; i < idxAcc.count; ++i) {
                    switch (idxAcc.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        out.indices[i] = reinterpret_cast<const uint32_t*>(idxBase)[i]; break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        out.indices[i] = reinterpret_cast<const uint16_t*>(idxBase)[i]; break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        out.indices[i] = reinterpret_cast<const uint8_t*>(idxBase)[i]; break;
                    default:
                        err = "unsupported index component type"; return false;
                    }
                }
            }
            return true;
        }
    }
    err = "no triangulated primitive with POSITION found";
    return false;
}

// Weld colocated vertices, then simplify to `ratio` of the original triangles.
void simplifyMesh(Mesh& m, float ratio) {
    const size_t srcVerts = m.vertexCount();
    const size_t idxCount = m.indices.size();

    // (a) weld
    std::vector<unsigned int> remap(srcVerts);
    const size_t newVerts = meshopt_generateVertexRemap(
        remap.data(), m.indices.data(), idxCount,
        m.positions.data(), srcVerts, sizeof(float) * 3);

    std::vector<uint32_t> weldedIdx(idxCount);
    meshopt_remapIndexBuffer(weldedIdx.data(), m.indices.data(), idxCount, remap.data());
    std::vector<float> weldedPos(newVerts * 3);
    meshopt_remapVertexBuffer(weldedPos.data(), m.positions.data(), srcVerts,
                              sizeof(float) * 3, remap.data());

    // (b) simplify
    const size_t target = size_t(double(idxCount) * ratio / 3.0) * 3;
    std::vector<uint32_t> lod(idxCount);
    float resultError = 0.f;
    const size_t lodCount = meshopt_simplify(
        lod.data(), weldedIdx.data(), idxCount,
        weldedPos.data(), newVerts, sizeof(float) * 3,
        target, /*target_error*/ 1e-2f, /*options*/ 0, &resultError);
    lod.resize(lodCount);

    printf("[simplify] verts %zu -> %zu (welded), tris %zu -> %zu, error %.4f\n",
           srcVerts, newVerts, idxCount / 3, lodCount / 3, resultError);

    m.positions = std::move(weldedPos);
    m.indices   = std::move(lod);
}

// UV unwrap + pack with xatlas. Returns true on success.
bool unwrap(const Mesh& m, uint32_t resolution, uint32_t padding) {
    xatlas::Atlas* atlas = xatlas::Create();

    xatlas::MeshDecl decl;
    decl.vertexCount          = (uint32_t)m.vertexCount();
    decl.vertexPositionData   = m.positions.data();
    decl.vertexPositionStride = sizeof(float) * 3;
    decl.indexCount           = (uint32_t)m.indices.size();
    decl.indexData            = m.indices.data();
    decl.indexFormat          = xatlas::IndexFormat::UInt32;

    const xatlas::AddMeshError e = xatlas::AddMesh(atlas, decl);
    if (e != xatlas::AddMeshError::Success) {
        printf("[xatlas] AddMesh failed: %s\n", xatlas::StringForEnum(e));
        xatlas::Destroy(atlas);
        return false;
    }

    xatlas::ChartOptions co;
    xatlas::PackOptions  po;
    po.resolution  = resolution;
    po.padding     = padding;
    po.bilinear    = true;
    po.createImage = false;
    xatlas::Generate(atlas, co, po);

    printf("[xatlas] atlas %ux%u, %u atlases, %u charts\n",
           atlas->width, atlas->height, atlas->atlasCount, atlas->chartCount);
    if (atlas->atlasCount > 0)
        printf("[xatlas] utilization %.1f%%\n", atlas->utilization[0] * 100.0f);

    xatlas::Destroy(atlas);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: glb-pipeline <input.glb> <output.glb> [--ratio 0.25]\n");
        return 1;
    }
    const std::string inPath  = argv[1];
    const std::string outPath = argv[2];
    float ratio = 0.25f;
    for (int i = 3; i < argc - 1; ++i)
        if (std::string(argv[i]) == "--ratio") ratio = (float)atof(argv[i + 1]);

    // 1. Load GLB
    tinygltf::Model model;
    tinygltf::TinyGLTF io;
    std::string err, warn;
    if (!io.LoadBinaryFromFile(&model, &err, &warn, inPath)) {
        printf("load failed: %s\n", err.c_str());
        return 1;
    }
    if (!warn.empty()) printf("warn: %s\n", warn.c_str());
    printf("[load] %s : %zu meshes, %zu materials, %zu images\n",
           inPath.c_str(), model.meshes.size(), model.materials.size(), model.images.size());

    // 2. Extract + simplify + unwrap (first primitive only, for now)
    Mesh mesh;
    if (!extractFirstPrimitive(model, mesh, err)) {
        printf("extract failed: %s\n", err.c_str());
        return 1;
    }
    printf("[extract] verts %zu, tris %zu\n", mesh.vertexCount(), mesh.triangleCount());

    simplifyMesh(mesh, ratio);
    if (!unwrap(mesh, /*resolution*/ 2048, /*padding*/ 4)) return 1;

    // 3. Write GLB (round-trip for now; geometry rebuild + baking come next)
    if (!io.WriteGltfSceneToFile(&model, outPath,
                                 /*embedImages*/ true, /*embedBuffers*/ true,
                                 /*prettyPrint*/ false, /*writeBinary*/ true)) {
        printf("write failed\n");
        return 1;
    }
    printf("[write] %s\n", outPath.c_str());
    printf("done. (NOTE: skeleton — output geometry not yet rebuilt/baked)\n");
    return 0;
}
