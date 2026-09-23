@echo off
rem ============================================================================
rem  Native Engine - Windows rebuild script (NativeEngine.exe + NativeEngine.dll)
rem  Uses g++ if you have one, otherwise Zig (pip install ziglang) which ships
rem  its own complete Windows C++ toolchain. Output lands in bin\ and is used
rem  automatically by NativeEngine.exe and by `python main.py`.
rem ============================================================================
where g++ >nul 2>nul
if %errorlevel%==0 (
    echo [native-engine] building with g++ ...
    g++ -std=c++17 -O3 -ffast-math -s -static -Iengine/src engine/src/ne_app.cpp -mwindows -luser32 -lgdi32 -o bin\NativeEngine.exe
    g++ -std=c++17 -O3 -ffast-math -s -static -shared -Iengine/src engine/src/ne_api.cpp engine/src/ne_app.cpp -luser32 -lgdi32 -o bin\NativeEngine.dll
    goto done
)
py -m ziglang version >nul 2>nul
if %errorlevel%==0 (
    echo [native-engine] building with zig ...
    py -m ziglang c++ -std=c++17 -O3 -ffast-math -s -static -Iengine/src engine/src/ne_app.cpp -mwindows -luser32 -lgdi32 -o bin\NativeEngine.exe
    py -m ziglang c++ -std=c++17 -O3 -ffast-math -s -static -shared -Iengine/src engine/src/ne_api.cpp engine/src/ne_app.cpp -luser32 -lgdi32 -o bin\NativeEngine.dll
    goto done
)
echo No compiler found. Either install MSYS2 mingw-w64 gcc, or:
echo     pip install ziglang
echo (Zig includes a full C++ toolchain - no Visual Studio needed.)
exit /b 1
:done
if exist bin\NativeEngine.exe (
    echo Built bin\NativeEngine.exe - double-click it to play NEON RUNNER.
) else (
    echo Build failed. Full error output above.
)
