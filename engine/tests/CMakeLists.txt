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
    script/lexer.cc
    shaderc/lexer.cc
    shaderc/parse_errors.cc
    shaderc/parser.cc
    support/enum.cc
    support/variant.cc
    ui/units.cc
    main.cc)
