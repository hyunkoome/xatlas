# 05. 뷰어 GUI 전반

xatlas 뷰어는 모델을 불러와 UV unwrap → 패킹 → 베이킹을 시각적으로 확인하는 데스크톱 앱입니다. (UV/패킹/베이크 상세는 각각 [02](02-uv-unwrap.md)/[03](03-packing.md)/[04](04-baker.md) 참고.)

코드: [viewer.cpp](../source/viewer/viewer.cpp), [viewer_gui.cpp](../source/viewer/viewer_gui.cpp), [viewer_model.cpp](../source/viewer/viewer_model.cpp).

## 1. 모델 로딩

지원 포맷 ([viewer_model.cpp](../source/viewer/viewer_model.cpp)):

| 포맷 | 라이브러리 |
|---|---|
| `.obj` | tinyobjloader / objzero |
| `.fbx` | OpenFBX |
| `.gltf` / `.glb` | cgltf |
| `.stl` | stl_reader |

- `Model` 메뉴 → `Open...` (NFD 네이티브 파일 다이얼로그) ([viewer.cpp:622](../source/viewer/viewer.cpp#L622)).
- 커맨드라인 첫 인자로 모델 경로 전달 가능 ([viewer.cpp:518](../source/viewer/viewer.cpp#L518)).
- 백그라운드 스레드 비동기 로딩, 진행 스피너/바 표시. 실패 시 모달 에러.

## 2. 카메라 / 내비게이션

두 가지 카메라 모드:

| 모드 | 활성화 | 조작 |
|---|---|---|
| **Orbit** (기본) | F4 | 좌클릭 드래그 회전, 휠/`+`·`-` 줌. 거리 0.1~500, pitch ±75° |
| **First-person** | F3 | WASD/방향키 이동, Q/E 상하, 좌클릭 드래그 시선. Shift 20×, Ctrl 0.2× 속도 |

- `Camera` 메뉴에서 모드 라디오 선택 + 감도 슬라이더(0.01~1.0) ([viewer.cpp:632](../source/viewer/viewer.cpp#L632)).
- 모델 로드 시 카메라 자동 리셋.

## 3. 렌더링 / 표시 모드 (View 메뉴)

- **Wireframe** 토글: `Charts`(차트 와이어프레임) / `Triangles`(삼각형) ([viewer.cpp:658](../source/viewer/viewer.cpp#L658)).
- **셰이딩 모드**: `Flat material`(기본) / `Lightmapped material` / `Lightmap only`(베이크 필요).
- **오버레이**: `None` / `Chart` / `Mesh` / `Stretch`(UV 왜곡) + 불투명도 슬라이더(0~1).
- **차트 색상 모드**: `Planar`/`Ortho`/`LSCM`/`Piecewise`/`Invalid`/`All` + 셀 크기 슬라이더(1~32).
- 단축키: `F1` bgfx 통계, `F2` GUI 토글.

## 4. 창 구성 / 워크플로우

ImGui 도킹 레이아웃 ([viewer.cpp:605](../source/viewer/viewer.cpp#L605)):

- 좌측 (~20%): **Atlas Options** — 아틀라스 생성 옵션 + (아틀라스 준비 후) 베이크 옵션.
- 우측 (~30%): **Atlas** — UV 아틀라스 미리보기 (아틀라스 생성 후 표시).
- 우측 하단: **Lightmap** — 베이크 라이트맵 미리보기 (베이크 후 표시).
- 중앙: 3D 모델 뷰포트.
- `Window` 메뉴에서 각 창 표시 토글.

워크플로우: 모델 열기 → (좌측) 옵션 조정 → `Generate` → (우측) 아틀라스 확인 → `Bake` → (하단) 라이트맵 확인.

## 5. 렌더링 백엔드 / 윈도잉

- 렌더링: **bgfx** 기반 — OpenGL / Direct3D 11(Windows) / Vulkan 자동 선택 ([viewer.cpp:376](../source/viewer/viewer.cpp#L376)).
- 윈도잉: **GLFW**. 타이틀 "xatlas viewer", 기본 1920×1080, MSAA×16, VSYNC.
- 플랫폼별 네이티브 핸들(X11 / Cocoa / Win32) 처리.

## 6. 기타

- 폰트: Roboto + Font Awesome 아이콘.
- 스레드 안전 에러 메시지 큐 → 모달 표시.
- 모델/아틀라스 **export·save GUI는 별도로 없음** (분석/확인 위주 뷰어).
