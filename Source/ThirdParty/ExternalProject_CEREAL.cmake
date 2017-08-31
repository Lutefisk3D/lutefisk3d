ExternalProject_Add(
   cereal_BUILD
   URL ""
   SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cereal
   UPDATE_COMMAND ""
   INSTALL_DIR ${ThirdParty_Install_Dir}
   CMAKE_ARGS -DJUST_INSTALL_CEREAL:BOOL=ON -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_STAGING_PREFIX:PATH=${ThirdParty_Install_Dir}
   CMAKE_GENERATOR "${CMAKE_GENERATOR}"
   CMAKE_GENERATOR_TOOLSET ${CMAKE_GENERATOR_TOOLSET}

)
add_library(cereal_IMP INTERFACE IMPORTED GLOBAL)
add_dependencies(cereal_IMP cereal_BUILD)
#TODO: cmake workaround
file(MAKE_DIRECTORY ${ThirdParty_Install_Dir}/include)
set_property(TARGET cereal_IMP PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ThirdParty_Install_Dir}/include)
