; echo off
set d=%CD%
set bd=win-x86_64
set id=%d%\..\install
set type=%1
set cmake_bt="Release"
set cmake_gen="Visual Studio 15 2017 Win64"

if "%type%" == "" (
  echo "Usage: build.bat [debug, release]"
  exit /b 2
)

if "%type%" == "debug" (
   set bd="%bd%d"
   set cmake_bt="Debug"
)

if not exist "%d%\%bd%" (       
   mkdir %d%\%bd%
)

cd %d%\%bd%
cmake -DCMAKE_BUILD_TYPE=%cmake_bt% ^
      -DCMAKE_INSTALL_PREFIX=%id% ^
      -G %cmake_gen% ^
      %cmake_opt% ..
      
cmake --build . ^
      --target install ^
      --config %cmake_bt%
      
cd %id%\bin
test-nvidia-decode-v2.exe
cd %d%
