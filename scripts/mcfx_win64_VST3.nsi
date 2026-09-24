; Compile script using Nullsoft Scriptable Install System (NSIS) on windows

;--------------------------------
!include x64.nsh
!include "MUI2.nsh"

; load the version from file
!define /file VERSION "../VERSION"

; The name of the installer
Name "mcfx_v${VERSION}_win64"

; The file to write
!system 'mkdir "../_WIN_RELEASE" 2> NUL'
!define OUTFILE "../_WIN_RELEASE/mcfx_v${VERSION}_VST3_win64.exe"
OutFile ${OUTFILE}

; Build Unicode installer
Unicode True

; The default installation directory
InstallDir "$PROGRAMFILES64\Common Files\VST3\mcfx"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------
; Pages
!define MUI_TEXT_WELCOME_INFO_TITLE "MCFX v${VERSION}"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "../README.md"
!insertmacro MUI_PAGE_COMPONENTS
; The directory page picks the VST3 folder; the apps always go to
; $PROGRAMFILES64\mcfx.
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

;--------------------------------

; The stuff to install
Section "VST3 plug-ins" SecVST3
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "..\build\vst3\*.vst3"
SectionEnd

; Built alongside the VST3s by build_all_win64.bat (VST3_STANDALONE_APPS).
; The network tools and the graph host are shipped as apps; the effects are
; plug-in only. mcfx_graph finds its plug-in scanner next to its .exe.
Section "mcfx_send / mcfx_receive / mcfx_graph standalone apps" SecApps
    SetShellVarContext all   ; Start menu shortcuts for all users (admin install)
    SetOutPath "$PROGRAMFILES64\mcfx"
    File "..\build\standalone\mcfx_send.exe"
    File "..\build\standalone\mcfx_receive.exe"
    File "..\build\standalone\mcfx_graph.exe"
    File "..\build\standalone\mcfx_graph_plugin_scanner.exe"

    CreateDirectory "$SMPROGRAMS\mcfx"
    CreateShortcut "$SMPROGRAMS\mcfx\mcfx_send.lnk"    "$PROGRAMFILES64\mcfx\mcfx_send.exe"
    CreateShortcut "$SMPROGRAMS\mcfx\mcfx_receive.lnk" "$PROGRAMFILES64\mcfx\mcfx_receive.exe"
    CreateShortcut "$SMPROGRAMS\mcfx\mcfx_graph.lnk"   "$PROGRAMFILES64\mcfx\mcfx_graph.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "The mcfx VST3 plug-ins."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecApps} "Standalone mcfx_send and mcfx_receive, to stream multichannel audio over the network without a DAW, and mcfx_graph, to host and route plug-ins. Installed to $PROGRAMFILES64\mcfx, with Start menu shortcuts."
!insertmacro MUI_FUNCTION_DESCRIPTION_END
