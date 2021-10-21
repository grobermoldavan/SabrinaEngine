@echo off

call :get_csi

set build_folder=.\win32_debug\

call :message "[MESSAGE] Build path: %build_folder%"
md %build_folder%

call :message "[MESSAGE] Call some msvc specific stuff"
call vcvars64

call :message "[MESSAGE] Build default subsystems"
for %%f in (engine\subsystems\*.c) do call :build_dll %%f

call :message "[MESSAGE] Build user subsystems"
for %%f in (user_application\subsystems\*.c) do call :build_dll %%f

call :message "[MESSAGE] Build main exe"
call :build_exe user_application/main.c

EXIT /B %ERRORLEVEL%

:build_exe
    call cl /Fd%%build_folder%%%~n1.pdb /Fe%%build_folder%%%~n1.exe /Fo%%build_folder%%%~n1.obj ^
    -Z7 -Od -EHsc -MT ^
    %~1 ^
    /I "." /DSE_DEBUG ^
    kernel32.lib user32.lib ^
    /link /DEBUG:FULL /OUT:%%build_folder%%%~n1.exe
EXIT /B 0

:build_dll
    call :message "[MESSAGE] build dll %~1"

    call cl /Fd%%build_folder%%%~n1.pdb /Fo%%build_folder%%%~n1.obj ^
    -Z7 -Od -EHsc -MT ^
    %~1 ^
    /LD /I "." /DSE_DEBUG ^
    kernel32.lib user32.lib ^
    /link /DEBUG:FULL /OUT:%%build_folder%%%~n1.dll

    @REM For some reason we .exp and .lib files are create in the root folder...
    move *.exp %build_folder%
    move *.lib %build_folder%
EXIT /B 0

@REM get_csi and message are taken from https://stackoverflow.com/questions/57736435/batch-file-to-change-color-of-text-depending-on-output-string-from-log-file
:get_csi
echo 1B 5B>CSI.hex
Del CSI.bin >NUL 2>&1
certutil -decodehex CSI.hex CSI.bin >NUL 2>&1
Set /P CSI=<CSI.bin
Goto :Eof

:message
Echo(%CSI%32m%~1%CSI%0m
Goto :Eof