
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
if exist %buildDirPath% (
    call :message "[engine builder] clearing build directory"
    rmdir /s %buildDirPath%
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
    set build_exe_source_file_path=%~1
    set build_exe_target_folder=%~2\
    set build_exe_engine_include_path=%~3
    set build_exe_file_name=%~4

    call :message "[msvc debug exe] target folder       : %build_exe_target_folder%"
    call :message "[msvc debug exe] source file path    : %build_exe_source_file_path%"
    call :message "[msvc debug exe] executable name     : %build_exe_file_name%"
    call :message "[msvc debug exe] engine include path : %build_exe_engine_include_path%"

    where cl
    if %ERRORLEVEL% NEQ 0 (
        call :message "[msvc debug exe] calling vcvars64"
        call vcvars64
    ) else (
        call :message "[msvc debug exe] vcvars64 is already called"
    )
    
    call cl /Fd"%%build_exe_target_folder%%%build_exe_file_name%.pdb" /Fe"%%build_exe_target_folder%%%build_exe_file_name%.exe" /Fo"%%build_exe_target_folder%%%build_exe_file_name%.obj" ^
    -Z7 -Od -EHsc -MT ^
    %build_exe_source_file_path% ^
    /I "%VK_SDK_PATH%\Include" /I %build_exe_engine_include_path% /DSE_DEBUG /DSE_VULKAN ^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset /GR-^
    kernel32.lib user32.lib ^
    /link /DEBUG:FULL /OUT:"%%build_exe_target_folder%%%build_exe_file_name%.exe"

    del %build_exe_target_folder%\*.ilk
    del %build_exe_target_folder%\*.obj
Goto :Eof

@REM =====================================================================================
@REM BUILD EXE MSVC RELEASE
@REM =====================================================================================
:build_exe_msvc_release
    set build_exe_source_file_path=%~1
    set build_exe_target_folder=%~2\
    set build_exe_engine_include_path=%~3
    set build_exe_file_name=%~4

    call :message "[msvc release exe] target folder       : %build_exe_target_folder%"
    call :message "[msvc release exe] source file path    : %build_exe_source_file_path%"
    call :message "[msvc release exe] executable name     : %build_exe_file_name%"
    call :message "[msvc release exe] engine include path : %build_exe_engine_include_path%"

    where cl
    if %ERRORLEVEL% NEQ 0 (
        call :message "[msvc release exe] calling vcvars64"
        call vcvars64
    ) else (
        call :message "[msvc release exe] vcvars64 is already called"
    )

    call cl /Fd"%%build_exe_target_folder%%%build_exe_file_name%.pdb" /Fe"%%build_exe_target_folder%%%build_exe_file_name%.exe" /Fo"%%build_exe_target_folder%%%build_exe_file_name%.obj" ^
    -O2 -EHsc -MT ^
    %build_exe_source_file_path% ^
    /I "." /I "%VK_SDK_PATH%\Include" /I %build_exe_engine_include_path% /DSE_VULKAN ^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset /GR-^
    kernel32.lib user32.lib ^
    /link /DEBUG:NONE /OUT:"%%build_exe_target_folder%%%build_exe_file_name%.exe"

    
    del %build_exe_target_folder%\*.ilk
    del %build_exe_target_folder%\*.obj
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
