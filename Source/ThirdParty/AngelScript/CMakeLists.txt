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

# Define source files
set(SOURCE_FILES
  source/as_array.h
  source/as_atomic.cpp
  source/as_atomic.h
  source/as_builder.cpp
  source/as_builder.h
  source/as_bytecode.cpp
  source/as_bytecode.h
  source/as_callfunc.cpp
  source/as_callfunc.h
  source/as_compiler.cpp
  source/as_compiler.h
  source/as_configgroup.cpp
  source/as_configgroup.h
  source/as_config.h
  source/as_context.cpp
  source/as_context.h
  source/as_criticalsection.h
  source/as_datatype.cpp
  source/as_datatype.h
  source/as_debug.h
  source/as_gc.cpp
  source/as_gc.h
  source/as_generic.cpp
  source/as_generic.h
  source/as_globalproperty.cpp
  source/as_map.h
  source/as_memory.cpp
  source/as_memory.h
  source/as_module.cpp
  source/as_module.h
  source/as_namespace.h
  source/as_objecttype.cpp
  source/as_objecttype.h
  source/as_outputbuffer.cpp
  source/as_outputbuffer.h
  source/as_parser.cpp
  source/as_parser.h
  source/as_property.h
  source/as_restore.cpp
  source/as_restore.h
  source/as_scriptcode.cpp
  source/as_scriptcode.h
  source/as_scriptengine.cpp
  source/as_scriptengine.h
  source/as_scriptfunction.cpp
  source/as_scriptfunction.h
  source/as_scriptnode.cpp
  source/as_scriptnode.h
  source/as_scriptobject.cpp
  source/as_scriptobject.h
  source/as_string.cpp
  source/as_string.h
  source/as_string_util.cpp
  source/as_string_util.h
  source/as_symboltable.h
  source/as_texts.h
  source/as_thread.cpp
  source/as_thread.h
  source/as_tokendef.h
  source/as_tokenizer.cpp
  source/as_tokenizer.h
  source/as_typeinfo.cpp
  source/as_typeinfo.h
  source/as_variablescope.cpp
  source/as_variablescope.h
)

if (IOS OR ANDROID OR RPI)
    enable_language (ASM)
    if (IOS)
        set (ASM_FILES source/as_callfunc_arm_xcode.S)
    else ()
        set (ASM_FILES source/as_callfunc_arm_gcc.S)
    endif ()
elseif (MSVC AND URHO3D_64BIT)
    enable_language (ASM_MASM)
    # Commented out due to cmake bug - todo: Recheck if this workaround is still necessary when increasing CMake minimum version
    #set (ASM_FILES source/as_callfunc_x64_msvc_asm.asm)
    # As a temporary workaround
    add_custom_command (OUTPUT as_callfunc_x64_msvc_asm.obj COMMAND ${CMAKE_ASM_MASM_COMPILER} -Fo ${CMAKE_CURRENT_BINARY_DIR}/as_callfunc_x64_msvc_asm.obj -c ${CMAKE_CURRENT_SOURCE_DIR}/source/as_callfunc_x64_msvc_asm.asm DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/as_callfunc_x64_msvc_asm.asm COMMENT "Temporary workaround for MASM")
    set (ASM_FILES ${CMAKE_CURRENT_BINARY_DIR}/as_callfunc_x64_msvc_asm.obj ${CMAKE_CURRENT_SOURCE_DIR}/source/as_callfunc_x64_msvc_asm.asm)
endif ()
list (APPEND SOURCE_FILES ${ASM_FILES})

# Define dependency libs
set (INCLUDE_DIRS include)

# Setup target

add_library(AngelScript STATIC ${SOURCE_FILES} ${INCLUDES})
# Install headers for building and using the Urho3D library (install dependency for Urho3D/Script/APITemplates.h)
install (FILES include/angelscript.h DESTINATION include/ThirdParty/AngelScript)
