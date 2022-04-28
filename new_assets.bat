@echo off
set curpath=%~dp0
echo %curpath%\res\n_assert
echo %1
set sourceDir=%curpath%\res\n_assert
set tarDir=.\%1

mkdir %1
xcopy %sourceDir% %tarDir% /s/y

SET SourceFile1=%curpath%\core\bin\server\luacluster.exe
SET SourceFile2=%curpath%\core\bin\server\luacluster_d.exe
if exist %SourceFile1% (
echo %SourceFile1%>%tarDir%\start_server.bat
) else (
echo %SourceFile2%>%tarDir%\start_server.bat
)