# glb-pipeline

고정밀 텍스처 GLB를 **메시 압축 → UV unwrap → 패킹 → (베이킹) → 압축 GLB**로 변환하는 CLI 도구.

설계 배경: [../../docs/08-glb-compression-pipeline.md](../../docs/08-glb-compression-pipeline.md)

## 현재 상태 (v1 골격)

- [x] GLB 로드 (tinygltf)
- [x] weld + decimation (meshoptimizer)
- [x] UV unwrap + 패킹 (xatlas, 본 저장소 소스 직접 컴파일)
- [x] GLB 저장 (tinygltf, 현재는 round-trip)
- [ ] 단순화된 지오메트리 + 새 UV를 출력 GLB로 재구성
- [ ] 베이킹: 노멀 + 베이스컬러 전사 (Embree)
- [ ] 정량 지표 / 아틀라스 PNG 덤프

> 베이크 스코프(1차): **노멀 + 베이스컬러**만. CPU(Embree). 텍스처 압축(KTX2)은 보류.

## 의존성

- **vcpkg** 매니페스트 모드([vcpkg.json](vcpkg.json)): tinygltf, meshoptimizer, nlohmann-json, stb
- **xatlas**: 저장소의 [source/xatlas/xatlas.cpp](../../source/xatlas/xatlas.cpp)를 직접 컴파일
- (베이킹 단계 추가 시) Embree

## 빌드

### 1. 환경 셋업 (최초 1회)

```bash
# Ubuntu
bash scripts/setup-ubuntu.sh
export VCPKG_ROOT=$HOME/vcpkg      # ~/.bashrc 에 추가 권장
```

```powershell
# Windows (PowerShell)
.\scripts\setup-windows.ps1
setx VCPKG_ROOT "$env:USERPROFILE\vcpkg"
```

### 2. 빌드

```bash
# Ubuntu
cmake --preset linux
cmake --build --preset linux

# Windows
cmake --preset windows
cmake --build --preset windows
```

생성물: `build/<preset>/glb-pipeline` (Windows는 `build/windows/Release/glb-pipeline.exe`).

## 실행

```bash
./build/linux/glb-pipeline input.glb output.glb --ratio 0.25
```

- `--ratio` : 단순화 목표 비율(삼각형 수 기준, 기본 0.25).

## Python 보조 환경 (지표/검수용, uv)

품질 지표 계산 등 Python 도구는 **uv 격리 환경**을 씁니다 (시스템 pip 미사용).
버전은 `requirements.in`(상위 명세) → `requirements.txt`(잠금)로 관리합니다.

```bash
bash scripts/setup-python.sh          # uv 자동설치 + .venv 생성 + lock 동기화
source .venv/bin/activate
```

의존성 변경:

```bash
# requirements.in 편집 후
uv pip compile requirements.in -o requirements.txt   # 잠금 갱신
uv pip sync requirements.txt                          # 환경 반영
```

> 테스트 GLB 생성기 `scripts/gen_test_glb.py`는 의존성이 없어 `python3`로 바로 실행됩니다.

## 디렉터리

```
tools/glb-pipeline/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── scripts/            # 환경 셋업 스크립트 (Ubuntu / Windows)
├── src/
│   ├── main.cpp        # 파이프라인 오케스트레이션
│   └── tiny_impl.cpp   # 헤더-only 구현 컴파일 단위
└── assets/             # 테스트 GLB (git 추적 제외)
```
