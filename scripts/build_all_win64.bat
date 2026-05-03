@echo off
setlocal
cd /d "%~dp0"

set BUILD_DIR=..\build
if not defined MSBUILD set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\MSBuild.exe"
if not defined MAKENSIS set MAKENSIS="C:\Program Files (x86)\NSIS\makensis.exe"
set MSBUILD_FLAGS=/t:Build /p:Configuration=Release /p:PreBuildEvent= /p:PostBuildEvent=
set CMAKE_COMMON=-DFFTW3_INCLUDE_DIR="../win-libs/" -DFFTW3F_LIBRARY="../win-libs/x64/libfftw3f-3.lib"

REM VST2SDKPATH is needed for VST2 hosting in mcfx_anything (and for VST2 plugin
REM builds). When provided via env (e.g. by GitHub Actions) propagate it to cmake.
if defined VST2SDKPATH set CMAKE_COMMON=%CMAKE_COMMON% -DVST2SDKPATH="%VST2SDKPATH%"

REM ── Code signing configuration ───────────────────────────────────────────────
REM   Set SIGN_CERT to the .p12 certificate file and SIGN_PASS to its password.
REM   Pass "sign" as a build argument to enable signing.
REM   All of SIGNTOOL / SIGN_CERT / SIGN_PASS / INSTALLER_CERT / INSTALLER_PASS
REM   can be pre-set in the environment (e.g. by CI) — defaults below only apply
REM   if the variable is not already defined.
if not defined SIGNTOOL set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe"
if not defined SIGN_CERT set SIGN_CERT=DevIDApplication.p12
if not defined SIGN_PASS set SIGN_PASS=
if not defined INSTALLER_CERT set INSTALLER_CERT=DevIDInstaller.p12
if not defined INSTALLER_PASS set INSTALLER_PASS=

REM ── Parse arguments ──────────────────────────────────────────────────────────
REM Usage: build_all_win64.bat [vst2] [vst3] [standalone] [sign]
REM Default (no args): build all three formats without signing

set BUILD_VST2=0
set BUILD_VST3=0
set BUILD_SA=0
set SIGN=0

if "%~1"=="" (
    set BUILD_VST2=1
    set BUILD_VST3=1
    set BUILD_SA=1
    goto :start_build
)
:parse_loop
if /i "%~1"=="vst2"       set BUILD_VST2=1
if /i "%~1"=="vst3"       set BUILD_VST3=1
if /i "%~1"=="standalone" set BUILD_SA=1
if /i "%~1"=="sign"       set SIGN=1
shift
if not "%~1"=="" goto :parse_loop

:start_build
echo.
echo Build targets:  VST2=%BUILD_VST2%  VST3=%BUILD_VST3%  Standalone=%BUILD_SA%  Sign=%SIGN%
echo.

REM ── VST3 + Standalone universal builds (single binary, up to 64 channels) ────
REM   When both are requested they share one cmake configure + MSBuild pass.
if "%BUILD_VST3%%BUILD_SA%"=="11" (
    echo ================================================================
    echo  BUILDING VST3 + Standalone  ^(universal, up to 64 channels^)
    echo ================================================================
    pushd "%BUILD_DIR%"
    cmake .. %CMAKE_COMMON% -DBUILD_VST3=TRUE -DBUILD_VST=TRUE -DBUILD_STANDALONE=TRUE -DMCFX_BUILD_MC=ON -DMCFX_BUILD_VST2_PER_CHANNEL=OFF -DMCFX_MAX_CHANNELS=64
    %MSBUILD% mcfx_plugin_suite.sln %MSBUILD_FLAGS%
    popd
) else if "%BUILD_VST3%"=="1" (
    echo ================================================================
    echo  BUILDING VST3  ^(universal, up to 64 channels^)
    echo ================================================================
    pushd "%BUILD_DIR%"
    cmake .. %CMAKE_COMMON% -DBUILD_VST3=TRUE -DBUILD_VST=TRUE -DBUILD_STANDALONE=FALSE -DMCFX_BUILD_MC=ON -DMCFX_BUILD_VST2_PER_CHANNEL=OFF -DMCFX_MAX_CHANNELS=64
    %MSBUILD% mcfx_plugin_suite.sln %MSBUILD_FLAGS%
    popd
) else if "%BUILD_SA%"=="1" (
    echo ================================================================
    echo  BUILDING Standalone  ^(universal, up to 64 channels^)
    echo ================================================================
    pushd "%BUILD_DIR%"
    cmake .. %CMAKE_COMMON% -DBUILD_VST3=FALSE -DBUILD_VST=TRUE -DBUILD_STANDALONE=TRUE -DMCFX_BUILD_MC=ON -DMCFX_BUILD_VST2_PER_CHANNEL=OFF -DMCFX_MAX_CHANNELS=64
    %MSBUILD% mcfx_plugin_suite.sln %MSBUILD_FLAGS%
    popd
)

REM mcfx_anything uses an out-of-process plugin scanner (mcfx_plugin_scanner.exe)
REM that must sit alongside the plugin binary at runtime. JUCE's
REM JUCE_COPY_PLUGIN_AFTER_BUILD races the scanner's POST_BUILD, so the copied
REM bundle in ..\build\vst3 and the staged standalone dir may not contain it.
REM Copy it explicitly from the scanner's artefacts dir before signing/packaging.
if "%BUILD_VST3%"=="1" if exist "..\build\vst3\mcfx_anything.vst3\Contents\x86_64-win" (
    for /r "..\build\mcfx_anything\mcfx_plugin_scanner_artefacts" %%f in (mcfx_plugin_scanner.exe) do copy /Y "%%f" "..\build\vst3\mcfx_anything.vst3\Contents\x86_64-win\" >nul
)
if "%BUILD_SA%"=="1" if exist "..\build\standalone" (
    for /r "..\build\mcfx_anything\mcfx_plugin_scanner_artefacts" %%f in (mcfx_plugin_scanner.exe) do copy /Y "%%f" "..\build\standalone\" >nul
)

REM Sign VST3 bundles: JUCE on Windows builds VST3 as a directory bundle;
REM the signable PE is the inner *.vst3 file under Contents\x86_64-win\.
REM Also sign any *.exe siblings inside the bundle (mcfx_plugin_scanner.exe).
REM "for /r" matches only files, so the outer bundle directory is skipped.
if "%BUILD_VST3%"=="1" if "%SIGN%"=="1" (
    echo Signing VST3 plugins...
    for /r "..\build\vst3" %%f in (*.vst3 *.exe) do call :do_sign "%%f"
)

REM Sign standalone executables (includes mcfx_plugin_scanner.exe)
if "%BUILD_SA%"=="1" if "%SIGN%"=="1" (
    echo Signing Standalone executables...
    call :do_sign "..\build\standalone\*.exe"
)

REM ── VST2 per-channel builds (one MSBuild pass per channel count) ─────────────
if "%BUILD_VST2%"=="1" (
    for %%x in (2 4 8 16 24 32 36 50 64 84 128) do (
        echo.
        echo ================================================================
        echo  BUILDING VST2  %%x channels
        echo ================================================================
        pushd "%BUILD_DIR%"
        cmake .. %CMAKE_COMMON% -DBUILD_VST3=FALSE -DBUILD_VST=TRUE -DBUILD_STANDALONE=FALSE -DMCFX_BUILD_MC=OFF -DMCFX_BUILD_VST2_PER_CHANNEL=ON -DNUM_CHANNELS:STRING=%%x
        %MSBUILD% mcfx_plugin_suite.sln %MSBUILD_FLAGS%
        popd
    )
)

REM Sign all VST2 DLLs once after the loop completes
if "%BUILD_VST2%"=="1" if "%SIGN%"=="1" (
    echo Signing VST2 plugins...
    call :do_sign "..\build\vst\*.dll"
)

REM ── Package installers ────────────────────────────────────────────────────────
REM   Download Nullsoft's Scriptable Install System (NSIS) from:
REM   https://sourceforge.net/projects/nsis/
echo.
if "%BUILD_VST3%"=="1" (
    echo Packaging VST3 installer...
    %MAKENSIS% /V4 mcfx_win64_VST3.nsi
)
if "%BUILD_SA%"=="1" (
    echo Packaging Standalone installer...
    %MAKENSIS% /V4 mcfx_win64_Standalone.nsi
)
if "%BUILD_VST2%"=="1" (
    echo Packaging VST2 installer...
    %MAKENSIS% /V4 mcfx_win64_VST2.nsi
)

REM Sign the packaged installers
if "%SIGN%"=="1" (
    echo Signing installers...
    call :do_sign_installer "..\_WIN_RELEASE\*_win64.exe"
)

goto :eof

REM ── Subroutine: two-pass signing (SHA1 legacy + SHA256 dual-sign) ─────────────
REM   Usage: call :do_sign <path-or-glob>
:do_sign
%SIGNTOOL% sign /f "%SIGN_CERT%" /p "%SIGN_PASS%" /t http://timestamp.comodoca.com %*
%SIGNTOOL% sign /f "%SIGN_CERT%" /p "%SIGN_PASS%" /fd sha256 /tr http://timestamp.comodoca.com/?td=sha256 /td sha256 /as /v %*
goto :eof

:do_sign_installer
%SIGNTOOL% sign /f "%INSTALLER_CERT%" /p "%INSTALLER_PASS%" /t http://timestamp.comodoca.com %*
%SIGNTOOL% sign /f "%INSTALLER_CERT%" /p "%INSTALLER_PASS%" /fd sha256 /tr http://timestamp.comodoca.com/?td=sha256 /td sha256 /as /v %*
goto :eof
