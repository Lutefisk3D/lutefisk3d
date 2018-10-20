set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Connection.h
    #HttpRequest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkEvents.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Network.h
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkPriority.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Protocol.h
)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/Connection.cpp
    #HttpRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Network.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NetworkPriority.cpp
)

install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Input )

set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} kNet_LIB PARENT_SCOPE)
set(Lutefisk3D_COMPONENT_SOURCES ${Lutefisk3D_COMPONENT_SOURCES} ${SOURCE} ${INCLUDES} PARENT_SCOPE)
