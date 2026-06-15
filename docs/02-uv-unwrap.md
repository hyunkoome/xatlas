# 02. UV Unwrap (차트 분할 + 파라미터화)

`ComputeCharts` 단계가 UV unwrap에 해당합니다. 두 가지 일을 합니다.

1. **차트 분할(Segmentation)**: 메시의 면들을 연결성·노멀·형태 비용을 기준으로 여러 차트로 나눔.
2. **파라미터화(Parameterization)**: 각 차트를 2D 평면으로 펼쳐 UV 좌표를 부여 (LSCM 등).

코드: `ComputeCharts` ([xatlas.h:195](../source/xatlas/xatlas.h#L195)), 분할 핵심 `growCharts` ([xatlas.cpp:5415](../source/xatlas/xatlas.cpp#L5415)), 비용 계산 `computeCost` ([xatlas.cpp:5844](../source/xatlas/xatlas.cpp#L5844)).

## ChartOptions — 분할 제어 옵션

`xatlas::ChartOptions` ([xatlas.h:173](../source/xatlas/xatlas.h#L173)):

| 필드 | 기본값 | 설명 |
|---|---|---|
| `paramFunc` | `nullptr` | 사용자 정의 파라미터화 함수. 미지정 시 내장 LSCM 사용 |
| `maxChartArea` | `0.0` (무제한) | 차트 최대 면적 제한 |
| `maxBoundaryLength` | `0.0` (무제한) | 차트 경계선 최대 길이 제한 |
| `normalDeviationWeight` | `2.0` | 면과 차트 평균 노멀 사이 각도 비용 가중치 |
| `roundnessWeight` | `0.01` | 차트가 둥글게 유지되도록 하는 가중치 |
| `straightnessWeight` | `6.0` | 경계가 직선이 되도록 하는 가중치 |
| `normalSeamWeight` | `4.0` | 노멀 seam 비용. `> 1000`이면 노멀 seam을 완전히 존중 |
| `textureSeamWeight` | `0.5` | 텍스처 seam 비용 가중치 |
| `maxCost` | `2.0` | (모든 비용 × 가중치) 합이 이 값을 넘으면 차트를 더 키우지 않음. **낮을수록 차트 수 증가** |
| `maxIterations` | `1` | 차트 성장/시딩 반복 횟수. **높을수록 품질↑** |
| `useInputMeshUvs` | `false` | 입력 메시의 UV를 차트 힌트로 사용 |
| `fixWinding` | `false` | UV winding 일관성 강제 |

> 동작 원리: 우선순위 큐 기반 **그리디 영역 성장(region growing)**. 시드 면에서 시작해 비용이 가장 낮은 이웃 면을 차트에 흡수하며, 누적 비용이 `maxCost`를 넘으면 중단합니다.

## 파라미터화 방식

- **Planar / Ortho**: 평면에 가까운 차트는 단순 직교 투영 ([xatlas.cpp:7276](../source/xatlas/xatlas.cpp#L7276)).
- **LSCM** (Least Squares Conformal Maps): 일반 곡면 차트의 기본 방식. OpenNL의 **전처리 Conjugate Gradient** 솔버로 희소 선형 시스템을 풉니다 (`computeLeastSquaresConformalMap` [xatlas.cpp:6482](../source/xatlas/xatlas.cpp#L6482)). → CUDA 가속 1순위 후보, [07 문서](07-gpu-cuda-acceleration.md) 참고.
- **Piecewise**: 조각별 평면 근사 ([xatlas.cpp:6563](../source/xatlas/xatlas.cpp#L6563)).

## 뷰어(GUI)에서의 UV Unwrap 기능

GUI 코드: [source/viewer/viewer_atlas.cpp](../source/viewer/viewer_atlas.cpp). "Chart options" 패널 ([viewer_atlas.cpp:951](../source/viewer/viewer_atlas.cpp#L951)).

### Chart options 컨트롤

| GUI 라벨 | 매핑 필드 | 위치 |
|---|---|---|
| `Use input mesh UVs` (체크박스) | `useInputMeshUvs` | [:954](../source/viewer/viewer_atlas.cpp#L954) |
| `Normal deviation weight` | `normalDeviationWeight` | [:962](../source/viewer/viewer_atlas.cpp#L962) |
| `Roundness weight` | `roundnessWeight` | :963 |
| `Straightness weight` | `straightnessWeight` | :964 |
| `Normal seam weight` | `normalSeamWeight` | :965 |
| `Texture seam weight` | `textureSeamWeight` | :966 |
| `Max cost` | `maxCost` | :967 |
| `Max iterations` | `maxIterations` | :968 |
| `Max chart area` | `maxChartArea` | :969 |
| `Max boundary length` | `maxBoundaryLength` | :970 |
| `Reset to default` (버튼) | `ChartOptions()` 기본값 복원 | :983 |

### 파라미터화 방식 선택 (라디오 버튼)

입력 UV 메시 모드가 아닐 때만 노출 ([viewer_atlas.cpp:972](../source/viewer/viewer_atlas.cpp#L972)):

- `LSCM` (기본)
- `libigl Harmonic`
- `libigl LSCM`
- `libigl ARAP`

### 생성/제어

- `Generate` 버튼: 백그라운드 스레드로 아틀라스 생성 ([:936](../source/viewer/viewer_atlas.cpp#L936)).
- `Pack input mesh UVs` 체크박스: 켜면 `AddUvMesh`(기존 UV 사용), 끄면 `AddMesh`(새 UV 생성) ([:940](../source/viewer/viewer_atlas.cpp#L940)).
- 진행 바 + 스피너 + `Cancel` 버튼 (취소는 진행 콜백 false 반환으로 처리).

### 시각화

- 차트별 랜덤 색상, 선택 차트 경계선(흰색) 표시.
- 오버레이 모드(뷰어 View 메뉴): `Chart`(차트 시각화), `Stretch`(UV 왜곡 시각화), `Mesh`.
- 결과 통계: 아틀라스 크기, 차트 수, 정점/삼각형 수, texels per unit, 공간 활용률(utilization).

관련: 패킹은 [03-packing.md](03-packing.md), 전체 GUI는 [05-viewer-gui.md](05-viewer-gui.md).
