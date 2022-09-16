@echo off

call :get_csi

set bat_file_dir=%cd%
set build_folder_examples=%bat_file_dir%\win32_debug\

call :message "[MESSAGE] Bat file dir: %bat_file_dir%"
call :message "[MESSAGE] Build dir: %build_folder_examples%"
rmdir /s /q %build_folder_examples%
md %build_folder_examples%

call :message "[MESSAGE] Call some msvc specific stuff"
call vcvars64

cd %bat_file_dir%\examples

call :message "[MESSAGE] Build example exes"
for /r %%f in (*.cpp) do if %%~nf == main call :build_exe %build_folder_examples% %%f %bat_file_dir%

call :message "[MESSAGE] Copy default assets"
md %build_folder_examples%\assets\default
xcopy %bat_file_dir%\engine\assets %build_folder_examples%\assets\default /s /e /y

call :message "[MESSAGE] Copy example assets"
md %build_folder_examples%\assets\application
for /r %%f in (*.cpp) do (
    call :message %%f
    if %%~nf == main (
        if exist %%~pfassets (
            xcopy %%~pfassets %build_folder_examples%\assets\application /s /e
        )
    )
)

call :message "[MESSAGE] Build shaders"
cd %build_folder_examples%\assets
for /r %%f in (\*.vert) do glslangValidator %%f -o %%f.spv -V -S vert
for /r %%f in (\*.frag) do glslangValidator %%f -o %%f.spv -V -S frag
for /r %%f in (\*.comp) do glslangValidator %%f -o %%f.spv -V -S comp
for /r %%f in (\*.spv) do spirv-dis %%f -o %%f.dis

EXIT /B %ERRORLEVEL%

:build_exe
    set build_exe_target_folder=%~1
    set build_exe_source_file_path=%~2
    set build_exe_result_file_name=%~p2
    set build_exe_result_file_name=%build_exe_result_file_name:~0, -1%
    for %%f in ("%build_exe_result_file_name%") do set build_exe_result_file_name=%%~nxf
    set build_exe_engine_include_path=%~3

    call :message "[MESSAGE] build exe %build_exe_source_file_path%"

    call cl /Fd%%build_exe_target_folder%%%build_exe_result_file_name%.pdb /Fe%%build_exe_target_folder%%%build_exe_result_file_name%.exe /Fo%%build_exe_target_folder%%%build_exe_result_file_name%.obj ^
    -Z7 -Od -EHsc -MT ^
    %build_exe_source_file_path% ^
    /I "." /I "%VK_SDK_PATH%\Include" /I %build_exe_engine_include_path% /DSE_DEBUG /DSE_VULKAN ^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset /GR-^
    kernel32.lib user32.lib ^
    /link /DEBUG:FULL /OUT:%%build_exe_target_folder%%%build_exe_result_file_name%.exe

    del %build_exe_target_folder%\*.ilk
    del %build_exe_target_folder%\*.obj
EXIT /B 0

REM :get_csi and :message are taken from https://stackoverflow.com/questions/57736435/batch-file-to-change-color-of-text-depending-on-output-string-from-log-file
:get_csi
    echo 1B 5B>CSI.hex
    Del CSI.bin >NUL 2>&1
    certutil -decodehex CSI.hex CSI.bin >NUL 2>&1
    Set /P CSI=<CSI.bin
Goto :Eof

:message
    Echo(%CSI%32m%~1%CSI%0m
Goto :Eof
