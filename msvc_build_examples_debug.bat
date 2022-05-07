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

call :message "[MESSAGE] Build default subsystems"
call :build_subsystems %build_folder_examples%\subsystems\default\ %bat_file_dir%\engine\subsystems\ %bat_file_dir%

cd %bat_file_dir%\examples

call :message "[MESSAGE] Build example subsysteams"
REM for /r %%f in (subsystems\*.c) do call :build_single_subsystem %build_folder_examples%\subsystems\application\ %%f %bat_file_dir%
for /r %%f in (triangle\subsystems\*.cpp) do call :build_single_subsystem %build_folder_examples%\subsystems\application\ %%f %bat_file_dir%

call :message "[MESSAGE] Build example exes"
REM for /r %%f in (*.c) do if %%~nf == main call :build_exe %build_folder_examples% %%f %bat_file_dir%
for /r %%f in (triangle\*.cpp) do if %%~nf == main call :build_exe %build_folder_examples% %%f %bat_file_dir%

call :message "[MESSAGE] Copy default assets"
md %build_folder_examples%\assets\default
xcopy %bat_file_dir%\engine\assets %build_folder_examples%\assets\default /s /e /y
REM 
REM call :message "[MESSAGE] Copy example assets"
REM md %build_folder_examples%\assets\application
REM for /r %%f in (*.c) do if %%~nf == main (
REM     if exist %%~pfassets (
REM         xcopy %%~pfassets %build_folder_examples%\assets\application /s /e
REM     )
REM )
REM
call :message "[MESSAGE] Build shaders"
cd %build_folder_examples%\assets
for /r %%f in (\*.vert) do glslangValidator %%f -o %%f.spv -V -S vert
for /r %%f in (\*.frag) do glslangValidator %%f -o %%f.spv -V -S frag
for /r %%f in (\*.comp) do glslangValidator %%f -o %%f.spv -V -S comp
for /r %%f in (\*.spv) do spirv-dis %%f -o %%f.dis

EXIT /B %ERRORLEVEL%

:build_subsystems
    set build_subsystems_target_folder=%~1
    set build_subsystems_source_folder=%~2
    set build_subsystems_engine_include_path=%~3

    md %build_subsystems_target_folder%
    for %%f in (%build_subsystems_source_folder%*.cpp) do call :build_dll %%f %build_subsystems_target_folder% %build_subsystems_engine_include_path%

    del *.exp
    del *.ilk
    del *.lib
    del *.obj

    del %build_subsystems_target_folder%\*.exp
    del %build_subsystems_target_folder%\*.ilk
    del %build_subsystems_target_folder%\*.lib
    del %build_subsystems_target_folder%\*.obj
EXIT /B 0

:build_single_subsystem
    set build_single_subsystem_target_folder=%~1
    set build_single_subsystem_source_file=%~2
    set build_single_subsystem_engine_include_path=%~3

    md %build_single_subsystem_target_folder%
    call :build_dll %build_single_subsystem_source_file% %build_single_subsystem_target_folder% %build_single_subsystem_engine_include_path%

    del *.exp
    del *.ilk
    del *.lib
    del *.obj

    del %build_single_subsystem_target_folder%\*.exp
    del %build_single_subsystem_target_folder%\*.ilk
    del %build_single_subsystem_target_folder%\*.lib
    del %build_single_subsystem_target_folder%\*.obj
EXIT /B 0

:build_dll
    set build_dll_source_file_path=%~1
    set build_dll_file_name=%~n1
    set build_dll_target_folder=%~2
    set build_dll_additional_include_path=%~3

    call :message "[MESSAGE] build dll %build_dll_source_file_path%"

    call cl /Fd%%build_dll_target_folder%%%build_dll_file_name%.pdb /Fo%%build_dll_target_folder%%%build_dll_file_name%.obj ^
    -Z7 -Od -EHsc -MT ^
    %~1 ^
    /LD /I "." /I "%VK_SDK_PATH%\Include" /I %build_dll_additional_include_path% /DSE_DEBUG ^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset^
    kernel32.lib user32.lib ^
    /link /DEBUG:FULL /OUT:%%build_dll_target_folder%%%build_dll_file_name%.dll

    REM For some reason we .exp and .lib files are created in the root folder...
    move *.exp %build_dll_target_folder%
    move *.lib %build_dll_target_folder%
EXIT /B 0

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
    /I "." /I "%VK_SDK_PATH%\Include" /I %build_exe_engine_include_path% /DSE_DEBUG ^
    /std:c++20 /W4 /wd4201 /wd4324 /wd4100 /wd4505 /utf-8 /validate-charset^
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
