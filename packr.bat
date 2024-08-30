@echo off
robocopy "GraphicsEngine/Game/Art" "GraphicsEngine/x64/Release/Art" /mir
robocopy "GraphicsEngine/Game/Audio" "GraphicsEngine/x64/Release/Audio" /mir
robocopy "GraphicsEngine/Game/Shaders" "GraphicsEngine/x64/Release/Shaders" /mir
robocopy "GraphicsEngine/x64/Release" "GraphicsEngine/x64/UntitledCardGame" /mir
powershell -Command "Compress-Archive 'GraphicsEngine/x64/UntitledCardGame' cardgame-release.zip -force"
rmdir /s /q "GraphicsEngine/x64/UntitledCardGame"