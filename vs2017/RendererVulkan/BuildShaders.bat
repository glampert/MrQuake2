@echo off

set DXC=%VK_SDK_PATH%\Bin\dxc.exe

:loop
	set inFile=%1
	shift /1
	set outFile=%1
	shift /1

	:: Compile both VS and PS
	%DXC% -spirv -T vs_6_0 -O3 -DVULKAN=1 -E VS_main -Fo %outFile%.vs %inFile%
	%DXC% -spirv -T ps_6_0 -O3 -DVULKAN=1 -E PS_main -Fo %outFile%.ps %inFile%

	:: Check for further batch arguments or break the loop
	IF [%1]==[] (
		goto end
	) ELSE (
		goto loop
	)
:end
