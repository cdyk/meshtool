#pragma once
#include <list>
#include "Common.h"

struct RenderMesh;
struct Vec4f;
struct Mat4f;
struct Mat3f;

class Renderer* main_VulkanInit(Logger l, GLFWwindow* window, uint32_t w, uint32_t h);

void main_VulkanResize(uint32_t w, uint32_t h);

void main_VulkanStartFrame();

void main_VulkanRender(uint32_t w, uint32_t h, Vector<RenderMesh*>& renderMeshes, const Vec4f& viewerViewport, const Mat4f& P, const Mat4f& M);

void main_VulkanPresent();

void main_VulkanCleanup();
