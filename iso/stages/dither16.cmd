@echo off
REM dither16.cmd - Dither and index chargen PNGs to 16 colors for PS1 4bpp
REM Usage: dither16.cmd <input_dir> [output_dir]
REM input_dir - directory containing chargen output PNGs (e.g. ycbu/out_ycbu/bf)
REM output_dir - optional, defaults to overwriting input dir
REM Requires: ImageMagick (magick command)
REM
REM Example: dither16.cmd ycbu/out_ycbu/bf

setlocal enabledelayedexpansion

set "SRC=%~1"
if "%SRC%"=="" (
    echo Usage: %~nx0 ^<input_dir^> [output_dir]
    exit /b 1
)

if not exist "%SRC%" (
    echo Error: directory not found: %SRC%
    exit /b 1
)

set "DST=%~2"
if "%DST%"=="" set "DST=%SRC%"

rem Ensure output directory exists
if not exist "%DST%" mkdir "%DST%"

for %%f in ("%SRC%\*.png") do (
    set "NAME=%%~nxf"
    echo Dithering !NAME! to 16 colors...
    magick "%%f" -channel A -threshold 80%% +channel -dither None -colors 16 "%DST%\!NAME!"
)

echo Done.
