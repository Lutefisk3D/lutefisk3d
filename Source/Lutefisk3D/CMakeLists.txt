set(CMAKE_CXX_VISIBILITY_PRESET hidden)

include(GenerateExportHeader)

################# Engine Core ########################
set(Lutefisk3D_LINK_LIBRARIES)
set(Lutefisk3D_INCLUDE_DIRS)
set(Lutefisk3D_DEPENDENCIES)
set(Lutefisk3D_COMPONENT_SOURCES)
add_definitions(-DLUTEFISK_PRECOMPILED_HEADERS)
set(MinimalHeaders
    Core/CoreEvents.h
    Input/InputEvents.h
    Scene/SceneEvents.h
    Graphics/GraphicsEvents.h
)
set(MinimalSources
    Core/CoreEvents.cpp
    Input/InputEvents.cpp
    Scene/SceneEvents.cpp
    Graphics/GraphicsEvents.cpp
    ${MinimalHeaders}
)

add_subdirectory(Container)
add_subdirectory(Core)
add_subdirectory(Math)
add_subdirectory(IO)
add_subdirectory(Resource)
add_subdirectory(Scene)
add_subdirectory(Engine)

if(NOT LUTEFISK3D_HEADLESS)
    add_subdirectory(Graphics)
    if(NOT LUTEFISK3D_UILESS)
        add_subdirectory(UI)
        add_subdirectory(QUI)
    endif()
else()
    set(LUTEFISK3D_2DLESS TRUE)
endif()

if(NOT LUTEFISK3D_INPUTLESS)
    add_subdirectory(Input)
endif()
if(NOT LUTEFISK3D_SOUNDLESS)
    add_subdirectory(Audio)
endif()
if(NOT LUTEFISK3D_NAVIGATIONLESS)
    add_subdirectory(Navigation)
endif()
if(NOT LUTEFISK3D_PHYSICLESS)
    add_subdirectory(Physics)
endif()
if(NOT LUTEFISK3D_2DLESS)
    add_subdirectory(2D)
endif()

INCLUDE_DIRECTORIES(${Lutefisk3D_INCLUDE_DIRS})
add_library(Lutefisk3D SHARED
    ${MinimalSources}
    ${Lutefisk3D_COMPONENT_SOURCES}
)
GENERATE_EXPORT_HEADER(Lutefisk3D)
target_include_directories(Lutefisk3D PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)

qt5_use_modules(Lutefisk3D LINK_PUBLIC Core Gui Widgets)
set_property(TARGET Lutefisk3D PROPERTY POSITION_INDEPENDENT_CODE ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
set_target_properties (Lutefisk3D PROPERTIES LINK_FLAGS "-Wl,--gc-sections") # -Wl,--print-gc-sections
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
set_target_properties (Lutefisk3D PROPERTIES LINK_FLAGS "-Wl,--gc-sections") # -Wl,--print-gc-sections
endif()

set_target_properties(Lutefisk3D PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(Lutefisk3D PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "Core/Lutefisk3D.h")
cotire(Lutefisk3D)

add_dependencies(Lutefisk3D ${Lutefisk3D_LINK_LIBRARIES} ${Lutefisk3D_DEPENDENCIES})

target_link_libraries(Lutefisk3D PRIVATE
    Threads::Threads
    ${CMAKE_DL_LIBS}
    ${ThirdParty_requisites}
    ${Lutefisk3D_LINK_LIBRARIES}
)
#set_property(TARGET Lutefisk3D PROPERTY CXX_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)

if(NOT LUTEFISK3D_HEADLESS)
    IF(WIN32)
         target_link_libraries(Lutefisk3D PRIVATE winmm imm32 version)
    endif()
endif()
if(NOT LUTEFISK3D_NETLESS)
    target_link_libraries(Lutefisk3D PRIVATE kNet_LIB) # add imported kNet target
endif()


install(TARGETS Lutefisk3D EXPORT lutefis3d-targets
            RUNTIME DESTINATION bin
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            INCLUDES DESTINATION include
)

install(EXPORT lutefis3d-targets DESTINATION lib/Lutefisk3D)
install(FILES ${MinimalHeaders} DESTINATION include/Lutefisk3D )
install(FILES ${PROJECT_BINARY_DIR}/Source/Lutefisk3D/lutefisk3d_export.h DESTINATION include/Lutefisk3D)
install(DIRECTORY ${PROJECT_BINARY_DIR}/ThirdParty_Builds/bin/ DESTINATION bin)
