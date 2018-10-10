set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ArrayPtr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataHandle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FlagSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HandleManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Hash.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HashMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HashTable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Ptr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RefCounted.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SmallOrig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SmallVector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Str.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sherwood_map.hpp
)
set(SOURCE
    ${CMAKE_CURRENT_LIST_DIR}/Allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RefCounted.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Str.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sherwood_map.cpp
)
install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Math )
target_sources(Lutefisk3D PRIVATE ${SOURCE} ${INCLUDES})
