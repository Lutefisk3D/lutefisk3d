#
# Copyright (c) 2008-2016 the Urho3D project.
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

# Include common to all samples

set (COMMON_SAMPLE_H_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Sample.h" "${CMAKE_CURRENT_SOURCE_DIR}/Sample.inl")

# Define dependency libs

# Macro for adding sample subdirectory
macro (add_sample_subdirectory SOURCE_DIR)
    if (NOT DEFINED ENV{EXCLUDE_SAMPLE_${SOURCE_DIR}})
        add_subdirectory (${SOURCE_DIR} ${ARGN})
    endif ()
endmacro ()

macro(add_sample NAME MAIN_SOURCE)
    add_executable(${NAME} ${MAIN_SOURCE}.cpp ${MAIN_SOURCE}.h ${COMMON_SAMPLE_H_FILES} ${ARGN})
    target_link_libraries(${NAME} Lutefisk3D )
endmacro()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# Add samples
add_sample_subdirectory (01_HelloWorld)
add_sample_subdirectory (02_HelloGUI)
add_sample_subdirectory (03_Sprites)
add_sample_subdirectory (04_StaticScene)
add_sample_subdirectory (05_AnimatingScene)
add_sample_subdirectory (06_SkeletalAnimation)
add_sample_subdirectory (07_Billboards)
add_sample_subdirectory (08_Decals)
add_sample_subdirectory (09_MultipleViewports)
add_sample_subdirectory (10_RenderToTexture)
if(NOT LUTEFISK3D_SOUNDLESS)
#    add_sample_subdirectory (14_SoundEffects)
    add_sample_subdirectory (29_SoundSynthesis)
endif()
add_sample_subdirectory (20_HugeObjectCount)
add_sample_subdirectory (23_Water)
#add_sample_subdirectory (26_ConsoleInput)
add_sample_subdirectory (30_LightAnimation)
add_sample_subdirectory (31_MaterialAnimation)
add_sample_subdirectory (34_DynamicGeometry)
add_sample_subdirectory (35_SignedDistanceFieldText)
add_sample_subdirectory (37_UIDrag)
add_sample_subdirectory (38_SceneAndUILoad)
add_sample_subdirectory (42_PBRMaterials)
add_sample_subdirectory (44_RibbonTrailDemo)
if (NOT LUTEFISK3D_PHYSICSLESS)
    add_sample_subdirectory (11_Physics)
    add_sample_subdirectory (12_PhysicsStressTest)
    add_sample_subdirectory (13_Ragdolls)
    add_sample_subdirectory (18_CharacterDemo)
    add_sample_subdirectory (19_VehicleDemo)
endif ()
if (NOT LUTEFISK3D_NAVIGATIONLESS)
    add_sample_subdirectory (15_Navigation)
#    add_sample_subdirectory (39_CrowdNavigation)
endif ()
if (NOT LUTEFISK3D_NETLESS)
    add_sample_subdirectory (16_Chat)
endif ()
if (NOT LUTEFISK3D_NETLESS AND NOT LUTEFISK3D_PHYSICSLESS)
    add_sample_subdirectory (17_SceneReplication)
endif ()
#if (URHO3D_ANGELSCRIPT)
##    add_sample_subdirectory (21_AngelScriptIntegration)
#endif ()
#if (URHO3D_LUA)
##    add_sample_subdirectory (22_LuaIntegration)
#endif ()
if (NOT LUTEFISK3D_2DLESS)
    add_sample_subdirectory (24_Urho2DSprite)
    add_sample_subdirectory (25_Urho2DParticle)
    add_sample_subdirectory (27_Urho2DPhysics)
    add_sample_subdirectory (28_Urho2DPhysicsRope)
    #add_sample_subdirectory (32_Urho2DConstraints)
    add_sample_subdirectory (33_Urho2DSpriterAnimation)
    add_sample_subdirectory (36_Urho2DTileMap)
endif ()
