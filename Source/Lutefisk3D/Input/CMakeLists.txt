set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Controls.h
    ${CMAKE_CURRENT_SOURCE_DIR}/InputEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/InputConstants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Input.h
)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/Controls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Input.cpp
)

install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Input )

if(UNIT_TESTING)
endif()

set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} glfw PARENT_SCOPE)
set(Lutefisk3D_COMPONENT_SOURCES ${Lutefisk3D_COMPONENT_SOURCES} ${SOURCE} ${INCLUDES} PARENT_SCOPE)
