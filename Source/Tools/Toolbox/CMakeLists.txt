#
# Copyright (c) 2018 Rokas Kupstys
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

if (NOT LUTEFISK3D_SYSTEMUI)
    return ()
endif ()
find_package(Qt5 COMPONENTS Gui REQUIRED)
file (GLOB_RECURSE SOURCE_FILES *.cpp *.h *.hpp)
add_library (Toolbox STATIC ${SOURCE_FILES})
target_link_libraries (Toolbox PUBLIC Lutefisk3D Qt5::Gui Qt5::Widgets)
target_compile_definitions(Toolbox PRIVATE -DURHO3D_TOOLBOX_EXPORTS=1)
if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions(Toolbox PUBLIC -DURHO3D_TOOLBOX_STATIC=1)
endif ()
target_include_directories(Toolbox
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
set_target_properties(Toolbox PROPERTIES AUTOMOC TRUE)
install(TARGETS Toolbox EXPORT Urho3D
    RUNTIME DESTINATION ${DEST_BIN_DIR}
    ARCHIVE DESTINATION ${DEST_ARCHIVE_DIR}
    LIBRARY DESTINATION ${DEST_LIBRARY_DIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION ${DEST_INCLUDE_DIR} FILES_MATCHING PATTERN *.h)
get_target_property(TARGET_SOURCES Toolbox SOURCES)
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${TARGET_SOURCES})
