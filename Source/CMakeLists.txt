add_subdirectory(ThirdParty) # add required third party libraries to the build

INCLUDE_DIRECTORIES(ThirdParty)
INCLUDE_DIRECTORIES(ThirdParty/cereal/include)
add_subdirectory(Lutefisk3D)
