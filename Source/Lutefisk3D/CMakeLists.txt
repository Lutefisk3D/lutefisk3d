set(CMAKE_CXX_VISIBILITY_PRESET hidden)

#######################################################################################################################
#												 Engine Core														  #
#######################################################################################################################
set(Lutefisk3D_LINK_LIBRARIES)
set(Lutefisk3D_INCLUDE_DIRS)
set(Lutefisk3D_DEPENDENCIES)
set(Lutefisk3D_COMPONENT_SOURCES)
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
add_library(Lutefisk3D SHARED
    ${MinimalSources}
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
    if(LUTEFISK3D_UI)
        add_subdirectory(UI)
        add_subdirectory(QUI)
    endif()
else()
    set(LUTEFISK3D_2D FALSE)
endif()
if (LUTEFISK3D_IK)
    add_subdirectory(IK)
endif ()
if(LUTEFISK3D_INPUT)
    add_subdirectory(Input)
endif()
if(LUTEFISK3D_SOUND)
    add_subdirectory(Audio)
endif()
if(LUTEFISK3D_NAVIGATION)
    add_subdirectory(Navigation)
endif()
if(LUTEFISK3D_NETWORK)
    add_subdirectory(Network)
endif()
if(LUTEFISK3D_PHYSICS)
    add_subdirectory(Physics)
endif()
if(LUTEFISK3D_2D)
    add_subdirectory(2D)
endif()
if(LUTEFISK3D_SYSTEMUI)
    add_subdirectory(SystemUI)
endif ()

target_sources(Lutefisk3D PRIVATE
    ${Lutefisk3D_COMPONENT_SOURCES}
    ${SOURCE_FILES}
)
target_sources(Lutefisk3D PUBLIC ${HEADER_FILES})
set_target_properties(Lutefisk3D PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_include_directories(Lutefisk3D PRIVATE
    $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>
    ${Lutefisk3D_INCLUDE_DIRS}
)

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
# Propagage all enabled warning to all users of Lutefisk3D
target_compile_options(Lutefisk3D PUBLIC ${LF_CMAKE_CXX_FLAGS})
target_link_libraries(Lutefisk3D PRIVATE
    Threads::Threads
    ${CMAKE_DL_LIBS}
    ${ThirdParty_requisites}
    ${Lutefisk3D_LINK_LIBRARIES}
)

#set_property(TARGET Lutefisk3D PROPERTY CXX_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)
# Add any variables starting with LUTEFISK3D_ as project defines
get_cmake_property(__cmake_variables VARIABLES)
foreach (var ${__cmake_variables})
    if ("${var}" MATCHES "^LUTEFISK3D_")
        if ("${${var}}")
            target_compile_definitions(Lutefisk3D PUBLIC -D${var})
        endif ()
    endif ()
endforeach()
if(NOT LUTEFISK3D_HEADLESS)
    IF(WIN32)
         target_link_libraries(Lutefisk3D PRIVATE winmm imm32 version)
    endif()
endif()
if(LUTEFISK3D_NETWORK)
    target_link_libraries(Lutefisk3D PRIVATE kNet_LIB) # add imported kNet target
endif()
if (BUILD_SHARED_LIBS AND NOT MSVC AND NOT APPLE)
    target_link_libraries(Urho3D PRIVATE -Wl,--whole-archive)
endif ()
if(LUTEFISK3D_PROFILING)
    list(APPEND Lutefisk3D_LINK_LIBRARIES easy_profiler)
endif()

#######################################################################################################################
# Link dependencies
#######################################################################################################################
set (THIRD_PARTY_DEPENDENCIES
#EnTT
    FreeType
    LZ4
    PugiXml
    rapidjson
    StanHull
    STB
    IconFontCppHeaders
    kNet
    ik
    Detour
    DetourCrowd
    DetourTileCache
    Recast
    Box2D
    Bullet
    ImGui
    cr
    nativefiledialog

   ${Lutefisk3D_LINK_LIBRARIES}
)
foreach (dep ${THIRD_PARTY_DEPENDENCIES})
    if (TARGET "${dep}")
        # By linking them as private but inheriting definitions/options/dirs we avoid having
        # to export third party lib targets in a monolithic dll build.
        get_target_property(_TARGET_TYPE ${dep} TYPE)
        if (BUILD_SHARED_LIBS AND ${_TARGET_TYPE} STREQUAL "STATIC_LIBRARY")
            if (MSVC)
                set_property(TARGET Lutefisk3D APPEND_STRING PROPERTY LINK_FLAGS " /WHOLEARCHIVE:${dep}")
            elseif (APPLE)
                set_property(TARGET Lutefisk3D APPEND PROPERTY LINK_LIBRARIES -force_load ${dep})
            endif ()
        endif ()
        get_target_property(VALUE ${dep} INTERFACE_COMPILE_DEFINITIONS)
        if (VALUE)
            target_compile_definitions(Lutefisk3D PUBLIC ${VALUE})
        endif ()
        get_target_property(VALUE ${dep} INTERFACE_COMPILE_OPTIONS)
        if (VALUE)
            target_compile_options(Lutefisk3D PUBLIC ${VALUE})
        endif ()
        get_target_property(VALUE ${dep} INTERFACE_INCLUDE_DIRECTORIES)
        if (VALUE)
            target_include_directories(Lutefisk3D PUBLIC ${VALUE})
        endif ()
        get_target_property(VALUE ${dep} INTERFACE_LINK_LIBRARIES)
        if (VALUE)
            foreach (dep_lib ${VALUE})
                if (NOT TARGET ${dep_lib})
                    target_link_libraries (Lutefisk3D PUBLIC "${dep_lib}")
                endif ()
            endforeach ()
        endif ()
        if (BUILD_SHARED_LIBS)
            target_link_libraries(Lutefisk3D PRIVATE ${dep})
        else ()
            if (${_TARGET_TYPE} STREQUAL "STATIC_LIBRARY")
                list (APPEND DEPENDENCY_TARGET_FILES $<TARGET_FILE:${dep}>)
            endif ()
        endif ()
    endif ()
endforeach()
#if (BUILD_SHARED_LIBS)
    target_compile_definitions(Lutefisk3D PRIVATE -DLUTEFISK3D_EXPORTS INTERFACE -DLUTEFISK3D_IMPORTS)
    target_compile_definitions(ImGui PRIVATE -DLUTEFISK3D_EXPORTS INTERFACE -DLUTEFISK3D_IMPORTS)
#else ()
#    target_compile_definitions (Lutefisk3D PUBLIC -DLUTEFISK3D_STATIC)
#endif ()
#cotire(Lutefisk3D)
#target_compile_definitions(Lutefisk3D PRIVATE -DLUTEFISK_PRECOMPILED_HEADERS)

#######################################################################################################################
#									install																			  #
#######################################################################################################################

install(TARGETS Lutefisk3D EXPORT lutefis3d-targets
            RUNTIME DESTINATION bin
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
#            INCLUDES DESTINATION include
)

install(EXPORT lutefis3d-targets DESTINATION lib/Lutefisk3D)
install(FILES ${MinimalHeaders} DESTINATION include/Lutefisk3D )
install(DIRECTORY ${PROJECT_BINARY_DIR}/ThirdParty_Builds/bin/ DESTINATION bin)

get_target_property(TARGET_SOURCES Lutefisk3D SOURCES)
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${TARGET_SOURCES})
