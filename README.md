# Vulkan-Test  (WIP)
一个以学习为目的Vulkan渲染沙盒，用于学习Vulkan相关知识，以及基于Vulkan后端来构建一个先进的渲染端架构  
Vulkan-test is a vulkan rendering sandbox.It's used to understand how things work and has become my go-to place for experiment.

  - [How to build](#How-to-build)
  - [Features](#Features)
  - [Roadmaps](#Roadmap)

## How to build
### Windows
#### 1. Setting up the environment
Vulkan-Test currently uses Vulkan as a rendering backend.
As a result we have to make sure that we set up the right environment for it. 
* Vulkan sdk
* Microsoft Visual C++ Redistributable for Visual Studio 2019

#### 2. Use cjing-build-system to build
Is very easy to build.  
1. We click and run "build.cmd" in order for a Visual Studio solution to be generated in "app/build"
2. Then open "VulkanTest.sln"

## Features
* Vulkan backend
* Render graph
* Fiber based jobsystem
* basic core lib
* Archetype based ecs
* Easy to build (use custom build system)

## Roadmap
v0.0.0-v0.1.0
| Feature  | Completion | Notes |
| :-----| ----: | :----: |
| Vulkan backend | 100% | Basic vulkan backend |
| Render graph | 100% | Complete | 
| ECS | 80% | Need to optimize | 
| Base render path | 10% | doing | 
| Forward+ shading | 5% | doing | 
| Scene | 5% | Use ecs | 
| GPU particles | 0% | todo | 
| Editor | 0% | todo. Based on imgui | 

## Dependencies
* dxcompiler
* glfw
* glm
* spriv_reflect
* stb
* volk