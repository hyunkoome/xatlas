# xatlas 문서

이 폴더는 xatlas 라이브러리와 뷰어(GUI)의 기능, 그리고 GPU(CUDA) 가속 후보를 정리한 문서입니다.

xatlas는 라이트맵 베이킹이나 텍스처 페인팅에 적합한 **고유 텍스처 좌표(UV)를 자동 생성**하는 C++11 라이브러리입니다. [thekla_atlas](https://github.com/Thekla/thekla_atlas)에서 갈라져 나온 독립 fork입니다.

## 파이프라인 한눈에 보기

```
입력 메시(AddMesh)
   └─> ComputeCharts   : 차트 분할(segmentation) + 파라미터화(LSCM 등)  ── "UV unwrap"
        └─> PackCharts : 차트들을 아틀라스에 배치(packing)
             └─> BuildOutputMeshes : 출력 메시 생성
                  └─> (옵션) Bake   : 뷰어에서 라이트맵 베이킹
```

## 문서 목록

| 문서 | 내용 |
|---|---|
| [01-overview.md](01-overview.md) | 전체 구조, 파이프라인, 핵심 자료구조 |
| [02-uv-unwrap.md](02-uv-unwrap.md) | **UV unwrap** — 차트 분할/파라미터화, `ChartOptions`, GUI |
| [03-packing.md](03-packing.md) | **패킹** — `PackOptions`, 배치 알고리즘, GUI |
| [04-baker.md](04-baker.md) | **베이커** — 라이트맵 베이킹, ray tracing, denoise, GUI |
| [05-viewer-gui.md](05-viewer-gui.md) | 뷰어 GUI 전반 — 모델 로딩, 카메라, 렌더링, 창 구성 |
| [06-api-reference.md](06-api-reference.md) | 공개 C++ API 레퍼런스 (`xatlas.h`) |
| [07-gpu-cuda-acceleration.md](07-gpu-cuda-acceleration.md) | **GPU/CUDA 고속화 후보** 리스트업 |
| [08-glb-compression-pipeline.md](08-glb-compression-pipeline.md) | **GLB 압축 파이프라인** — merge→unwrap→pack→bake→저장, 결합 오픈소스 |

## 코드 위치 요약

- 라이브러리 본체(단일 파일): [source/xatlas/xatlas.cpp](../source/xatlas/xatlas.cpp), [source/xatlas/xatlas.h](../source/xatlas/xatlas.h)
- C 바인딩: [source/xatlas/xatlas_c.h](../source/xatlas/xatlas_c.h)
- 뷰어(GUI): [source/viewer/](../source/viewer/)
- 예제: [source/examples/](../source/examples/)
