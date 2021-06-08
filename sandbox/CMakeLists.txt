add_executable(vull-sandbox Config.cc main.cc VehicleController.cc)
target_compile_features(vull-sandbox PRIVATE cxx_std_20)
target_enable_optimisations(vull-sandbox)
target_link_libraries(vull-sandbox PRIVATE vull-engine)
