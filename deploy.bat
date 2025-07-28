cmake --build build --target asset_baker renderer --config Release
cd "build"
mkdir "deploy"
mkdir "deploy\bin"
mkdir "deploy\assets"
robocopy "Release" "deploy\bin" /mt /z /e
robocopy "assets" "deploy\assets" /mt /z /e
del "deploy\bin\dxc.exe"
del "deploy\bin\dxv.exe"
del "deploy\bin\renderer.exp"
del "deploy\bin\renderer.lib"
if exist "Renderer.zip" del "Renderer.zip"
cd deploy
tar.exe -c -f "../Renderer.zip" "bin"
tar.exe -r -f "../Renderer.zip" "assets"
cd ..
rmdir /s /q "deploy"
cd ..
