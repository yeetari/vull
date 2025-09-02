target_sources(vull PRIVATE
    core/application.cc
    core/log.cc
    core/tracing.cc
    ecs/entity.cc
    ecs/world.cc
    ecs2/world.cc
    json/lexer.cc
    json/parser.cc
    json/tree.cc
    maths/colour.cc
    maths/random.cc
    platform/common.cc
    platform/linux.cc
    scene/scene.cc
    scene/transform.cc
    support/args_parser.cc
    support/assert.cc
    support/stream.cc
    support/string.cc
    support/string_builder.cc
    tasklet/fiber.cc
    tasklet/latch.cc
    tasklet/mutex.cc
    tasklet/promise.cc
    tasklet/scheduler.cc
    tasklet/tasklet.cc
    tasklet/x86_64_sysv.S
    vpak/file_system.cc
    vpak/pack_file.cc
    vpak/stream.cc
    vpak/writer.cc)

if(VULL_BUILD_GRAPHICS)
    target_sources(vull PRIVATE
        graphics/default_renderer.cc
        graphics/deferred_renderer.cc
        graphics/frame_pacer.cc
        graphics/material.cc
        graphics/mesh.cc
        graphics/mesh_streamer.cc
        graphics/object_renderer.cc
        graphics/skybox_renderer.cc
        graphics/texture_streamer.cc
        terrain/chunk.cc
        terrain/noise.cc
        terrain/quad_tree.cc
        terrain/terrain.cc
        vulkan/allocator.cc
        vulkan/buffer.cc
        vulkan/command_buffer.cc
        vulkan/context.cc
        vulkan/context_table.cc
        vulkan/fence.cc
        vulkan/image.cc
        vulkan/memory.cc
        vulkan/pipeline.cc
        vulkan/query_pool.cc
        vulkan/queue.cc
        vulkan/render_graph.cc
        vulkan/semaphore.cc
        vulkan/shader.cc
        vulkan/swapchain.cc)
endif()

if(VULL_BUILD_PHYSICS)
    target_sources(vull PRIVATE
        physics/mpr.cc
        physics/physics_engine.cc
        physics/rigid_body.cc
        physics/shape.cc)
endif()

if(VULL_BUILD_SCRIPT)
    target_sources(vull PRIVATE
        script/environment.cc
        script/lexer.cc
        script/parser.cc
        script/value.cc
        script/vm.cc)
endif()

target_sources(vull-shaderc PRIVATE
    shaderc/ast.cc
    shaderc/error.cc
    shaderc/hir.cc
    shaderc/legaliser.cc
    shaderc/lexer.cc
    shaderc/parser.cc
    shaderc/spv_backend.cc
    shaderc/spv_builder.cc
    shaderc/tree.cc)

if(VULL_BUILD_UI)
    target_sources(vull PRIVATE
        ui/layout/box_layout.cc
        ui/layout/pane.cc
        ui/layout/screen_pane.cc
        ui/widget/button.cc
        ui/widget/image_label.cc
        ui/widget/label.cc
        ui/widget/slider.cc
        ui/widget/time_graph.cc
        ui/element.cc
        ui/font.cc
        ui/font_atlas.cc
        ui/painter.cc
        ui/renderer.cc
        ui/tree.cc
        ui/units.cc
        ui/window.cc)
endif()

if(VULL_BUILD_WAYLAND_WINDOW)
    target_sources(vull PRIVATE
        platform/wayland.cc)
endif()

if(VULL_BUILD_X11_WINDOW)
    target_sources(vull PRIVATE
        platform/x11.cc)
endif()

if(VULL_BUILD_WAYLAND_WINDOW OR VULL_BUILD_X11_WINDOW)
    target_sources(vull PRIVATE
        platform/xkb.cc)
endif()
