# meshtool

Small mesh processing tool that is easy for me to tweak. I needed a fast viewer of large OBJ files, so currently it just contains a pretty fast, but quite featureless, loader of OBJ files in addition to a generic viewer. Features and tools will come as I need them.

It also doubles as my Vulkan playgrond. Currently includes:
- Plain simple pull-from-storage-buffer renderer.
- A basic VK_NVX_raytracing-based path-trace renderer.
- A basic VK_NV_mesh_shader renderer.

## Building dependencies

Clone the git repo, and update submodules
```
git submodule update --init --recursive
```
Also, the Vulkan SDK is required, at least version 1.1.85.

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
