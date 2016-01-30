#
# Copyright (c) 2015-2015 the Lutefisk3D project.
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

set(SOURCE_FILES
DetourCrowd.cpp
DetourLocalBoundary.cpp
DetourObstacleAvoidance.cpp
DetourPathCorridor.cpp
DetourPathQueue.cpp
DetourProximityGrid.cpp
)
set(INCLUDES
DetourCrowd.h
DetourLocalBoundary.h
DetourObstacleAvoidance.h
DetourPathCorridor.h
DetourPathQueue.h
DetourProximityGrid.h
)

add_library(DetourCrowd OBJECT ${SOURCE_FILES} ${INCLUDES})
install (FILES ${INCLUDES} DESTINATION include/ThirdParty/DetourCrowd)
add_dependencies(DetourCrowd Detour Recast)

target_include_directories(DetourCrowd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories($<TARGET_PROPERTY:Detour,INTERFACE_INCLUDE_DIRECTORIES>)
######################### Add component to the project's list
list(APPEND ThirdParty_requisites $<TARGET_OBJECTS:DetourCrowd>)
SET(ThirdParty_requisites ${ThirdParty_requisites} PARENT_SCOPE)
