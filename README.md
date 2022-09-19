# Vulkan-Test  (WIP)
一个基于C++17和Vulkan的游戏引擎，用于学习和功能性的实验。  
  
Vulkan-test is a toy game engine.It's used to understand how things work and has become my go-to place for experiment.

  - [How to build](#How-to-build)
  - [Features](#Features)
  - [Roadmaps](#Roadmap)

## How to build
### Windows
#### 1. Setting up the environment
Vulkan-Test currently uses Vulkan as a rendering backend.
As a result we have to make sure that we set up the right environment for it. 
* Vulkan sdk
* Microsoft Visual C++ Redistributable for Visual Studio 2022

#### 2. Update submodule
```
git submodule update --remote   
```
ECS is a required submodule

#### 3. Use cjing-build-system to build
Is very easy to build.  
1. We click and run "build.cmd" in order for a Visual Studio solution to be generated in "app/build"
2. Then open "VulkanTest.sln"

## Features
* Vulkan backend
* Render graph
* Fiber based jobsystem
* Basic core lib
* Archetype based ecs library
* Easy to build (use custom build system)
* Tiled forward rendering
* Editor and resource importers

## Roadmap
v0.0.0-v0.1.0
| Feature  | Completion | Notes |
| :-----| ----: | :----: |
| Vulkan backend | 100% | Basic vulkan backend (multithreading) |
| Core | 100% | Complete | 
| Render graph | 100% | Complete | 
| ECS | 100% | Support multithreading | 
| Native lua binder | 100% | Basic version | 
| Resource | 100% | Resource pipeline(import and compile) | 
| Forward+ shading | 100% | done | 
| Scene | 60% | Use ecs with jobSystem | 
| Base render path | 75% | doing | 
| Editor | 50% | Basic editor | 
| Graphical features | 20% | doing | 


## Dependencies
* dxcompiler
* glfw
* glm
* spriv_reflect
* stb
* volk
* imGui
* rapidjson