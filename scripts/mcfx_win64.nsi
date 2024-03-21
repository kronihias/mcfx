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
!define OUTFILE "../_WIN_RELEASE/mcfx_v${VERSION}_win64.exe"
OutFile ${OUTFILE}

; Build Unicode installer
Unicode True

; The default installation directory
InstallDir $PROGRAMFILES64\Steinberg\VSTPlugins\mcfx_v${VERSION}_win64\"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------
; Pages
!insertmacro MUI_PAGE_WELCOME

!define MUI_TEXT_WELCOME_INFO_TITLE "MCFX v${VERSION}"

!insertmacro MUI_PAGE_LICENSE "../README.md"
!insertmacro MUI_LANGUAGE "English"
Page components SelectComponents
Page directory
Page instfiles

Function SelectComponents 
  ; SetBrandingImage C:\Users\cimil\Desktop\test.png
FunctionEnd

;--------------------------------

; The stuff to install
; Installer sections



; mcfx_convolver
Section "MCFX Convolver VST2" mcfx_convolver
    SetOutPath "$INSTDIR"
    ; File /r "..\build-VSC++-windows-x86_64\vst\*.dll"
    File /r "..\build-VSC++-windows-x86_64\vst\mcfx_convolver*.dll"
    ${DisableX64FSRedirection}
SectionEnd


; mcfx_delay
Section "MCFX Delay VST2" mcfx_delay
    SetOutPath "$INSTDIR"
    ; File /r "..\build-VSC++-windows-x86_64\vst\*.dll"
    File /r "..\build-VSC++-windows-x86_64\vst\mcfx_delay*.dll"
    ${DisableX64FSRedirection}
SectionEnd

; mcfx_filter
Section "MCFX Filter VST2" mcfx_filter
    SetOutPath "$INSTDIR"
    ; File /r "..\build-VSC++-windows-x86_64\vst\*.dll"
    File /r "..\build-VSC++-windows-x86_64\vst\mcfx_filter*.dll"
    ${DisableX64FSRedirection}
SectionEnd

; mcfx_gain_delay
Section "MCFX Gain Delay VST2" mcfx_gain_delay
    SetOutPath "$INSTDIR"
    ; File /r "..\build-VSC++-windows-x86_64\vst\*.dll"
    File /r "..\build-VSC++-windows-x86_64\vst\mcfx_gain_delay*.dll"
    ${DisableX64FSRedirection}
SectionEnd

; mcfx_meter
Section "MCFX Meter VST2" mcfx_meter
    SetOutPath "$INSTDIR"
    ; File /r "..\build-VSC++-windows-x86_64\vst\*.dll"
    File /r "..\build-VSC++-windows-x86_64\vst\mcfx_meter*.dll"
    ${DisableX64FSRedirection}
SectionEnd

; libfftw3f-3.dll
Section "FFTW3f library" libfftw3f-3.dll
    SetOutPath "$SYSDIR"
    File "..\win-libs\x64\libfftw3f-3.dll"
SectionEnd
