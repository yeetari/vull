target_sources(vull-tests PRIVATE
    container/perfect_map.cc
    container/vector.cc
    container/work_stealing_queue.cc
    ecs/entity.cc
    json/lexer.cc
    json/parser.cc
    maths/colour.cc
    maths/epsilon.cc
    maths/relational.cc
    shaderc/lexer.cc
    shaderc/parse_errors.cc
    shaderc/parser.cc
    support/enum.cc
    support/string_view.cc
    support/variant.cc
    tasklet/future.cc
    tasklet/simple.cc
    runner.cc)

if(VULL_BUILD_SCRIPT)
    target_sources(vull-tests PRIVATE script/lexer.cc)
endif()

if(VULL_BUILD_UI)
    target_sources(vull-tests PRIVATE ui/units.cc)
endif()
