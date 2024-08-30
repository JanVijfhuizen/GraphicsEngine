@echo off
robocopy "GraphicsEngine/Game/Art" "GraphicsEngine/x64/Debug/Art" /mir
robocopy "GraphicsEngine/Game/Audio" "GraphicsEngine/x64/Debug/Audio" /mir
robocopy "GraphicsEngine/Game/Shaders" "GraphicsEngine/x64/Debug/Shaders" /mir
robocopy "GraphicsEngine/x64/Debug" "GraphicsEngine/x64/UntitledCardGame" /mir
powershell -Command "Compress-Archive 'GraphicsEngine/x64/UntitledCardGame' cardgame-debug.zip -force"
rmdir /s /q "GraphicsEngine/x64/UntitledCardGame"