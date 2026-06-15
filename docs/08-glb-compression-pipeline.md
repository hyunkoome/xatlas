# 08. GLB 압축 파이프라인 (merge → unwrap → pack → bake → 저장)

> 목표: **고정밀 텍스처가 있는 GLB**를 읽어 → 가까운 점 병합 + 폴리곤 감소로 **메시 압축** → **UV unwrap → 패킹** → 원본 디테일을 **노멀/텍스처 베이킹**으로 전사 → **압축된 GLB**로 저장.

핵심 전제부터: **xatlas는 이 워크플로우 중 UV unwrap + 패킹(3·4단계)만** 담당합니다. GLB 입출력, 메시 압축, 베이킹은 다른 오픈소스를 결합해야 합니다.

---

## 단계별 구성 요약

| 단계 | 할 일 | 추천 C++ 오픈소스 | 라이선스 |
|---|---|---|---|
| 1. GLB 로드 | 메시/텍스처/머티리얼 읽기 | **cgltf** (뷰어가 이미 사용) / tinygltf | MIT |
| 2. 메시 압축 | 가까운 점 weld + decimation | **meshoptimizer** | MIT |
| 3. UV unwrap | 차트 분할 + 파라미터화 | **xatlas** (본 저장소) | MIT |
| 4. 패킹 | 아틀라스 배치 | **xatlas** | MIT |
| 5. 탄젠트 생성 | 노멀맵용 tangent space | **MikkTSpace** | zlib |
| 6. 베이킹 | HIGH→LOW 노멀/텍스처/AO 전사 | **Embree**(CPU) / **OptiX**(GPU) + 직접 구현 | Apache-2.0 |
| 7. 텍스처 후처리 | 디노이즈 / dilation / 리사이즈 / 저장 | **OpenImageDenoise**, **stb_image** | Apache-2.0 / MIT |
| 8. 텍스처 압축 | KTX2 / Basis 슈퍼컴프레션 | **basis_universal** / **KTX-Software** | Apache-2.0 |
| 9. GLB 저장 | 압축 지오메트리+텍스처 쓰기 | **cgltf**(write) + **meshoptimizer**(EXT_meshopt) / **Draco** | MIT / Apache-2.0 |

> 참고 도구: meshoptimizer의 **gltfpack** CLI는 1·2·8·9단계(로드→simplify→재정렬→meshopt/KTX2 압축→저장)를 이미 수행합니다. unwrap·bake는 없지만 **참고 구현**으로 매우 유용합니다.

---

## 데이터 흐름

```
원본 GLB (고정밀 메시 + 고해상 텍스처)
   │   ※ 반드시 2벌 유지: HIGH(원본 그대로) / LOW(압축 대상)
   ▼
[2] meshoptimizer → LOW 생성
     - meshopt_generateVertexRemap : 가까운/동일 정점 weld
     - meshopt_simplify            : 폴리곤 감소(목표 비율/오차)
   ▼
[3][4] xatlas → LOW unwrap + pack → 새 UV + 아틀라스
   ▼
[6] 베이킹: 아틀라스 각 텍셀에서
     LOW 표면점 → 노멀 방향 ray 발사 → HIGH 표면 적중
        ├─ HIGH 노멀  → LOW 탄젠트공간 변환 → 노멀맵
        ├─ HIGH 텍스처(HIGH UV로 샘플) → 새 아틀라스 → 베이스컬러맵
        └─ (옵션) AO / curvature / metallic-roughness 등
   ▼
[7] 디노이즈 + dilation(가장자리 번짐 방지) + [8] KTX2 압축
   ▼
[9] cgltf → 압축 GLB 저장 (+ meshopt/Draco 지오메트리 압축)
```

---

## 핵심 라이브러리 상세

**meshoptimizer** (https://github.com/zeux/meshoptimizer) — 이 파이프라인의 **2번 단계 정답**입니다.
- `meshopt_generateVertexRemap`(가까운 점 병합/중복 제거), `meshopt_simplify` / `meshopt_simplifySloppy`(decimation), 버텍스/인덱스 캐시 최적화, 그리고 **glTF용 지오메트리 압축(EXT_meshopt_compression)** 까지 한 번에 제공합니다.
- 같은 저장소의 **gltfpack** CLI는 "glb 로드 → simplify → 재정렬 → meshopt/KTX2 압축 → glb 저장"을 이미 다 합니다(단, unwrap·bake는 없음). **2·8·9단계의 동작 참고 구현**으로 매우 유용합니다.

**xatlas** (3·4단계) — `AddMesh`로 LOW 메시를 넣고 `Generate` 호출. 출력 `Vertex`의 **`xref`(원본 정점 인덱스)** 와 `uv`, `atlasIndex`가 베이킹에서 텍셀↔표면 대응을 만들 때 핵심입니다 ([06-api-reference.md](06-api-reference.md) 참고).

**Embree / OptiX** (6단계) — 베이킹은 결국 **"LOW 표면 → HIGH 표면" ray casting**입니다.
- 본 저장소 뷰어의 [source/viewer/viewer_bake.cpp](../source/viewer/viewer_bake.cpp)가 Embree로 **텍셀 래스터화 + ray trace + dilation + OIDN 디노이즈**를 이미 구현해 둔, 거의 그대로 참고할 수 있는 코드입니다 ([04-baker.md](04-baker.md)).
- 단, 뷰어 베이크는 "라이트맵(GI)" 용이라 **노멀/텍스처 전사(transfer) 로직은 직접 추가**해야 합니다. 적중점에서 HIGH의 노멀/원본 텍스처를 샘플하는 부분만 바꾸면 구조는 동일합니다.
- 앞서 관심 보인 **CUDA 가속**과 직결: 베이커를 OptiX(RTX RT코어)로 올리면 큰 효과 ([07-gpu-cuda-acceleration.md](07-gpu-cuda-acceleration.md) §3).

**MikkTSpace** — 노멀맵을 구우면 **반드시 동일한 탄젠트 기준**이 필요합니다. 런타임 엔진과 베이커가 같은 탄젠트를 써야 노멀맵이 정확합니다. 업계 표준이라 이걸 쓰는 게 안전합니다.

**basis_universal / KTX-Software** — 최종 GLB의 텍스처를 **KTX2(UASTC/ETC1S)** 로 압축(`EXT_texture_basisu`). "압축된 glb"의 텍스처 용량을 크게 줄이는 부분입니다.

---

## 단계별 코드 스케치

> 아래는 실제 동작 코드가 아니라 **호출 흐름 골격**입니다. 각 라이브러리의 최신 시그니처는 공식 헤더를 확인하세요.

### 1. GLB 로드 (cgltf)

```cpp
#include "cgltf.h"
cgltf_options opt = {};
cgltf_data* data = nullptr;
cgltf_parse_file(&opt, "input.glb", &data);
cgltf_load_buffers(&opt, data, "input.glb");
// data->meshes[].primitives[] 에서 POSITION/NORMAL/TEXCOORD_0 accessor,
// data->images / data->textures / data->materials 추출
```

### 2. 메시 압축 (meshoptimizer)

```cpp
#include "meshoptimizer.h"

// (a) 가까운/동일 정점 병합 = weld
std::vector<unsigned int> remap(indexCount);
size_t vertexCount = meshopt_generateVertexRemap(
    remap.data(), indices, indexCount, vertices, srcVertexCount, sizeof(Vertex));
meshopt_remapIndexBuffer(indices, indices, indexCount, remap.data());
meshopt_remapVertexBuffer(vertices, vertices, srcVertexCount, sizeof(Vertex), remap.data());

// (b) 폴리곤 감소 = decimation (목표 인덱스 수 target, 허용 오차 targetError)
std::vector<unsigned int> lod(indexCount);
float resultError = 0.f;
size_t lodCount = meshopt_simplify(
    lod.data(), indices, indexCount, &vertices[0].px, vertexCount, sizeof(Vertex),
    /*target*/ size_t(indexCount * 0.25f), /*targetError*/ 0.01f, /*options*/ 0, &resultError);
// → LOW 메시 = (vertices, lod[0..lodCount])
```

> 정밀 weld가 더 필요하면 위치 기준 공간 해시로 epsilon 병합을 직접 추가. xatlas도 입력에서 `MeshDecl::epsilon`으로 colocal 정점을 처리합니다.

### 3·4. UV unwrap + 패킹 (xatlas)

```cpp
#include "xatlas.h"
xatlas::Atlas* atlas = xatlas::Create();

xatlas::MeshDecl decl;
decl.vertexCount        = lowVertexCount;
decl.vertexPositionData = lowPositions;  decl.vertexPositionStride = sizeof(float)*3;
decl.vertexNormalData   = lowNormals;    decl.vertexNormalStride   = sizeof(float)*3;
decl.indexCount         = lowIndexCount;
decl.indexData          = lowIndices;    decl.indexFormat = xatlas::IndexFormat::UInt32;
xatlas::AddMesh(atlas, decl);

xatlas::ChartOptions co;                 // 필요 시 maxIterations 등 조정 (02 문서)
xatlas::PackOptions  po;
po.resolution  = 2048;                    // 아틀라스 해상도
po.padding     = 4;                       // 번짐 방지 패딩
po.bilinear    = true;
po.createImage = false;                   // 베이킹은 직접 하므로 불필요
xatlas::Generate(atlas, co, po);

// 결과: atlas->meshes[i].vertexArray[].uv (아틀라스 픽셀 좌표),
//       .xref (원본 LOW 정점 인덱스), .atlasIndex
xatlas::Destroy(atlas);
```

자세한 옵션: [02-uv-unwrap.md](02-uv-unwrap.md), [03-packing.md](03-packing.md), [06-api-reference.md](06-api-reference.md).

### 5. 탄젠트 생성 (MikkTSpace)

```cpp
#include "mikktspace.h"
// SMikkTSpaceInterface 콜백(getNumFaces/getPosition/getNormal/getTexCoord/setTSpaceBasic) 구현
// → LOW 메시 + 새 UV 기준으로 정점 탄젠트 계산.
// ⚠️ 베이커와 런타임 엔진이 "같은" 탄젠트 규약을 써야 노멀맵이 정확함.
```

### 6. 베이킹 (Embree + 직접 구현)

```cpp
// 본 저장소 source/viewer/viewer_bake.cpp 가 Embree 기반
// "텍셀 래스터화 → ray trace → dilation → OIDN 디노이즈" 를 이미 구현.
// 라이트맵(GI) 대신 아래 "전사(transfer)" 로직으로 교체하면 됨:

// 1) HIGH 메시로 Embree BVH(RTCScene) 구성
// 2) 새 아틀라스의 각 텍셀 → 속한 LOW 삼각형/표면점 P, 보간 노멀 N 계산(래스터화)
// 3) P에서 ±N 방향으로 ray 발사(cage/offset), HIGH 표면 적중점 H 탐색
// 4) 적중 삼각형의 무게중심으로 HIGH 속성 보간:
//      - 노멀맵      : HIGH 노멀을 LOW 탄젠트공간(TBN)으로 변환 후 [0,1] 인코딩
//      - 베이스컬러   : HIGH 원본 텍스처를 HIGH UV로 샘플
//      - AO          : 반구 샘플 occlusion (viewer_bake.cpp 그대로 활용)
// 5) 결과를 아틀라스 텍셀에 기록
```

- CPU: **Embree**(`rtcInitIntersectContext`, `rtcIntersect1`). GPU: **OptiX**(RTX RT코어) — 앞서 논의한 CUDA 가속 후보 ([07-gpu-cuda-acceleration.md](07-gpu-cuda-acceleration.md) §3).
- 노멀/텍스처 전사용 turnkey C++ 라이브러리는 사실상 없어, **Embree로 ray cast 골격을 깔고 샘플링 로직만 직접** 작성하는 것이 현실적입니다 (xNormal·Blender가 하는 일을 코드로).

### 7. 후처리 (OpenImageDenoise + stb)

```cpp
// dilation: 차트 경계 밖으로 가장자리 색 번지기 (viewer_bake.cpp 의 bakeFilter 참고)
// AO/GI 노이즈: OIDN RTLightmap/RT 필터로 디노이즈
// 저장/리사이즈: stb_image_write (PNG), stb_image_resize2 (mip/축소)
```

### 8. 텍스처 압축 (basis_universal / KTX-Software)

```cpp
// 베이크된 PNG/RGBA → KTX2(UASTC 또는 ETC1S) 인코딩
// glTF 확장: EXT_texture_basisu 로 참조. 용량 대폭 절감.
```

### 9. 압축 GLB 저장 (cgltf + meshoptimizer/Draco)

```cpp
// (a) 지오메트리: meshopt_encodeVertexBuffer/encodeIndexBuffer → EXT_meshopt_compression
//     또는 Draco(KHR_draco_mesh_compression) 로 압축
// (b) cgltf 로 노드/메시/머티리얼/이미지(KTX2) 구성 후 .glb 로 write
//     (cgltf_write_file / 직접 직렬화)
```

---

## 주의할 점 (실패하기 쉬운 부분)

1. **원본 2벌 유지** — 베이킹은 HIGH→LOW 전사이므로 simplify 전 원본 메시·텍스처가 그대로 있어야 함. 한 메시에 in-place로 하면 전사할 디테일 소스가 사라짐.
2. **순서 고정** — weld/simplify(2) → unwrap(3). 메시를 줄인 *뒤* 새 UV 생성. 기존 UV는 버려짐.
3. **dilation/패딩 필수** — `PackOptions.padding`/`bilinear` + 베이크 후 dilation 둘 다. 안 하면 차트 경계 번짐(bleeding).
4. **탄젠트 일관성** — 노멀맵은 베이커·런타임이 동일 탄젠트(MikkTSpace) 규약일 때만 정확.
5. **cage/offset 튜닝** — 베이크 ray 발사 거리(near/far, cage)가 너무 짧으면 미적중, 너무 길면 오적중. HIGH/LOW 간 거리에 맞춰 조정.
6. **xatlas 범위 명확화** — xatlas는 unwrap/pack만. GLB I/O·압축·베이킹은 직접 결합 대상.

---

## 라이브러리 출처

| 라이브러리 | 저장소 |
|---|---|
| cgltf | github.com/jkuhlmann/cgltf |
| tinygltf | github.com/syoyo/tinygltf |
| meshoptimizer (+ gltfpack) | github.com/zeux/meshoptimizer |
| xatlas | github.com/jpcy/xatlas (본 저장소 fork) |
| MikkTSpace | github.com/mmikk/MikkTSpace |
| Embree | github.com/embree/embree |
| OptiX | developer.nvidia.com/optix |
| OpenImageDenoise | github.com/OpenImageDenoise/oidn |
| stb (image / write / resize) | github.com/nothings/stb |
| basis_universal | github.com/BinomialLLC/basis_universal |
| KTX-Software | github.com/KhronosGroup/KTX-Software |
| Draco | github.com/google/draco |
