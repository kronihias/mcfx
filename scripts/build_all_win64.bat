set BUILD_DIR="..\build-VSC++-windows-x86_64"

for %%x in (2 4 8 16 24 32 36 50 64 128) do (
	echo
	echo BUILDING %%x CHANNEL VERSION

	pushd %BUILD_DIR%
		cmake .. -DNUM_CHANNELS:STRING=%%x
		"C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\MSBuild.exe" mcfx_plugin_suite.sln /t:Build /p:Configuration=Release
	popd
)
@REM Download Nullsoft's Scriptable Install System (NSIS)
@REM https://sourceforge.net/projects/nsis/
"C:\Program Files (x86)\NSIS\makensis.exe" /V4 mcfx_win64.nsi