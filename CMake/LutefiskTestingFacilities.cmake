option(UNIT_TESTING "Enable building of unit tests ?" OFF)
if(UNIT_TESTING)
    enable_testing()
    find_package(Qt5Test 5.5.0)

    macro(add_lutefisk_test name)
        add_executable(${name} Tests/${name}.cpp)
        add_test(${name} ${name})

        target_link_libraries(${name} Lutefisk3D)
        qt5_use_modules(${name} Core Gui Test)
        set_target_properties(${name} PROPERTIES AUTOMOC TRUE)
    endmacro()
endif()
