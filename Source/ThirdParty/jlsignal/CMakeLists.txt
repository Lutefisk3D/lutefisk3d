IF(MSVC)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DWIN32 -DNOMINMAX -D_WIN32_WINNT=0x501)
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces -Wno-unused-variable -Wno-reorder -std=c++11" )
ENDIF()
set(jlsignal_INCLUDES
    Signal.h
    SignalBase.h
    FastFunc.h
    SignalDefinitions.h
    DoublyLinkedList.h
    ObjectPool.h
    ObjectPoolScopedAllocator.h
    Utils.h
    ScopedAllocator.h
    StaticSignalConnectionAllocators.h
)
add_library(jlsignal STATIC
    ObjectPool.cpp
    SignalBase.cpp
    ${jlsignal_INCLUDES}
    )
set_property(TARGET jlsignal PROPERTY POSITION_INDEPENDENT_CODE ON)
install(TARGETS jlsignal ARCHIVE DESTINATION lib)
install(FILES ${jlsignal_INCLUDES} DESTINATION include/jlsignal)
