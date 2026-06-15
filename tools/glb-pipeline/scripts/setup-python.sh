#!/usr/bin/env bash
#
# Python 보조 도구용 격리 환경 셋업 (uv 우선). 시스템 pip 를 오염시키지 않는다.
#   - uv 가 없으면 공식 인스톨러로 자동 설치 (~/.local/bin).
#   - .venv 생성 후, 잠금된 requirements.txt 로 정확히 동기화(uv pip sync).
#   - uv 설치가 불가한 오프라인 환경이면 표준 venv 로 폴백.
#
# 사용법:  bash scripts/setup-python.sh
# 활성화:  source .venv/bin/activate   (Windows: .venv\Scripts\activate)
#
# 의존성 버전 변경 시:
#   requirements.in 수정 → uv pip compile requirements.in -o requirements.txt → 본 스크립트 재실행

set -euo pipefail

PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJ_DIR"
VENV_DIR=".venv"
REQ="requirements.txt"

ensure_uv() {
  if command -v uv >/dev/null 2>&1; then return 0; fi
  export PATH="$HOME/.local/bin:$PATH"
  if command -v uv >/dev/null 2>&1; then return 0; fi
  echo "==> uv 미설치 → 공식 인스톨러로 설치"
  if curl -LsSf https://astral.sh/uv/install.sh | sh; then
    export PATH="$HOME/.local/bin:$PATH"
    command -v uv >/dev/null 2>&1
  else
    return 1
  fi
}

if ensure_uv; then
  echo "==> uv $(uv --version) 사용"
  uv venv "$VENV_DIR"
  uv pip sync --python "$VENV_DIR/bin/python" "$REQ"
else
  echo "==> uv 사용 불가 → 표준 venv 폴백 (uv 권장: https://docs.astral.sh/uv/)"
  python3 -m venv "$VENV_DIR"
  "$VENV_DIR/bin/python" -m pip install --upgrade pip
  "$VENV_DIR/bin/python" -m pip install -r "$REQ"
fi

echo ""
echo "==> 완료. 활성화:"
echo "    source $PROJ_DIR/$VENV_DIR/bin/activate"
echo "참고: scripts/gen_test_glb.py 는 의존성 없이 'python3 ...' 로도 실행됩니다."
