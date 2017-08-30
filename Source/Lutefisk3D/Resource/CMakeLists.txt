set(INCLUDES
    BackgroundLoader.h
    Image.h
    JSONFile.h
    JSONValue.h
    Resource.h
    ResourceCache.h
    XMLElement.h
    XMLFile.h
    Localization.h
    ResourceEvents.h
)
set(SOURCE
    BackgroundLoader.cpp
    Decompress.cpp
    Resource.cpp
    ResourceCache.cpp
    Localization.cpp
    Image.cpp
    JSONFile.cpp
        JSONValue.cpp
    PListFile.cpp
    XMLFile.cpp
        XMLElement.cpp
    ResourceEvents.cpp
)


add_library(Lutefisk3D_Resource OBJECT ${SOURCE} ${INCLUDES})
add_dependencies(Lutefisk3D_Resource PugiXml rapidjson SDL2_LIB)
SET(ThirdParty_requisites ${ThirdParty_requisites} PugiXml PARENT_SCOPE)

set_property(TARGET Lutefisk3D_Resource PROPERTY INTERFACE_LINK_LIBRARIES
    SDL2_LIB
    Lutefisk3D_IO # for FileWatcher
)

target_include_directories(Lutefisk3D_Resource PRIVATE
    $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt5::Gui,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(Lutefisk3D_Resource PRIVATE
    $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>
    $<TARGET_PROPERTY:Qt5::Gui,INTERFACE_COMPILE_DEFINITIONS>
)
set_property(TARGET Lutefisk3D_Resource PROPERTY POSITION_INDEPENDENT_CODE ON)
