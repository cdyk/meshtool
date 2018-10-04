# meshtool
Meshprocessing tool


## Building dependencies

Clone the git repo, and update submodules
```
git submodule update --init --recursive
```

## GLFW
```
cd libs\glfw
mkdir build
cd build
cmake-gui ..
```
Press "Configure" button and select 64bit VS2017. Open libs\glfw\build\GLFW.sln and build.

