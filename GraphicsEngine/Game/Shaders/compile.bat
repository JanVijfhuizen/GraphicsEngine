%~dp0/glslc.exe shader.vert -o vert.spv
%~dp0/glslc.exe shader.frag -o frag.spv

%~dp0/glslc.exe shader-dyn.vert -o vert-dyn.spv
%~dp0/glslc.exe shader-dyn.frag -o frag-dyn.spv

%~dp0/glslc.exe shader-sc.vert -o vert-sc.spv
%~dp0/glslc.exe shader-sc.frag -o frag-sc.spv

pause