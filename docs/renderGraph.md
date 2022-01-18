## RenderGraph
****
### 1. Example
```cpp
RenderGraph graph;
void Init()
{
    // Define backbuffer
    PhysicalResource backbufferRes;
    backbufferRes.width = swapchainW;
    backbufferRes.height = swapchainH;
    graph.SetBackBufferDim(backBufferRes);

    Attachment output;
    auto& mainPass = graph.AddPass("main", QUEUE_GRAPHICS);
    mainPass.WriteTexture("output", output);
    mainPass.SetBuildRenderPassCallback([&](GPU::CommandList cmd)
    {
        cmd.SetProgram("vert.glsl", "frag.glsl");
        Image::DrawFullScreen(cmd);
    });

    graph.SetBackbufferSource("output");
    graph.Bake();
}

void Render()
{
    auto& device = GPU::GetDevice();
    JobSystem::JobHandle handle;
    graph.Render(device, &handle);
    JobSystem::Wait(handle);
}

```

### 2. Data structures
* RenderResource
    * WritenPasses/ReadPasses
    * RenderTextureResource
    * RenderBufferResource
* RenderPass
    * InputResources
    * OutputResources
* RenderGraph
    * PhysicalResource
    * PhysicalPass

### 3. Bake graph
1. Filter and reorder render passses
    * 根据资源Read/Write依赖排序，从无依赖的Pass排序到输出BackBuffer的Pass
2. Build physical resources
3. Build physical passes
4. Build transient resources
5. Build GPU::RenderPassInfos
6. Build logical barriers
7. Build physical barriers

### 4. Render graph
1. Setup GPU::Attachments
