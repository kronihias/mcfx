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
!define OUTFILE "../_WIN_RELEASE/mcfx_v${VERSION}_Standalone_win64.exe"
OutFile ${OUTFILE}

; Build Unicode installer
Unicode True

; The default installation directory
InstallDir "$PROGRAMFILES64\mcfx"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------
; Pages
!insertmacro MUI_PAGE_WELCOME

!define MUI_TEXT_WELCOME_INFO_TITLE "MCFX v${VERSION}"

!insertmacro MUI_PAGE_LICENSE "../README.md"
!insertmacro MUI_LANGUAGE "English"
Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section
    SetOutPath "$INSTDIR"
    File "..\build\standalone\*.exe"
    ${DisableX64FSRedirection}
SectionEnd
