; example1.nsi
;
; This script is perhaps one of the simplest NSIs you can make. All of the
; optional settings are left to their default settings. The installer simply 
; prompts the user asking them where to install, and drops a copy of example1.nsi
; there. 

;--------------------------------
!include x64.nsh
!include "MUI.nsh"

; load the version from file
!define /file VERSION "../VERSION"

; The name of the installer
Name "mcfx_v${VERSION}_win64"

; The file to write
!define OUTFILE "../_WIN_RELEASE/mcfx_v${VERSION}_win64.exe"
OutFile ${OUTFILE}

; The default installation directory
InstallDir $PROGRAMFILES64\Steinberg\VSTPlugins

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages
!insertmacro MUI_PAGE_LICENSE "../README.md"

Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File /r ..\build\vst\*.dll

   ${DisableX64FSRedirection}

SectionEnd ; end the section
