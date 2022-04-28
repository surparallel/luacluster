SET SourceFile1=BINPATH\core\bin\server\luacluster.exe
SET SourceFile2=BINPATH\core\bin\server\luacluster_d.exe
if exist %SourceFile1% (
SourceFile1
) else (
SourceFile2
)
