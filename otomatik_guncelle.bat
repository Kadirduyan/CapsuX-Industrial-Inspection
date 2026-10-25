@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ========================================================
echo   CapsuX - Otomatik GitHub Guncelleme Araci
echo ========================================================
echo.

git add .

git diff --cached --quiet
if %errorlevel% equ 0 (
    echo [BILGI] Yeni bir degisiklik bulunamadi. Mevcut commitler kontrol ediliyor...
) else (
    for /f "tokens=1-3 delims=./- " %%a in ("%date%") do set TODAY=%%a.%%b.%%c
    set COMMIT_MSG=Otomatik guncelleme (%date% - %time:~0,8%)
    echo [ISLEM] Degisiklikler kaydediliyor...
    git commit -m "Otomatik güncelleme: %date% %time:~0,8%"
)

echo.
echo [ISLEM] GitHub'a gonderiliyor (origin main)...
git push -u origin main

if %errorlevel% equ 0 (
    echo.
    echo [BASARILI] Tum degisiklikler GitHub'a basariyla gonderildi!
) else (
    echo.
    echo [HATA] Push islemi sirasinda bir sorun olustu.
)

echo.
pause
