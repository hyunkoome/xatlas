# 04. 베이커 (Baker)

베이킹은 **뷰어 전용 기능**입니다 (라이브러리 핵심 API가 아니라 뷰어 예제에 구현). 생성된 아틀라스 UV 위에 조명을 구워 **라이트맵**을 만듭니다.

코드: [source/viewer/viewer_bake.cpp](../source/viewer/viewer_bake.cpp).

## 베이킹하는 맵 종류

- **Lightmap (라이트맵 / GI)**: Monte Carlo path tracing으로 직접광 + 간접광(바운스)을 누적한 RGB. RGBA32F 포맷으로 저장 ([viewer_bake.cpp:1001](../source/viewer/viewer_bake.cpp#L1001), 668–672).

> 별도의 normal/position/AO 전용 베이크 타입은 없습니다. AO 효과는 path tracing 과정에서 자연스럽게 나타납니다(자기 차폐).

## 핵심 알고리즘 / 의존성

| 단계 | 내용 | 위치 |
|---|---|---|
| Embree 초기화 | Intel Embree로 BVH/ray 디바이스 생성 (DLL/so 동적 로드) | [:413, :441](../source/viewer/viewer_bake.cpp#L413) |
| Rasterize | 아틀라스 텍셀 ↔ 표면 위치/노멀 대응을 래스터화 | `bakeRasterize` [:484](../source/viewer/viewer_bake.cpp#L484) |
| Trace rays | 반구 균일 샘플링(Halton 의사난수) + 재귀 경로추적 + Russian roulette | `bakeTraceRaysTask` [:585](../source/viewer/viewer_bake.cpp#L585) |
| Filter / Dilation | 빈 텍셀 채움 + bilinear 경계 확장(8-이웃) | `bakeFilter` [:724](../source/viewer/viewer_bake.cpp#L724) |
| Denoise | OpenImageDenoise(OIDN) `RTLightmap` 필터 | `bakeDenoiseThread` [:880](../source/viewer/viewer_bake.cpp#L880) |

- 멀티스레딩: 워커 스레드 + enkiTS 태스크 스케줄러로 ray tracing 병렬화 ([:804, :699](../source/viewer/viewer_bake.cpp#L804)). 초당 처리 ray 수(`Mrays/s`)를 로그로 출력.
- 광선 근평면 `kNear = 0.01 * modelScale`, 원평면 `FLT_MAX`(무한) — 그래서 AO 거리 제한 슬라이더는 없음.

## GUI 컨트롤

"Lightmap" 옵션 패널 `bakeShowGuiOptions` ([viewer_bake.cpp:1069](../source/viewer/viewer_bake.cpp#L1069)):

| GUI 라벨 | 컨트롤 | 매핑 | 범위/기본값 | 위치 |
|---|---|---|---|---|
| `Bake` | 버튼 | `bakeExecute()` 실행 | | [:1087](../source/viewer/viewer_bake.cpp#L1087) |
| `Denoise` | 버튼 | OIDN 디노이즈 실행 | 베이크 완료 후 | :1091 |
| `Sky color` | ColorEdit3 | `skyColor` | 기본 흰색(1,1,1) | :1095 |
| `Max depth` | SliderInt | `maxDepth` | **1~16, 기본 8** | :1096 |
| `Stop` | 버튼 | 누적 샘플 보존하며 trace 중단 | trace 중에만 | :1122 |
| `Cancel` | 버튼 | 워커 스레드 종료 | rasterize/trace 중 | :1115, :1125 |

진행 표시: `Initializing Embree` → `Rasterizing`(진행 바) → `Tracing rays [N samples]`(누적 샘플 수 표시) → `Denoising`(OIDN 진행률).

## 라이트맵 표시 / 뷰 옵션

"Lightmap" 창 `bakeShowGuiWindow` ([viewer_bake.cpp:1138](../source/viewer/viewer_bake.cpp#L1138)):

- `Fit to window` 체크박스(기본 켜짐), `Scale` 입력(수동 배율).
- 라이트맵 텍스처 미리보기(RGBA32F, point sampling).

뷰어 View 메뉴 ([viewer.cpp:699](../source/viewer/viewer.cpp#L699)):

- `Lightmap point sampling`: 라이트맵을 nearest/linear 필터 토글.
- `Use denoised lightmap`: 원본 ↔ OIDN 디노이즈 결과 전환.
- 셰이딩 모드 `Lightmapped material`, `Lightmap only`로 3D 모델에 라이트맵 입혀 보기.

## 제약

- 멀티 아틀라스 미지원: "Baking doesn't support multiple atlases" ([:1076](../source/viewer/viewer_bake.cpp#L1076)).
- 32비트 빌드 미지원 ([:1082](../source/viewer/viewer_bake.cpp#L1082)).
- Embree/OIDN 미설치 시 에러 메시지 출력(동적 로드 실패).
- 라이트맵 파일 **저장(export) GUI는 없음**. 데이터는 `bakeGetLightmap()`으로 접근 가능 ([:1167](../source/viewer/viewer_bake.cpp#L1167)).

> 베이킹의 ray tracing은 현재 **CPU(Embree)** 기반입니다. GPU(OptiX/CUDA) 가속 후보는 [07-gpu-cuda-acceleration.md](07-gpu-cuda-acceleration.md) 참고.
