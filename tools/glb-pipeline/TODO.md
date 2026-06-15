# glb-pipeline 진행 현황 / 로드맵

> 목표: 고정밀 텍스처 GLB → 메시 압축 → UV unwrap → 패킹 → 베이킹(노멀+베이스컬러) → 압축 GLB.
> 스코프(1차): 노멀 + 베이스컬러만, CPU(Embree), 텍스처 압축(KTX2) 보류.
> 설계 문서: [../../docs/08-glb-compression-pipeline.md](../../docs/08-glb-compression-pipeline.md)

## ✅ 완료 (2026-06-15)

- [x] **빌드 환경**: CMake + vcpkg 매니페스트 (tinygltf, meshoptimizer, nlohmann-json, stb)
- [x] **크로스플랫폼 설정**: CMakePresets (linux/windows), setup 스크립트(Ubuntu/Windows)
- [x] **Python 환경**: uv 기반 `.venv`, `requirements.in`→`requirements.txt` 버전 고정
- [x] **테스트 GLB 생성기**: `scripts/gen_test_glb.py` (의존성 없음)
- [x] **골격 파이프라인**: GLB 로드 → weld+decimation → xatlas unwrap → GLB write
- [x] **Ubuntu 빌드 + end-to-end 실행 검증** (gcc 12, vcpkg)
      - 검증 결과: 구 9216→2304 삼각형(25%), 86 charts, 활용률 63.9%

## 🔜 다음 (데이터 수령 후, 2026-06-16~)

- [ ] **입력 GLB 3종 확보**: 단일 머티리얼 단순 / 멀티 머티리얼 / 고밀도(30만+)
- [ ] **멀티 머티리얼 여부 확정** → 베이커 구조 결정
- [ ] **출력 지오메트리 재구성**: 단순화 메시 + 새 UV(+노멀)를 출력 GLB에 기록
      (현재는 입력을 그대로 되쓰는 round-trip)
- [ ] **탄젠트 생성**: MikkTSpace 연동 (노멀맵 정확도)
- [ ] **Embree 전사 baker**:
      - [ ] vcpkg 의존성에 embree 추가
      - [ ] 텍셀 래스터화 (LOW 표면점/노멀)
      - [ ] cage ray cast → HIGH 적중점
      - [ ] 노멀 전사 (HIGH 노멀 → LOW 탄젠트공간 인코딩, glTF +Y 규약)
      - [ ] 베이스컬러 전사 (HIGH 텍스처를 HIGH UV로 샘플)
      - [ ] dilation (차트 경계 번짐 방지)
- [ ] **출력 GLB에 베이크 텍스처 임베드** (PNG/JPEG, KTX2는 보류)

## 📋 마무리 / 품질

- [ ] **정량 지표**: HIGH↔LOW 기하 오차, 렌더 PSNR/SSIM, 노멀 각도오차, 삼각형 감소율, 아틀라스 utilization
- [ ] **아틀라스/베이크 PNG 덤프** (Blender 검수용)
- [ ] **3종 에셋 전부 통과** (특히 멀티 머티리얼·고밀도 견고화)
- [ ] **Windows 빌드 실측 확인**

## 🗓 일정 메모

- 골격(빌드+glue)은 결정적 작업이라 빠르게 완료. 남은 시간은 **베이커 품질 반복**에 집중되며, 이는 실데이터 + Blender 시각 검수 왕복에 좌우됨.
- 낙관: 단일 머티리얼 노멀+베이스컬러 첫 결과물 2~3일.
- 안전 목표: 일주일 (멀티 머티리얼·고밀도 견고화 + 검수 반영 버퍼).

## 결정 사항 (요약)

- 빌드: CMake + vcpkg (premake/xatlas 본체는 미변경). 주력 Ubuntu, 빌드확인 Windows.
- I/O: tinygltf (GLB read/write). 압축 지오메트리: meshoptimizer.
- 소비처: Blender + Omniverse → glTF 표준(+Y) 노멀맵 + MikkTSpace 탄젠트.
- 베이크: CPU Embree. OIDN/디노이즈 불필요(결정적 전사). KTX2 보류(임포터 호환성).
- 커밋: Claude attribution 없이 작성.
