#
# Copyright (c) 2008-2015 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

include_directories(include)

add_definitions(-DHAVE_STDINT_H)

set(SOURCE_FILES
asm-bits.c
cpuid_main.c
libcpuid_util.c
rdtsc.c
recog_amd.c
recog_intel.c
)
set(INCLUDES
asm-bits.h
libcpuid_constants.h
libcpuid.h
libcpuid_util.h
rdtsc.h
recog_amd.h
recog_intel.h
)
# Define generated source files
if (MSVC AND URHO3D_64BIT)
    enable_language (ASM_MASM)
    # Commented out due to cmake bug
    #set (ASM_FILES masm-x64.asm)
    # As a temporary workaround
    add_custom_command (OUTPUT masm-x64.obj COMMAND ${CMAKE_ASM_MASM_COMPILER} -Fo ${CMAKE_CURRENT_BINARY_DIR}/masm-x64.obj -c ${CMAKE_CURRENT_SOURCE_DIR}/masm-x64.asm DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/masm-x64.asm COMMENT "Temporary workaround for MASM") 
    set (ASM_FILES ${CMAKE_CURRENT_BINARY_DIR}/masm-x64.obj ${CMAKE_CURRENT_SOURCE_DIR}/masm-x64.asm)
endif ()

add_library(LibCpuId OBJECT ${SOURCE_FILES} ${INCLUDES})
# Install headers for building and using the Urho3D library
install (FILES ${INCLUDES} DESTINATION include/ThirdParty/LibCpuId)
