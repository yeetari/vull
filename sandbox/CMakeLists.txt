vull_executable(vull-sandbox vull::core vull::graphics vull::main vull::physics vull::ui vull::x11-window)
target_sources(vull-sandbox PRIVATE free_camera.cc main.cc orbit_camera.cc)
vull_depend_builtin(vull-sandbox)
