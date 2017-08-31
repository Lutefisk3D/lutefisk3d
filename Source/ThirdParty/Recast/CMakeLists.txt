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
RecastAlloc.cpp
RecastArea.cpp
RecastContour.cpp
Recast.cpp
RecastFilter.cpp
RecastLayers.cpp
RecastMesh.cpp
RecastMeshDetail.cpp
RecastRasterization.cpp
RecastRegion.cpp
)
set(INCLUDES
RecastAlloc.h
RecastAssert.h
Recast.h
)

add_library(Recast STATIC ${SOURCE_FILES} ${INCLUDES})
#install (FILES ${INCLUDES} DESTINATION include/ThirdParty/Recast)
set_property(TARGET Recast PROPERTY POSITION_INDEPENDENT_CODE ON)
