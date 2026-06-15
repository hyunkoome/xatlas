# 01. 개요 (Overview)

## xatlas란

- 외부 의존성이 없는 **C++11 단일 파일 라이브러리**. 통합 시 [xatlas.cpp](../source/xatlas/xatlas.cpp)와 [xatlas.h](../source/xatlas/xatlas.h) 두 파일만 있으면 됩니다.
- 메시를 입력받아 **겹치지 않는 UV 좌표(아틀라스)** 를 생성합니다.
- 주 용도: **라이트맵 베이킹**, 텍스처 페인팅, AO/노멀 등 텍스처 베이킹용 UV 생성.
- 수치 솔버로 **OpenNL**이 내부에 인라인 통합되어 있습니다 ([xatlas.cpp:3928](../source/xatlas/xatlas.cpp#L3928) `namespace opennl`).

## 처리 파이프라인

| 단계 | API | 하는 일 |
|---|---|---|
| 1. 메시 추가 | `AddMesh` / `AddUvMesh` | 입력 지오메트리 등록 (비동기 처리, `AddMeshJoin`으로 대기) |
| 2. 차트 계산 | `ComputeCharts` | 메시를 여러 **차트(chart)** 로 분할(segmentation)하고 각 차트를 평면으로 펼침(파라미터화) = **UV unwrap** |
| 3. 패킹 | `PackCharts` | 차트들을 하나 이상의 아틀라스에 빈틈없이 배치 |
| 4. 출력 생성 | (내부) BuildOutputMeshes | 최종 정점/인덱스/차트 정보를 담은 출력 메시 생성 |

- `Generate`는 위 2~4단계(`ComputeCharts` + `PackCharts`)를 한 번에 수행합니다.
- `ComputeCharts`와 `PackCharts`는 옵션을 바꿔 **여러 번 재호출**할 수 있습니다 (재분할/재패킹).

진행 단계는 `ProgressCategory`로 추적합니다: `AddMesh`, `ComputeCharts`, `PackCharts`, `BuildOutputMeshes` ([xatlas.h:241](../source/xatlas/xatlas.h#L241)).

## 차트(Chart)와 아틀라스(Atlas)

- **차트**: 연결된 면(face)들의 집합으로, 하나의 아틀라스에 속하는 펼쳐진 조각. 타입은 `Planar`, `Ortho`, `LSCM`, `Piecewise`, `Invalid` ([xatlas.h:39](../source/xatlas/xatlas.h#L39)).
- **아틀라스**: 차트들이 배치된 결과. `resolution`을 0이 아닌 값으로 지정하면 여러 개의 sub-atlas가 생성될 수 있습니다.
- 출력 `Atlas` 구조체는 `image`(옵션), `meshes`, `utilization`(공간 활용률), `width`/`height`, `atlasCount`, `chartCount`, `texelsPerUnit` 등을 제공합니다 ([xatlas.h:84](../source/xatlas/xatlas.h#L84)).

## 핵심 자료구조 (라이브러리 내부)

- **희소 행렬(Sparse matrix)**: LSCM 솔버용. 동적 형식 `NLSparseMatrix`와 CRS(Compressed Row Storage) 형식 `NLCRSMatrix` ([xatlas.cpp:3974~4004](../source/xatlas/xatlas.cpp#L3974)).
- **BitImage**: 차트 패킹 시 점유 영역을 비트맵(`uint64_t` 패킹)으로 표현 ([xatlas.cpp:1333](../source/xatlas/xatlas.cpp#L1333)).
- **해시맵 / edge map**: 정점 용접(welding), 인접 정보 계산용.

## 확장 포인트

- `ParameterizeFunc`: 사용자 정의 파라미터화 함수 지정 가능 ([xatlas.h:171](../source/xatlas/xatlas.h#L171)).
- `SetAlloc` / `SetPrint` / `SetProgressCallback`: 메모리 할당자, 로그, 진행 콜백 커스터마이즈.

자세한 API는 [06-api-reference.md](06-api-reference.md) 참고.
