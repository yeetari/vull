target_sources(vull-core PRIVATE
    core/Log.cc
    core/Transform.cc
    ecs/Entity.cc
    ecs/World.cc
    maths/Colour.cc
    maths/Random.cc
    platform/Linux.cc
    support/Assert.cc
    support/Stream.cc
    support/String.cc
    support/StringBuilder.cc
    tasklet/Scheduler.cc
    tasklet/Tasklet.cc
    tasklet/Mutex.cc
    tasklet/x86_64_sysv.S
    vpak/FileSystem.cc
    vpak/Reader.cc
    vpak/Writer.cc)

target_sources(vull-graphics PRIVATE
    core/Scene.cc
    graphics/DefaultRenderer.cc
    graphics/DeferredRenderer.cc
    graphics/FramePacer.cc
    graphics/Material.cc
    graphics/Mesh.cc
    graphics/SkyboxRenderer.cc
    terrain/Chunk.cc
    terrain/Noise.cc
    terrain/QuadTree.cc
    terrain/Terrain.cc)

target_sources(vull-json PRIVATE
    json/Lexer.cc
    json/Parser.cc
    json/Tree.cc)

target_sources(vull-main PRIVATE core/Main.cc)

target_sources(vull-physics PRIVATE
    physics/Mpr.cc
    physics/PhysicsEngine.cc
    physics/RigidBody.cc
    physics/Shape.cc)

target_sources(vull-script PRIVATE
    script/Builder.cc
    script/ConstantPool.cc
    script/Lexer.cc
    script/Parser.cc
    script/Vm.cc)

target_sources(vull-ui PRIVATE
    ui/layout/BoxLayout.cc
    ui/layout/Pane.cc
    ui/layout/ScreenPane.cc
    ui/widget/Button.cc
    ui/widget/Label.cc
    ui/widget/Slider.cc
    ui/widget/TimeGraph.cc
    ui/CommandList.cc
    ui/Element.cc
    ui/Font.cc
    ui/FontAtlas.cc
    ui/Renderer.cc
    ui/Tree.cc
    ui/Window.cc)

target_sources(vull-vulkan PRIVATE
    vulkan/Allocator.cc
    vulkan/Buffer.cc
    vulkan/CommandBuffer.cc
    vulkan/Context.cc
    vulkan/ContextTable.cc
    vulkan/DescriptorBuilder.cc
    vulkan/Fence.cc
    vulkan/Image.cc
    vulkan/Pipeline.cc
    vulkan/QueryPool.cc
    vulkan/Queue.cc
    vulkan/RenderGraph.cc
    vulkan/Semaphore.cc
    vulkan/Shader.cc
    vulkan/Swapchain.cc)

target_sources(vull-x11-window PRIVATE
    core/Window.cc)
