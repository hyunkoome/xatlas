# 06. 공개 API 레퍼런스

헤더: [source/xatlas/xatlas.h](../source/xatlas/xatlas.h) (C++), C 바인딩은 [xatlas_c.h](../source/xatlas/xatlas_c.h).

## 라이프사이클

```cpp
xatlas::Atlas *atlas = xatlas::Create();
// ... AddMesh / ComputeCharts / PackCharts 또는 Generate ...
xatlas::Destroy(atlas);
```

- `Atlas *Create()` — 빈 아틀라스 생성 ([xatlas.h:98](../source/xatlas/xatlas.h#L98)).
- `void Destroy(Atlas*)` — 해제.

## 메시 입력

### `AddMesh` — 일반 메시 (UV 새로 생성)

```cpp
AddMeshError AddMesh(Atlas*, const MeshDecl&, uint32_t meshCountHint = 0);
void AddMeshJoin(Atlas*); // 비동기 처리 대기
```

`MeshDecl` 주요 필드 ([xatlas.h:109](../source/xatlas/xatlas.h#L109)):

- `vertexPositionData` / `vertexPositionStride` / `vertexCount` (필수)
- `vertexNormalData`, `vertexUvData`(차트 힌트), `indexData`/`indexCount`/`indexFormat`(UInt16/UInt32)
- `faceIgnoreData` — 아틀라스에서 제외할 면
- `faceMaterialData` — 같은 머티리얼끼리만 한 차트
- `faceVertexCount` — n-gon 지원(없으면 삼각형 가정)
- `epsilon` — 동일 위치 판정 거리

### `AddUvMesh` — 기존 UV를 그대로 패킹

```cpp
AddMeshError AddUvMesh(Atlas*, const UvMeshDecl&);
```

`UvMeshDecl`: `vertexUvData`, `indexData`, `faceMaterialData`(겹치는 UV는 다른 머티리얼로) 등 ([xatlas.h:156](../source/xatlas/xatlas.h#L156)).

`AddMeshError`: `Success`, `Error`, `IndexOutOfRange`, `InvalidFaceVertexCount`, `InvalidIndexCount` ([xatlas.h:141](../source/xatlas/xatlas.h#L141)).

## 처리

```cpp
void ComputeCharts(Atlas*, ChartOptions = {});   // UV unwrap  → 02 문서
void PackCharts(Atlas*, PackOptions = {});        // 패킹       → 03 문서
void Generate(Atlas*, ChartOptions = {}, PackOptions = {}); // 위 둘을 한 번에
```

- `ComputeCharts` / `PackCharts`는 옵션을 바꿔 여러 번 재호출 가능.
- `ChartOptions` 상세 → [02-uv-unwrap.md](02-uv-unwrap.md), `PackOptions` 상세 → [03-packing.md](03-packing.md).

## 출력 구조체

- `Atlas` ([xatlas.h:84](../source/xatlas/xatlas.h#L84)): `image`, `meshes`, `utilization[]`, `width`, `height`, `atlasCount`, `chartCount`, `meshCount`, `texelsPerUnit`.
- `Mesh` ([xatlas.h:68](../source/xatlas/xatlas.h#L68)): `chartArray`, `indexArray`, `vertexArray`, 각 count.
- `Vertex` ([xatlas.h:59](../source/xatlas/xatlas.h#L59)): `atlasIndex`, `chartIndex`, `uv[2]`(정규화 안 됨, 아틀라스 픽셀 범위), `xref`(원본 정점 인덱스).
- `Chart` ([xatlas.h:49](../source/xatlas/xatlas.h#L49)): `faceArray`, `atlasIndex`, `faceCount`, `type`(ChartType), `material`.
- `image` 비트 플래그: `kImageChartIndexMask`, `kImageHasChartIndexBit`, `kImageIsBilinearBit`, `kImageIsPaddingBit` ([xatlas.h:78](../source/xatlas/xatlas.h#L78)).

## 콜백 / 커스터마이즈

```cpp
typedef void (*ParameterizeFunc)(const float *positions, float *texcoords,
                                 uint32_t vertexCount, const uint32_t *indices, uint32_t indexCount);

typedef bool (*ProgressFunc)(ProgressCategory, int progress, void *userData); // false 반환 시 취소
void SetProgressCallback(Atlas*, ProgressFunc = nullptr, void *userData = nullptr);

void SetAlloc(ReallocFunc, FreeFunc = nullptr); // 커스텀 메모리 할당자
void SetPrint(PrintFunc, bool verbose);         // 커스텀 로그

const char *StringForEnum(AddMeshError);
const char *StringForEnum(ProgressCategory);
```

`ProgressCategory`: `AddMesh`, `ComputeCharts`, `PackCharts`, `BuildOutputMeshes` ([xatlas.h:241](../source/xatlas/xatlas.h#L241)).

## 사용 예제

- [source/examples/example.cpp](../source/examples/example.cpp) — 기본 사용.
- [source/examples/example_uvmesh.cpp](../source/examples/example_uvmesh.cpp) — 기존 UV 패킹.
