
@REM =====================================================================================
@REM MAIN
@REM =====================================================================================

@echo off
call :get_csi

set currentDir=%cd%
set projectName=%~1
set sourceDirPath=%~2
set buildDirPath=%~3
set engineDirPath=%~4
set compiler=%~5
set optimization=%~6
set buildMode=%~7

call :message "[engine builder] current dir         : %currentDir%"
call :message "[engine builder] project name        : %projectName%"
call :message "[engine builder] source dir path     : %sourceDirPath%"
call :message "[engine builder] build dir path      : %buildDirPath%"
call :message "[engine builder] engine dir path     : %engineDirPath%"
call :message "[engine builder] compiler            : %compiler%"
call :message "[engine builder] optimization        : %optimization%"

@REM ==================== Check compiler options
if "%compiler%"=="msvc" (
    call :message "[engine builder] compiler option check success"
) else (
    call :error "[engine builder] compiler option check fail"
    EXIT /B 0
)

@REM ==================== Check optimization options
if "%optimization%"=="debug" (
    call :message "[engine builder] optimization option check success"
) else if "%optimization%"=="release" (
    call :message "[engine builder] optimization option check success"
) else (
    call :error "[engine builder] optimization option check fail"
    EXIT /B 0
)

@REM ==================== Prepare build directory
if not "%buildMode%"=="exe_only" (
    if exist %buildDirPath% (
        call :message "[engine builder] clearing build directory"
        rmdir /s %buildDirPath%
    )
)
mkdir %buildDirPath%

@REM ==================== Build executable
if "%compiler%"=="gcc" (
    call :error "[engine builder] @TODO : implement gcc compilation"
) else if "%compiler%"=="clang" (
    call :error "[engine builder] @TODO : implement clang compilation"
) else if "%compiler%"=="msvc" (
    if "%optimization%"=="debug" (
        call :build_exe_msvc_debug "%sourceDirPath%\main.cpp" "%buildDirPath%" "%engineDirPath%" "%projectName%"
    ) else if "%optimization%"=="release" (
        call :build_exe_msvc_release "%sourceDirPath%\main.cpp" "%buildDirPath%" "%engineDirPath%" "%projectName%"
    )
)

if "%buildMode%"=="exe_only" (
    EXIT /B 0
)

@REM ==================== Copy default and application assets
md %buildDirPath%\assets\default
xcopy %engineDirPath%\engine\assets %buildDirPath%\assets\default /s /e /y

if exist %sourceDirPath%\assets (
    md %buildDirPath%\assets\application
    xcopy %sourceDirPath%\assets %buildDirPath%\assets\application /s /e /y
)

@REM ==================== Build shaders
for /r %buildDirPath%\assets %%f in (*.vert) do (
    glslangValidator %%f -o %%f.spv -V -S vert
    del %%f
)
for /r %buildDirPath%\assets %%f in (*.frag) do (
    glslangValidator %%f -o %%f.spv -V -S frag
    del %%f
)
for /r %buildDirPath%\assets %%f in (*.comp) do (
    glslangValidator %%f -o %%f.spv -V -S comp
    del %%f
)
for /r %buildDirPath%\assets %%f in (*.spv) do (
    spirv-dis %%f -o %%f.dis
)

EXIT /B 0

@REM =====================================================================================
@REM BUILD EXE MSVC DEBUG
@REM =====================================================================================
:build_exe_msvc_debug
    set buildExeSourceFilePath=%~1
    set buildExeTargetFolder=%~2\
    set buildExeEngineIncludePath=%~3
    set buildExeProjectName=%~4

    call :message "[msvc debug exe] target folder       : %buildExeTargetFolder%"
    call :message "[msvc debug exe] source file path    : %buildExeSourceFilePath%"
    call :message "[msvc debug exe] executable name     : %buildExeProjectName%"
    call :message "[msvc debug exe] engine include path : %buildExeEngineIncludePath%"

    where cl
    if %ERRORLEVEL% NEQ 0 (
        call :message "[msvc debug exe] calling vcvars64"
        call vcvars64
    ) else (
        call :message "[msvc debug exe] vcvars64 is already called"
    )
    
    call cl /Fd"%%buildExeTargetFolder%%%buildExeProjectName%.pdb" /Fe"%%buildExeTargetFolder%%%buildExeProjectName%.exe" /Fo"%%buildExeTargetFolder%%%buildExeProjectName%.obj" ^
    -Z7 -Od -EHsc -MT ^
    %buildExeSourceFilePath% ^
    /I "%VK_SDK_PATH%\Include" /I %buildExeEngineIncludePath% /DSE_DEBUG /DSE_VULKAN^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset /GR-^
    kernel32.lib user32.lib Shell32.lib Shlwapi.lib Pathcch.lib ^
    /link /DEBUG:FULL /OUT:"%%buildExeTargetFolder%%%buildExeProjectName%.exe"

    del %buildExeTargetFolder%\*.ilk
    del %buildExeTargetFolder%\*.obj
Goto :Eof

@REM =====================================================================================
@REM BUILD EXE MSVC RELEASE
@REM =====================================================================================
:build_exe_msvc_release
    set buildExeSourceFilePath=%~1
    set buildExeTargetFolder=%~2\
    set buildExeEngineIncludePath=%~3
    set buildExeProjectName=%~4

    call :message "[msvc release exe] target folder       : %buildExeTargetFolder%"
    call :message "[msvc release exe] source file path    : %buildExeSourceFilePath%"
    call :message "[msvc release exe] executable name     : %buildExeProjectName%"
    call :message "[msvc release exe] engine include path : %buildExeEngineIncludePath%"

    where cl
    if %ERRORLEVEL% NEQ 0 (
        call :message "[msvc release exe] calling vcvars64"
        call vcvars64
    ) else (
        call :message "[msvc release exe] vcvars64 is already called"
    )

    call cl /Fd"%%buildExeTargetFolder%%%buildExeProjectName%.pdb" /Fe"%%buildExeTargetFolder%%%buildExeProjectName%.exe" /Fo"%%buildExeTargetFolder%%%buildExeProjectName%.obj" ^
    -O2 -EHsc -MT ^
    %buildExeSourceFilePath% ^
    /I "." /I "%VK_SDK_PATH%\Include" /I %buildExeEngineIncludePath% /DSE_VULKAN^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset /GR-^
    kernel32.lib user32.lib Shell32.lib Shlwapi.lib Pathcch.lib ^
    /link /DEBUG:NONE /OUT:"%%buildExeTargetFolder%%%buildExeProjectName%.exe"

    
    del %buildExeTargetFolder%\*.ilk
    del %buildExeTargetFolder%\*.obj
Goto :Eof

@REM :get_csi, :message and :error are taken from https://stackoverflow.com/questions/57736435/batch-file-to-change-color-of-text-depending-on-output-string-from-log-file
:get_csi
    echo 1B 5B>CSI.hex
    Del CSI.bin >NUL 2>&1
    certutil -decodehex CSI.hex CSI.bin >NUL 2>&1
    Set /P CSI=<CSI.bin
Goto :Eof

:message
    Echo(%CSI%32m%~1%CSI%0m
Goto :Eof

:error
    Echo(%CSI%31m%~1%CSI%0m
Goto :Eof
