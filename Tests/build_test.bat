@echo off
rem Compile et lance le test hors-ligne du moteur (sans JUCE).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
if not exist build_test mkdir build_test
cl /nologo /std:c++17 /EHsc /utf-8 /O2 /W3 /I Source Tests\engine_test.cpp /Fo:build_test\ /Fe:build_test\engine_test.exe
if errorlevel 1 exit /b 1
build_test\engine_test.exe demos
