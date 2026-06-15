# Windows 개발/빌드 환경 셋업 스크립트 (PowerShell).
#   - 필수 도구(git, cmake, Visual Studio 2022) 확인
#   - vcpkg 클론 + 부트스트랩
#
# 사용법 (PowerShell):  .\scripts\setup-windows.ps1
#   실행 정책 막히면:   powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows.ps1
#
# 전제: Visual Studio 2022 (Desktop development with C++) 가 설치되어 있어야 합니다.
#       git, cmake 가 PATH 에 있어야 합니다. (없으면 winget 으로 설치 안내)

$ErrorActionPreference = "Stop"

function Test-Cmd($name) { return [bool](Get-Command $name -ErrorAction SilentlyContinue) }

Write-Host "==> 필수 도구 확인"
foreach ($tool in @("git", "cmake")) {
    if (-not (Test-Cmd $tool)) {
        Write-Warning "$tool 가 PATH 에 없습니다. 설치 예: winget install $tool"
    } else {
        Write-Host "  OK: $tool"
    }
}

Write-Host "==> vcpkg 셋업"
$VcpkgDir = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $env:USERPROFILE "vcpkg" }
if (-not (Test-Path (Join-Path $VcpkgDir ".git"))) {
    git clone https://github.com/microsoft/vcpkg.git $VcpkgDir
}
& (Join-Path $VcpkgDir "bootstrap-vcpkg.bat") -disableMetrics

Write-Host ""
Write-Host "==> 완료. 환경변수 VCPKG_ROOT 를 설정하세요 (영구 적용):"
Write-Host "    setx VCPKG_ROOT `"$VcpkgDir`""
Write-Host ""
Write-Host "이후 빌드 (새 셸에서):"
Write-Host "    cd tools\glb-pipeline"
Write-Host "    cmake --preset windows"
Write-Host "    cmake --build --preset windows"
