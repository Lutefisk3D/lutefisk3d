set(INCLUDES
    AudioDefs.h
    Audio.h
    BufferedSoundStream.h
    OggVorbisSoundStream.h
    Sound.h
    SoundListener.h
    SoundSource3D.h
    SoundSource.h
    SoundStream.h
)
set(SOURCE
    Audio.cpp
    BufferedSoundStream.cpp
    OggVorbisSoundStream.cpp
    Sound.cpp
    SoundListener.cpp
    SoundSource3D.cpp
    SoundSource.cpp
    SoundStream.cpp
)
add_library(Lutefisk3D_Audio STATIC ${SOURCE} ${INCLUDES} $<TARGET_OBJECTS:STB> )

target_include_directories(Lutefisk3D_Audio PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Audio PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Audio PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(Lutefisk3D_Audio SDL2_LIB)

set_property(TARGET Lutefisk3D_Audio PROPERTY INTERFACE_LINK_LIBRARIES
    Lutefisk3D_Core
    SDL2_LIB
)

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
