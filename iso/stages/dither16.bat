@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo   PNG8 RGB + Transparent Alpha Dithering
echo ==========================================
echo.
echo 0-24%% Alpha  = Transparent
echo 25-100%% Alpha = Floyd-Steinberg Gradient
echo.

for /R %%i in (*.png) do (
    echo %%~nxi | findstr /I "temp_orig_" >nul
    if errorlevel 1 (
        echo Processing: %%i

        copy /Y "%%i" "%%~dpi\temp_orig_%%~nxi" >nul

        magick "%%~dpi\temp_orig_%%~nxi" ^
            ( +clone ^
              -alpha extract ^
              -level 25%%,100%%,0%%,100%% ^
              -colors 2 ^
              -dither FloydSteinberg ^
            ) ^
            -alpha off ^
            -colors 15 ^
            -dither FloydSteinberg ^
            -alpha on ^
            -compose CopyAlpha ^
            -composite ^
            "PNG8:%%i"

        del /Q "%%~dpi\temp_orig_%%~nxi"
    )
)

echo.
echo ==========================================
echo Done!
echo ==========================================
pause