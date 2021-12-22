@echo off

call :get_csi

set bat_file_dir=%cd%
set build_folder_examples=%bat_file_dir%\win32_release\

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
for /r %%f in (subsystems\*.c) do call :build_single_subsystem %build_folder_examples%\subsystems\application\ %%f %bat_file_dir%

call :message "[MESSAGE] Build example exes"
for /r %%f in (*.c) do if %%~nf == main call :build_exe %build_folder_examples% %%f %bat_file_dir%

call :message "[MESSAGE] Copy default assets"
md %build_folder_examples%\assets\default
xcopy %bat_file_dir%\engine\assets %build_folder_examples%\assets\default /s /e /y

call :message "[MESSAGE] Copy example assets"
md %build_folder_examples%\assets\application
for /r %%f in (*.c) do if %%~nf == main (
    if exist %%~pfassets (
        xcopy %%~pfassets %build_folder_examples%\assets\application /s /e
    )
)

call :message "[MESSAGE] Build shaders"
cd %build_folder_examples%\assets
for /r %%f in (\*.vert) do (
    glslangValidator %%f -o %%f.spv -V -S vert
    del %%f
)
for /r %%f in (\*.frag) do (
    glslangValidator %%f -o %%f.spv -V -S frag
    del %%f
)

EXIT /B %ERRORLEVEL%

:build_subsystems
    set build_subsystems_target_folder=%~1
    set build_subsystems_source_folder=%~2
    set build_subsystems_engine_include_path=%~3

    md %build_subsystems_target_folder%
    for %%f in (%build_subsystems_source_folder%*.c) do call :build_dll %%f %build_subsystems_target_folder% %build_subsystems_engine_include_path%

    del *.exp
    del *.ilk
    del *.lib
    del *.obj
    del *.pdb

    del %build_subsystems_target_folder%\*.exp
    del %build_subsystems_target_folder%\*.ilk
    del %build_subsystems_target_folder%\*.lib
    del %build_subsystems_target_folder%\*.obj
    del %build_subsystems_target_folder%\*.pdb
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
    del *.pdb

    del %build_single_subsystem_target_folder%\*.exp
    del %build_single_subsystem_target_folder%\*.ilk
    del %build_single_subsystem_target_folder%\*.lib
    del %build_single_subsystem_target_folder%\*.obj
    del %build_single_subsystem_target_folder%\*.pdb
EXIT /B 0

:build_dll
    set build_dll_source_file_path=%~1
    set build_dll_file_name=%~n1
    set build_dll_target_folder=%~2
    set build_dll_additional_include_path=%~3

    call :message "[MESSAGE] build dll %build_dll_source_file_path%"

    call cl /Fo%%build_dll_target_folder%%%build_dll_file_name%.obj ^
    -O2 -EHsc -MT ^
    %~1 ^
    /LD /I "." /I "%VK_SDK_PATH%\Include" /I %build_dll_additional_include_path% ^
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

    call cl /Fe%%build_exe_target_folder%%%build_exe_result_file_name%.exe /Fo%%build_exe_target_folder%%%build_exe_result_file_name%.obj ^
    -O2 -EHsc -MT ^
    %build_exe_source_file_path% ^
    /I "." /I "%VK_SDK_PATH%\Include" /I %build_exe_engine_include_path% ^
    kernel32.lib user32.lib ^
    /link /DEBUG:FULL /OUT:%%build_exe_target_folder%%%build_exe_result_file_name%.exe

    del %build_exe_target_folder%\*.ilk
    del %build_exe_target_folder%\*.obj
    del %build_exe_target_folder%\*.pdb
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
