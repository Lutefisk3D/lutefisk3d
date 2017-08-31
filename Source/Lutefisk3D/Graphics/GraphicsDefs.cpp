//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Math/Vector3.h"

namespace Urho3D
{
// The extern keyword is required when building Urho3D.dll for Windows platform
// The keyword is not required for other platforms but it does no harm, aside from warning from static analyzer

extern LUTEFISK3D_EXPORT const StringHash VSP_AMBIENTSTARTCOLOR("AmbientStartColor");
extern LUTEFISK3D_EXPORT const StringHash VSP_AMBIENTENDCOLOR("AmbientEndColor");
extern LUTEFISK3D_EXPORT const StringHash VSP_BILLBOARDROT("BillboardRot");
extern LUTEFISK3D_EXPORT const StringHash VSP_CAMERAPOS("CameraPos");
extern LUTEFISK3D_EXPORT const StringHash VSP_CLIPPLANE("ClipPlane");
extern LUTEFISK3D_EXPORT const StringHash VSP_NEARCLIP("NearClip");
extern LUTEFISK3D_EXPORT const StringHash VSP_FARCLIP("FarClip");
extern LUTEFISK3D_EXPORT const StringHash VSP_DEPTHMODE("DepthMode");
extern LUTEFISK3D_EXPORT const StringHash VSP_DELTATIME("DeltaTime");
extern LUTEFISK3D_EXPORT const StringHash VSP_ELAPSEDTIME("ElapsedTime");
extern LUTEFISK3D_EXPORT const StringHash VSP_FRUSTUMSIZE("FrustumSize");
extern LUTEFISK3D_EXPORT const StringHash VSP_GBUFFEROFFSETS("GBufferOffsets");
extern LUTEFISK3D_EXPORT const StringHash VSP_LIGHTDIR("LightDir");
extern LUTEFISK3D_EXPORT const StringHash VSP_LIGHTPOS("LightPos");
extern LUTEFISK3D_EXPORT const StringHash VSP_NORMALOFFSETSCALE("NormalOffsetScale");
extern LUTEFISK3D_EXPORT const StringHash VSP_MODEL("Model");
extern LUTEFISK3D_EXPORT const StringHash VSP_VIEW("View");
extern LUTEFISK3D_EXPORT const StringHash VSP_VIEWINV("ViewInv");
extern LUTEFISK3D_EXPORT const StringHash VSP_VIEWPROJ("ViewProj");
extern LUTEFISK3D_EXPORT const StringHash VSP_UOFFSET("UOffset");
extern LUTEFISK3D_EXPORT const StringHash VSP_VOFFSET("VOffset");
extern LUTEFISK3D_EXPORT const StringHash VSP_ZONE("Zone");
extern LUTEFISK3D_EXPORT const StringHash VSP_LIGHTMATRICES("LightMatrices");
extern LUTEFISK3D_EXPORT const StringHash VSP_SKINMATRICES("SkinMatrices");
extern LUTEFISK3D_EXPORT const StringHash VSP_VERTEXLIGHTS("VertexLights");
extern LUTEFISK3D_EXPORT const StringHash PSP_AMBIENTCOLOR("AmbientColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_CAMERAPOS("CameraPosPS");
extern LUTEFISK3D_EXPORT const StringHash PSP_DELTATIME("DeltaTimePS");
extern LUTEFISK3D_EXPORT const StringHash PSP_DEPTHRECONSTRUCT("DepthReconstruct");
extern LUTEFISK3D_EXPORT const StringHash PSP_ELAPSEDTIME("ElapsedTimePS");
extern LUTEFISK3D_EXPORT const StringHash PSP_FOGCOLOR("FogColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_FOGPARAMS("FogParams");
extern LUTEFISK3D_EXPORT const StringHash PSP_GBUFFERINVSIZE("GBufferInvSize");
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTCOLOR("LightColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTDIR("LightDirPS");
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTPOS("LightPosPS");
extern LUTEFISK3D_EXPORT const StringHash PSP_NORMALOFFSETSCALE("NormalOffsetScalePS");
extern LUTEFISK3D_EXPORT const StringHash PSP_MATDIFFCOLOR("MatDiffColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_MATEMISSIVECOLOR("MatEmissiveColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_MATENVMAPCOLOR("MatEnvMapColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_MATSPECCOLOR("MatSpecColor");
extern LUTEFISK3D_EXPORT const StringHash PSP_NEARCLIP("NearClipPS");
extern LUTEFISK3D_EXPORT const StringHash PSP_FARCLIP("FarClipPS");
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWCUBEADJUST("ShadowCubeAdjust");
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWDEPTHFADE("ShadowDepthFade");
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWINTENSITY("ShadowIntensity");
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWMAPINVSIZE("ShadowMapInvSize");
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWSPLITS("ShadowSplits");
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTMATRICES("LightMatricesPS");
extern LUTEFISK3D_EXPORT const StringHash PSP_VSMSHADOWPARAMS("VSMShadowParams");
extern LUTEFISK3D_EXPORT const StringHash PSP_ROUGHNESS("Roughness");
extern LUTEFISK3D_EXPORT const StringHash PSP_METALLIC("Metallic");
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTRAD("LightRad");
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTLENGTH("LightLength");
extern LUTEFISK3D_EXPORT const StringHash PSP_ZONEMIN("ZoneMin");
extern LUTEFISK3D_EXPORT const StringHash PSP_ZONEMAX("ZoneMax");

extern LUTEFISK3D_EXPORT const Vector3 DOT_SCALE(1 / 3.0f, 1 / 3.0f, 1 / 3.0f);

extern LUTEFISK3D_EXPORT const VertexElement LEGACY_VERTEXELEMENTS[] =
{
    VertexElement(TYPE_VECTOR3, SEM_POSITION, 0, false),     // Position
    VertexElement(TYPE_VECTOR3, SEM_NORMAL, 0, false),       // Normal
    VertexElement(TYPE_UBYTE4_NORM, SEM_COLOR, 0, false),    // Color
    VertexElement(TYPE_VECTOR2, SEM_TEXCOORD, 0, false),     // Texcoord1
    VertexElement(TYPE_VECTOR2, SEM_TEXCOORD, 1, false),     // Texcoord2
    VertexElement(TYPE_VECTOR3, SEM_TEXCOORD, 0, false),     // Cubetexcoord1
    VertexElement(TYPE_VECTOR3, SEM_TEXCOORD, 1, false),     // Cubetexcoord2
    VertexElement(TYPE_VECTOR4, SEM_TANGENT, 0, false),      // Tangent
    VertexElement(TYPE_VECTOR4, SEM_BLENDWEIGHTS, 0, false), // Blendweights
    VertexElement(TYPE_UBYTE4, SEM_BLENDINDICES, 0, false),  // Blendindices
    VertexElement(TYPE_VECTOR4, SEM_TEXCOORD, 4, true),      // Instancematrix1
    VertexElement(TYPE_VECTOR4, SEM_TEXCOORD, 5, true),      // Instancematrix2
    VertexElement(TYPE_VECTOR4, SEM_TEXCOORD, 6, true),      // Instancematrix3
    VertexElement(TYPE_INT, SEM_OBJECTINDEX, 0, false)      // Objectindex
};

extern LUTEFISK3D_EXPORT const unsigned ELEMENT_TYPESIZES[] =
{
    sizeof(int),
    sizeof(float),
    2 * sizeof(float),
    3 * sizeof(float),
    4 * sizeof(float),
    sizeof(unsigned),
    sizeof(unsigned)
};
}
