@echo off

rem Run this batch file to copy the necessary external DLLs to the bin folder.

copy src\external\renderdoc\lib\dbghelp.dll bin\x64\Debug\dbghelp.dll
copy src\external\renderdoc\lib\renderdoc.dll bin\x64\Debug\renderdoc.dll
copy src\external\optick\lib\x64\debug\OptickCore.dll bin\x64\Debug\OptickCore.dll

copy src\external\renderdoc\lib\dbghelp.dll bin\x64\Release\dbghelp.dll
copy src\external\renderdoc\lib\renderdoc.dll bin\x64\Release\renderdoc.dll
copy src\external\optick\lib\x64\release\OptickCore.dll bin\x64\Release\OptickCore.dll
