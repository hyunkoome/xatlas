# 07. GPU / CUDA 고속화 후보 리스트업

xatlas 파이프라인에서 **GPU(CUDA)로 가속 가능한 연산 부하 지점**을 분석한 문서입니다. 각 항목의 알고리즘 성격, 현재 병렬화 상태, CUDA 적합도, 기대 효과를 정리합니다.

> 결론 요약: **LSCM 솔버(cuSPARSE/cuBLAS)**, **차트 래스터화/dilation**, **베이커 ray tracing(OptiX)** 이 3대 후보입니다. 반면 차트 분할(region growing)과 메시 빌드는 직렬성이 강해 GPU 이득이 작습니다.

---

## 우선순위 요약표

| # | 대상 | 알고리즘 | 병렬성 | CUDA 적합도 | 기대 효과 | 위치 |
|---|---|---|---|---|---|---|
| 1 | **LSCM 솔버** | 전처리 CG (SpMV + BLAS) | 높음(희소 선형대수) | ★★★ cuSPARSE/cuBLAS | 10~50× | [xatlas.cpp:6482](../source/xatlas/xatlas.cpp#L6482), [:4386](../source/xatlas/xatlas.cpp#L4386) |
| 2 | **차트 래스터화 + dilation** | 삼각형 래스터화 + 이웃 확장 | 매우 높음(픽셀 병렬) | ★★★ 컴퓨트 커널 | 20~100× | [:8449](../source/xatlas/xatlas.cpp#L8449), [:1333](../source/xatlas/xatlas.cpp#L1333) |
| 3 | **베이커 ray tracing** | Monte Carlo path tracing | 매우 높음(ray 병렬) | ★★★ OptiX/CUDA | 큼(GPU RT) | [viewer_bake.cpp:585](../source/viewer/viewer_bake.cpp#L585) |
| 4 | **per-face/vertex 벌크 연산** | 노멀/면적/직교투영 | 임베러싱리 병렬 | ★★★ 단순 커널 | 5~30× | [:2705](../source/xatlas/xatlas.cpp#L2705), [:2673](../source/xatlas/xatlas.cpp#L2673), [:7276](../source/xatlas/xatlas.cpp#L7276) |
| 5 | **패킹 위치 탐색** | brute-force 2D 배치 | 위치별 병렬(조기종료 손해) | ★★ atomic-min 리덕션 | 5~15× | [:8652](../source/xatlas/xatlas.cpp#L8652) |
| 6 | 차트 분할(segmentation) | 우선순위 큐 region growing | **직렬 병목** | ★ (재설계 필요) | 1~3× | [:5415](../source/xatlas/xatlas.cpp#L5415) |
| 7 | 메시 빌드/해싱 | 정점 용접·인덱스 재매핑 | 약함, 대역폭 바운드 | ★ | 1~2× | [:9627](../source/xatlas/xatlas.cpp#L9627) |

---

## 1. LSCM 파라미터화 솔버 — **최우선 후보**

LSCM(Least Squares Conformal Maps)은 일반 곡면 차트를 펼치는 핵심 수치 계산으로, **전처리 Conjugate Gradient(PRE_CG)** 로 희소 선형 시스템을 풉니다.

- 솔버 진입: `computeLeastSquaresConformalMap` ([xatlas.cpp:6482](../source/xatlas/xatlas.cpp#L6482))
- CG 본체: `nlSolveSystem_PRE_CG` ([:4386](../source/xatlas/xatlas.cpp#L4386))
- 희소 행렬-벡터 곱(SpMV): `nlSparseMatrix_mult_rows` ([:4217](../source/xatlas/xatlas.cpp#L4217)), CRS 버전 ([:4140](../source/xatlas/xatlas.cpp#L4140))
- Jacobi 전처리: `nlJacobiPreconditionerMult` ([:4477](../source/xatlas/xatlas.cpp#L4477))
- BLAS: `ddot`/`daxpy`/`dscal` ([:4348](../source/xatlas/xatlas.cpp#L4348))

**반복당 연산**: SpMV 2회(`M·d`, `P·r`) + 내적 3회 + AXPY 3회 + 스칼라곱 1회. 최대 `5 × vertexCount` 반복 ([:6492](../source/xatlas/xatlas.cpp#L6492)).

**자료구조**: CRS(Compressed Row Storage) — `val[]`, `rowptr[]`, `colind[]`, 대각 `diag[]`.

**현재 상태**: CPU 직렬. CRS에 8-slice 병렬 구조가 있으나 실제로는 슬라이스를 순차 처리.

**CUDA 매핑**:
- SpMV → **cuSPARSE** `cusparseSpMV` (CRS 그대로 사용 가능)
- 내적/AXPY/SCAL → **cuBLAS** `cublasDdot`/`cublasDaxpy`/`cublasDscal`
- Jacobi 전처리 → 원소별 곱 커널 또는 cuBLAS
- 대안: cuSPARSE/cuSOLVER의 기성 CG, 또는 AMGX

**주의점**: 차트당 정점 수가 작으면 H2D/D2H 전송 오버헤드가 이득을 잡아먹음. **차트 여러 개를 배치(batch)로 묶어 GPU로** 푸는 설계가 유리. 큰 메시(차트당 >10k 정점)에서 효과 큼.

---

## 2. 차트 래스터화 + Dilation (패킹) — **최우선 후보**

패킹 시 각 차트를 2D 비트맵에 보수적 래스터화하고, `padding`/`bilinear`용 dilation을 수행합니다.

- 래스터화 루프 ([xatlas.cpp:8449](../source/xatlas/xatlas.cpp#L8449)), `BitImage` ([:1333](../source/xatlas/xatlas.cpp#L1333))
- dilation: 8-이웃 스캔 ([:1433](../source/xatlas/xatlas.cpp#L1433))

**성격**: 삼각형/픽셀 단위로 완전 독립 → 임베러싱리 병렬. 비트맵은 `uint64_t` 패킹.

**현재 상태**: 차트별·삼각형별 순차.

**CUDA 매핑**:
- 삼각형 래스터화 → 그래픽 파이프라인 또는 컴퓨트 커널(픽셀당 1스레드)
- dilation → separable convolution / 비트 연산 커널
- 겹침 판정 `canBlit` ([:1403](../source/xatlas/xatlas.cpp#L1403)) → 비트 AND 리덕션

해상도²만큼의 픽셀 병렬이라 가속 폭이 가장 큼.

---

## 3. 베이커 Ray Tracing — **GPU RT 후보** (뷰어 기능)

라이트맵 베이킹은 현재 **CPU Embree**로 path tracing합니다 ([viewer_bake.cpp:585](../source/viewer/viewer_bake.cpp#L585)).

**성격**: texel × sample × bounce 모두 독립 → GPU ray tracing에 이상적.

**CUDA 매핑**:
- **NVIDIA OptiX** (RTX 하드웨어 BVH/RT 코어) 또는 순수 CUDA 커널로 path tracer 구현
- Embree(CPU) ↔ OptiX(GPU)를 빌드 옵션으로 선택
- 디노이즈는 이미 OIDN인데, **OIDN GPU(CUDA) 백엔드** 또는 **OptiX AI Denoiser**로 대체 가능

> 베이킹은 라이브러리 핵심이 아니라 뷰어 예제 기능이라는 점 유의. 상세는 [04-baker.md](04-baker.md).

---

## 4. Per-face / Per-vertex 벌크 연산 — **쉬운 후보**

완전 독립 연산이라 단순 커널로 즉시 병렬화 가능:

- 면 노멀 계산 (cross + normalize) ([xatlas.cpp:2705](../source/xatlas/xatlas.cpp#L2705))
- 면 면적 계산 ([:2673](../source/xatlas/xatlas.cpp#L2673))
- 직교 투영 파라미터화(Planar/Ortho): 정점당 dot product 2회 ([:7276](../source/xatlas/xatlas.cpp#L7276))

**주의**: 연산 강도(arithmetic intensity)가 낮아 메모리 대역폭 바운드. 단독 가속보다는 **다른 GPU 단계와 융합(fuse)** 하거나 데이터가 이미 GPU에 있을 때 의미 있음. 그렇지 않으면 전송 비용이 더 큼.

---

## 5. 패킹 위치 탐색 (brute force) — **부분 후보**

`findChartLocation_bruteForce` ([xatlas.cpp:8652](../source/xatlas/xatlas.cpp#L8652)): 모든 (x, y) 후보 위치에 대해 적합도 계산 + 겹침 판정.

**성격**: 위치별 독립이지만, 공유 `best_metric` 기반 **조기 종료**가 있어 병렬화 시 그 이점이 줄어듦.

**CUDA 매핑**: 위치를 스레드에 매핑, `atomicMin`으로 최적 적합도 리덕션. 조기 종료율이 높은 입력에서는 이득이 제한적.

---

## 6·7. GPU 부적합 (직렬/대역폭 바운드)

- **차트 분할 (region growing)** ([xatlas.cpp:5415](../source/xatlas/xatlas.cpp#L5415)): 단일 전역 우선순위 큐로 면을 하나씩 흡수 → 본질적 직렬. 비용 계산(`computeCost` [:5844](../source/xatlas/xatlas.cpp#L5844))만 부분 병렬 가능하나 병목 해소엔 한계. GPU로 의미 있는 이득을 보려면 **동시 다중 차트 flood-fill** 식 알고리즘 재설계 필요.
- **메시 빌드 / 해싱** ([:9627](../source/xatlas/xatlas.cpp#L9627)): 정점 용접·인덱스 재매핑은 포인터 추적·메모리 복사 위주 → 대역폭 바운드, GPU 전송이 오히려 손해.

---

## 권장 접근 순서

1. **LSCM 솔버를 cuSPARSE/cuBLAS로 이식** (차트 배치 처리로 전송 오버헤드 최소화) — 가장 확실한 이득.
2. **패킹 래스터화/dilation을 컴퓨트 커널화**.
3. (뷰어) **베이커를 OptiX로** — 별도 모듈이라 독립적으로 진행 가능.
4. per-face/vertex 연산은 위 단계들과 **데이터 상주 + 융합** 전제로만.

> 모든 추정 배속은 메시 크기·차트 분포·전송 패턴에 크게 좌우됩니다. 실제 적용 전 프로파일링으로 병목(특히 H2D/D2H 전송 비중)을 먼저 확인하길 권장합니다.
