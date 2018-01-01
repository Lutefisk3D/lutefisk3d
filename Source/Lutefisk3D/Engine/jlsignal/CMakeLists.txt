IF(MSVC)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DWIN32 -DNOMINMAX -D_WIN32_WINNT=0x501)
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces -Wno-unused-variable -Wno-reorder -std=c++11" )
ENDIF()
set(jlsignal_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Signal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SignalBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FastFunc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SignalDefinitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DoublyLinkedList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ObjectPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ObjectPoolScopedAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ScopedAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StaticSignalConnectionAllocators.h
)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/ObjectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SignalBase.cpp
)
set(Lutefisk3D_COMPONENT_SOURCES ${Lutefisk3D_COMPONENT_SOURCES} ${SOURCE} ${jlsignal_INCLUDES} PARENT_SCOPE)
install(FILES ${jlsignal_INCLUDES} DESTINATION include/Lutefisk3D/Engine/jlsignal)
