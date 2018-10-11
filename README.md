# meshtool

Small mesh processing tool that is easy for me to tweak. I needed a fast viewer of large OBJ files, so currently it just contains a pretty fast, but quite featureless, loader of OBJ files in addition to a generic viewer. Features and tools will come as I need them.

It also doubles as a playground for me to learn Vulkan.

## Building dependencies

Clone the git repo, and update submodules
```
git submodule update --init --recursive
```
Also, the Vulkan SDK is required.

## GLFW
```
cd libs\glfw
mkdir build
cd build
cmake-gui ..
```
Press "Configure" button and select 64bit VS2017. Open libs\glfw\build\GLFW.sln and build.

## Credits

Developed by Christopher Dyken.

## License

Meshtool is licensed under the MIT license, see LICENSE for more information.
