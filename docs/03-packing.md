# 03. 패킹 (Packing)

`PackCharts` 단계는 분할·파라미터화된 차트들을 하나 이상의 아틀라스에 **겹치지 않게 배치**합니다.

코드: `PackCharts` ([xatlas.h:235](../source/xatlas/xatlas.h#L235)), 배치 탐색 `findChartLocation_bruteForce` ([xatlas.cpp:8652](../source/xatlas/xatlas.cpp#L8652)), 차트 래스터화 → `BitImage` ([xatlas.cpp:1333](../source/xatlas/xatlas.cpp#L1333)).

## PackOptions — 패킹 제어 옵션

`xatlas::PackOptions` ([xatlas.h:197](../source/xatlas/xatlas.h#L197)):

| 필드 | 기본값 | 설명 |
|---|---|---|
| `maxChartSize` | `0` (무제한) | 이보다 큰 차트는 축소 |
| `padding` | `0` | 차트 주변에 둘 패딩(텍셀 수) |
| `texelsPerUnit` | `0.0` | 단위 길이당 텍셀 수(밀도). 0이면 `resolution`에 맞춰 추정 |
| `resolution` | `0` | 아틀라스 해상도. 0이면 단일 아틀라스를 `texelsPerUnit`로 결정. 0이 아니면 해당 해상도의 아틀라스를 1개 이상 생성 |
| `bilinear` | `true` | bilinear 필터링 시 샘플될 텍셀을 위한 여유 공간 확보 |
| `blockAlign` | `false` | 차트를 4×4 블록에 정렬 (패킹 속도도 향상) |
| `bruteForce` | `false` | 느리지만 최상의 결과. 끄면 랜덤 배치 |
| `createImage` | `false` | `Atlas::image` 생성 여부 |
| `rotateChartsToAxis` | `true` | 차트를 convex hull 축에 맞춰 회전 |
| `rotateCharts` | `true` | 패킹 향상을 위한 차트 회전 허용 |

> `texelsPerUnit`/`resolution` 관계: 둘 다 0이면 약 1024×1024 아틀라스에 맞춰 추정. `resolution`만 0이 아니면 `texelsPerUnit`을 그에 맞게 추정.

## 배치 알고리즘 개요

1. 각 차트를 UV → 2D 비트맵으로 **보수적 래스터화**(conservative rasterization), `padding`/`bilinear`를 위한 dilation 수행.
2. 아틀라스 비트맵 위에서 위치를 탐색해 겹치지 않으면서 적합도(extent²+area)가 가장 좋은 자리에 배치.
   - `bruteForce`이면 모든 후보 위치를 순회(느리지만 최적), 아니면 랜덤 배치.
3. 차트가 다 안 들어가면 새로운 sub-atlas 생성(`resolution` 지정 시).

## 뷰어(GUI)에서의 패킹 기능

"Pack options" 패널 ([viewer_atlas.cpp:993](../source/viewer/viewer_atlas.cpp#L993)).

| GUI 라벨 | 매핑 필드 | 범위/비고 | 위치 |
|---|---|---|---|
| `Bilinear` (체크박스) | `bilinear` | | [:997](../source/viewer/viewer_atlas.cpp#L997) |
| `Brute force` (체크박스) | `bruteForce` | | :998 |
| `Block align` (체크박스) | `blockAlign` | 4×4 블록 정렬 | :999 |
| `Texels per unit` (슬라이더) | `texelsPerUnit` | 0.0 ~ 32.0 | :1000 |
| `Resolution` (입력) | `resolution` | 최소 8 | :1001 |
| `Padding` (슬라이더) | `padding` | 0 ~ 8 | :1002 |
| `Max chart size` (입력) | `maxChartSize` | | :1003 |
| `Rotate charts` (체크박스) | `rotateCharts` | UV 메시 모드일 때만 | :1005 |
| `Reset to default` (버튼) | 기본값 + `createImage=true` 복원 | | :1007 |

> 옵션 변경 시 `packChanged` 플래그가 설정되어, 다음 생성 때 `PackCharts`만 재실행됩니다 (분할 결과 재사용).

## 아틀라스 미리보기 / 통계 (GUI)

"Atlas" 창 ([viewer_atlas.cpp:1053](../source/viewer/viewer_atlas.cpp#L1053)):

- 여러 아틀라스 간 좌우 화살표로 이동, "Atlas N of M" 표시.
- `Fit to window`, `Scale`(수동 배율).
- `Show Bilinear`(bilinear 영역 초록), `Show Padding`(패딩 영역 빨강), `Show 4x4 grid`(4텍셀 격자).
- 텍스처 위에 마우스 오버 시 차트 인덱스 툴팁.

결과 통계 ([viewer_atlas.cpp:1015](../source/viewer/viewer_atlas.cpp#L1015)): 아틀라스 크기(`WxH` 또는 `WxHxN`), 차트 수, 정점/삼각형 수, texels per unit, 활용률(utilization %).

관련: UV unwrap은 [02-uv-unwrap.md](02-uv-unwrap.md).
