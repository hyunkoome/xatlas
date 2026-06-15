#!/usr/bin/env bash
#
# Ubuntu 개발/빌드 환경 셋업 스크립트.
#   - 빌드 도구(build-essential, cmake, ninja, git 등) 설치
#   - vcpkg 클론 + 부트스트랩 (의존성: tinygltf, meshoptimizer, ...)
#
# 사용법:  bash scripts/setup-ubuntu.sh
# (sudo 권한이 필요한 apt 단계가 포함되어 있습니다.)

set -euo pipefail

echo "==> apt 패키지 설치"
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  git \
  curl zip unzip tar \
  pkg-config

# Embree/베이킹 단계에서 추가로 필요해지면 여기에 시스템 라이브러리를 더합니다.
# (현재 v1 스코프(load/simplify/unwrap)는 위 패키지 + vcpkg로 충분)

echo "==> vcpkg 셋업"
VCPKG_DIR="${VCPKG_ROOT:-$HOME/vcpkg}"
if [ ! -d "$VCPKG_DIR/.git" ]; then
  git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
fi
"$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics

echo ""
echo "==> 완료. 아래를 ~/.bashrc 등에 추가하세요:"
echo "    export VCPKG_ROOT=$VCPKG_DIR"
echo ""
echo "이후 빌드:"
echo "    cd tools/glb-pipeline"
echo "    cmake --preset linux && cmake --build --preset linux"
