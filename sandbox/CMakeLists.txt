vull_executable(vull-sandbox vull::core vull::graphics vull::main vull::physics vull::ui vull::x11-window)
target_sources(vull-sandbox PRIVATE FreeCamera.cc main.cc OrbitCamera.cc)
