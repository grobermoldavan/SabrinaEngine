@echo off

call :get_csi

set buildFolderDir=%cd%\builds\examples_win32_debug\

call :message "[MESSAGE] Build example exes"
for /r %cd%\examples %%f in (*.cpp) do (
    if %%~nf == main (
        call :build %%f %buildFolderDir%
    )
)

EXIT /B 0

:build
    set projectName=%~p1
    set projectName=%projectName:~0, -1%
    for %%g in ("%projectName%") do set projectName=%%~nxg

    set sourceFolderDir=%~p1
    set buildFolderDir=%~2\%projectName%
    set engineInclude="."

    call :message " -------------------------------------------- %projectName% --------------------------------------------"
    call engine_builder\win_builder "%projectName%" "%sourceFolderDir%" "%buildFolderDir%" "%engineInclude%" msvc debug exe_only
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
