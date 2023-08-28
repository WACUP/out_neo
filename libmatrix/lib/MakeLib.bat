@echo off
call "%VS110COMNTOOLS%\VsDevCmd.bat"

set NAME=libvlccore

set DLL="C:\Projets\Fritivi\Fritivi.NET\bin\Debug\%NAME%.dll"
set DEF="%NAME%.def"
set DUMP="%NAME%.dump"
set LIB="%NAME%.lib"
set EXP="%NAME%.exp"

echo EXPORTS> %DEF%
dumpbin /exports /out:%DUMP% %DLL%

for /f "usebackq skip=15 tokens=4,*" %%i in (%DUMP%) do echo %%i>> %DEF%
lib /NOLOGO /def:%DEF% /out:%LIB% /machine:x86
del %DUMP% %DEF% %EXP% /f
