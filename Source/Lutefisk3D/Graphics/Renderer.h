//
// Copyright (c) 2008-2017 the Urho3D project.
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

#pragma once
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/DataHandle.h"
#include "Lutefisk3D/Math/Color.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Graphics/Drawable.h"
#include "Lutefisk3D/Graphics/Viewport.h"
#include "jlsignal/SignalBase.h"

namespace Urho3D
{
class Geometry;
class Drawable;
class Light;
class Material;
class Pass;
class Technique;
class Octree;
class Graphics;
class RenderPath;
class RenderSurface;
class ResourceCache;
class Scene;
class Skeleton;
class OcclusionBuffer;
class Technique;
class Texture;
class Texture2D;
class TextureCube;
class VertexBuffer;
class View;
class Zone;
struct BatchQueue;
class ShaderVariation;
struct Batch;
using VertexBufferHandle = DataHandle<VertexBuffer,20,20>;
extern template class SharedPtr<ShaderVariation>;

static const int SHADOW_MIN_PIXELS = 64;
static const int INSTANCING_BUFFER_DEFAULT_SIZE = 1024;

/// Light vertex shader variations.
enum LightVSVariation
{
    LVS_DIR = 0,
    LVS_SPOT,
    LVS_POINT,
    LVS_SHADOW,
    LVS_SPOTSHADOW,
    LVS_POINTSHADOW,
    LVS_SHADOWNORMALOFFSET,
    LVS_SPOTSHADOWNORMALOFFSET,
    LVS_POINTSHADOWNORMALOFFSET,
    MAX_LIGHT_VS_VARIATIONS
};

/// Per-vertex light vertex shader variations.
enum VertexLightVSVariation
{
    VLVS_NOLIGHTS = 0,
    VLVS_1LIGHT,
    VLVS_2LIGHTS,
    VLVS_3LIGHTS,
    VLVS_4LIGHTS,
    MAX_VERTEXLIGHT_VS_VARIATIONS
};

/// Light pixel shader variations.
enum LightPSVariation
{
    LPS_NONE = 0,
    LPS_SPOT,
    LPS_POINT,
    LPS_POINTMASK,
    LPS_SPEC,
    LPS_SPOTSPEC,
    LPS_POINTSPEC,
    LPS_POINTMASKSPEC,
    LPS_SHADOW,
    LPS_SPOTSHADOW,
    LPS_POINTSHADOW,
    LPS_POINTMASKSHADOW,
    LPS_SHADOWSPEC,
    LPS_SPOTSHADOWSPEC,
    LPS_POINTSHADOWSPEC,
    LPS_POINTMASKSHADOWSPEC,
    MAX_LIGHT_PS_VARIATIONS
};

/// Deferred light volume vertex shader variations.
enum DeferredLightVSVariation
{
    DLVS_NONE = 0,
    DLVS_DIR,
    DLVS_ORTHO,
    DLVS_ORTHODIR,
    MAX_DEFERRED_LIGHT_VS_VARIATIONS
};

/// Deferred light volume pixels shader variations.
enum DeferredLightPSVariation
{
    DLPS_NONE = 0,
    DLPS_SPOT,
    DLPS_POINT,
    DLPS_POINTMASK,
    DLPS_SPEC,
    DLPS_SPOTSPEC,
    DLPS_POINTSPEC,
    DLPS_POINTMASKSPEC,
    DLPS_SHADOW,
    DLPS_SPOTSHADOW,
    DLPS_POINTSHADOW,
    DLPS_POINTMASKSHADOW,
    DLPS_SHADOWSPEC,
    DLPS_SPOTSHADOWSPEC,
    DLPS_POINTSHADOWSPEC,
    DLPS_POINTMASKSHADOWSPEC,
    DLPS_SHADOWNORMALOFFSET,
    DLPS_SPOTSHADOWNORMALOFFSET,
    DLPS_POINTSHADOWNORMALOFFSET,
    DLPS_POINTMASKSHADOWNORMALOFFSET,
    DLPS_SHADOWSPECNORMALOFFSET,
    DLPS_SPOTSHADOWSPECNORMALOFFSET,
    DLPS_POINTSHADOWSPECNORMALOFFSET,
    DLPS_POINTMASKSHADOWSPECNORMALOFFSET,
    DLPS_ORTHO,
    DLPS_ORTHOSPOT,
    DLPS_ORTHOPOINT,
    DLPS_ORTHOPOINTMASK,
    DLPS_ORTHOSPEC,
    DLPS_ORTHOSPOTSPEC,
    DLPS_ORTHOPOINTSPEC,
    DLPS_ORTHOPOINTMASKSPEC,
    DLPS_ORTHOSHADOW,
    DLPS_ORTHOSPOTSHADOW,
    DLPS_ORTHOPOINTSHADOW,
    DLPS_ORTHOPOINTMASKSHADOW,
    DLPS_ORTHOSHADOWSPEC,
    DLPS_ORTHOSPOTSHADOWSPEC,
    DLPS_ORTHOPOINTSHADOWSPEC,
    DLPS_ORTHOPOINTMASKSHADOWSPEC,
    DLPS_ORTHOSHADOWNORMALOFFSET,
    DLPS_ORTHOSPOTSHADOWNORMALOFFSET,
    DLPS_ORTHOPOINTSHADOWNORMALOFFSET,
    DLPS_ORTHOPOINTMASKSHADOWNORMALOFFSET,
    DLPS_ORTHOSHADOWSPECNORMALOFFSET,
    DLPS_ORTHOSPOTSHADOWSPECNORMALOFFSET,
    DLPS_ORTHOPOINTSHADOWSPECNORMALOFFSET,
    DLPS_ORTHOPOINTMASKSHADOWSPECNORMALOFFSET,
    MAX_DEFERRED_LIGHT_PS_VARIATIONS
};

/// High-level rendering subsystem. Manages drawing of 3D views.
class LUTEFISK3D_EXPORT Renderer : public jl::SignalObserver
{
public:
    using ShadowMapFilter = std::function<void(View* /*view*/, Texture2D* /*shadowMap*/,float /*blurScale*/)>;

    Renderer(Context* context);
    virtual ~Renderer();

    void SetNumViewports(unsigned num);
    void SetViewport(unsigned index, Viewport* viewport);
    void SetDefaultRenderPath(RenderPath* renderPath);
    void SetDefaultRenderPath(XMLFile* file);
    void SetDefaultTechnique(Technique* tech);
    void SetHDRRendering(bool enable);
    void SetSpecularLighting(bool enable);
    void SetTextureAnisotropy(int level);
    void SetTextureFilterMode(TextureFilterMode mode);
    void SetTextureQuality(eQuality quality);
    void SetMaterialQuality(eQuality quality);
    void SetDrawShadows(bool enable);
    void SetShadowMapSize(int size);
    void SetShadowQuality(ShadowQuality quality);
    void SetShadowSoftness(float shadowSoftness);
    void SetVSMShadowParameters(float minVariance, float lightBleedingReduction);
    void SetVSMMultiSample(int multiSample);
    void SetShadowMapFilter(ShadowMapFilter functionPtr);
    void SetReuseShadowMaps(bool enable);
    void SetMaxShadowMaps(int shadowMaps);
    void SetDynamicInstancing(bool enable);
    void SetNumExtraInstancingBufferElements(unsigned elements);
    void SetMinInstances(int instances);
    void SetMaxSortedInstances(int instances);
    void SetMaxOccluderTriangles(int triangles);
    void SetOcclusionBufferSize(int size);
    void SetOccluderSizeThreshold(float screenSize);
    void SetThreadedOcclusion(bool enable);

    void ReloadShaders();

    void ApplyShadowMapFilter(View* view, Texture2D* shadowMap, float blurScale);

    /// Return number of backbuffer viewports.
    unsigned GetNumViewports() const { return viewports_.size(); }
    /// Return backbuffer viewport by index.
    Viewport* GetViewport(unsigned index) const;
    /// Return nth backbuffer viewport associated to a scene. Index 0 returns the first.
    Viewport* GetViewportForScene(Scene* scene, unsigned index) const;
    /// Return default renderpath.
    RenderPath* GetDefaultRenderPath() const;

    /// Return default non-textured material technique.
    Technique* GetDefaultTechnique() const;

    /// Return whether HDR rendering is enabled.
    bool GetHDRRendering() const { return hdrRendering_; }
    /// Return whether specular lighting is enabled.
    bool GetSpecularLighting() const { return specularLighting_; }
    /// Return whether drawing shadows is enabled.
    bool GetDrawShadows() const { return drawShadows_; }
    /// Return default texture max. anisotropy level.
    int GetTextureAnisotropy() const { return textureAnisotropy_; }
    /// Return default texture filtering mode.
    TextureFilterMode GetTextureFilterMode() const { return textureFilterMode_; }
    /// Return texture quality level.
    int GetTextureQuality() const { return textureQuality_; }
    /// Return material quality level.
    int GetMaterialQuality() const { return materialQuality_; }
    /// Return shadow map resolution.
    int GetShadowMapSize() const { return shadowMapSize_; }
    /// Return shadow quality.
    ShadowQuality GetShadowQuality() const { return shadowQuality_; }

    /// Return shadow softness.
    float GetShadowSoftness() const { return shadowSoftness_; }

    /// Return VSM shadow parameters.
    Vector2 GetVSMShadowParameters() const { return vsmShadowParams_; }
    /// Return VSM shadow multisample level.
    int GetVSMMultiSample() const { return vsmMultiSample_; }
    /// Return whether shadow maps are reused.
    bool GetReuseShadowMaps() const { return reuseShadowMaps_; }
    /// Return maximum number of shadow maps per resolution.
    int GetMaxShadowMaps() const { return maxShadowMaps_; }
    /// Return whether dynamic instancing is in use.
    bool GetDynamicInstancing() const { return dynamicInstancing_; }
    /// Return number of extra instancing buffer elements.
    int GetNumExtraInstancingBufferElements() const { return numExtraInstancingBufferElements_; }
    /// Return minimum number of instances required in a batch group to render as instanced.
    int GetMinInstances() const { return minInstances_; }
    /// Return maximum number of sorted instances per batch group.
    int GetMaxSortedInstances() const { return maxSortedInstances_; }
    /// Return maximum number of occluder triangles.
    int GetMaxOccluderTriangles() const { return maxOccluderTriangles_; }
    /// Return occlusion buffer width.
    int GetOcclusionBufferSize() const { return occlusionBufferSize_; }
    /// Return occluder screen size threshold.
    float GetOccluderSizeThreshold() const { return occluderSizeThreshold_; }
    /// Return whether occlusion rendering is threaded.
    bool GetThreadedOcclusion() const { return threadedOcclusion_; }
    /// Return number of views rendered.
    unsigned GetNumViews() const { return views_.size(); }
    /// Return number of primitives rendered.
    unsigned GetNumPrimitives() const { return numPrimitives_; }
    /// Return number of batches rendered.
    unsigned GetNumBatches() const { return numBatches_; }
    /// Return number of geometries rendered.
    unsigned GetNumGeometries(bool allViews = false) const;
    /// Return number of lights rendered.
    unsigned GetNumLights(bool allViews = false) const;
    /// Return number of shadow maps rendered.
    unsigned GetNumShadowMaps(bool allViews = false) const;
    /// Return number of occluders rendered.
    unsigned GetNumOccluders(bool allViews = false) const;
    /// Return the default zone.
    Zone* GetDefaultZone() const { return defaultZone_.get(); }
    /// Return the default material.
    Material* GetDefaultMaterial() const { return defaultMaterial_.get(); }
    /// Return the default range attenuation texture.
    Texture2D* GetDefaultLightRamp() const { return defaultLightRamp_; }
    /// Return the default spotlight attenuation texture.
    Texture2D* GetDefaultLightSpot() const { return defaultLightSpot_; }
    /// Return the shadowed pointlight face selection cube map.
    TextureCube* GetFaceSelectCubeMap() const { return faceSelectCubeMap_; }
    /// Return the shadowed pointlight indirection cube map.
    TextureCube* GetIndirectionCubeMap() const { return indirectionCubeMap_; }
    /// Return the instancing vertex buffer
    VertexBuffer* GetInstancingBuffer() const { return dynamicInstancing_ ? instancingBuffer_.get() : nullptr; }
    /// Return the frame update parameters.
    const FrameInfo& GetFrameInfo() const { return frame_; }

    void Update(float timeStep);
    void Render();
    void DrawDebugGeometry(bool depthTest);
    void QueueRenderSurface(RenderSurface* renderTarget);
    void QueueViewport(RenderSurface* renderTarget, Viewport* viewport);

    Geometry* GetLightGeometry(Light* light);
    Geometry* GetQuadGeometry();
    Texture2D* GetShadowMap(Light* light, Camera* camera, unsigned viewWidth, unsigned viewHeight);
    Texture *GetScreenBuffer(int width, int height, uint32_t format, int multiSample, bool autoResolve, bool cubemap,
                             bool filtered, bool srgb, unsigned persistentKey = 0);
    RenderSurface* GetDepthStencil(int width, int height, int multiSample, bool autoResolve);
    OcclusionBuffer* GetOcclusionBuffer(Camera* camera);
    Camera* GetShadowCamera();
    void StorePreparedView(View* view, Camera* cullCamera);
    View* GetPreparedView(Camera* cullCamera);
    void SetBatchShaders(Batch& batch, const Technique* tech, const BatchQueue& queue, bool allowShadows = true);
    void SetLightVolumeBatchShaders(Batch &batch, Camera *camera, const QString &vsName, const QString &psName,
                                    const QString &vsDefines, const QString &psDefines);
    void SetCullMode(CullMode mode, const Urho3D::Camera *camera);
    bool ResizeInstancingBuffer(unsigned numInstances);
    void OptimizeLightByScissor(Light* light, Camera* camera);
    void OptimizeLightByStencil(Light* light, Camera* camera);
    const Rect& GetLightScissor(Light* light, Camera* camera);
    static View* GetActualView(View* view);

private:
    void Initialize();
    void LoadShaders();
    void LoadPassShaders(Pass* pass, std::vector<SharedPtr<ShaderVariation> >& vertexShaders, std::vector<SharedPtr<ShaderVariation> >& pixelShaders, const BatchQueue& queue);
    void ReleaseMaterialShaders();
    void ReloadTextures();
    void CreateGeometries();
    void CreateInstancingBuffer();
    void SetIndirectionTextureData();
    void UpdateQueuedViewport(unsigned index);
    void PrepareViewRender();
    void RemoveUnusedBuffers();
    void ResetShadowMapAllocations();
    void ResetScreenBufferAllocations();
    void ResetShadowMaps();
    void ResetBuffers();
    QString GetShadowVariations() const;
    void HandleScreenMode(int, int, bool, bool, bool, bool, int, int);
    void HandleRenderUpdate(float ts);
    /// Blur the shadow map.
    void BlurShadowMap(View* view, Texture2D* shadowMap, float blurScale);

    Context *m_context;
    WeakPtr<Graphics> graphics_;
    SharedPtr<RenderPath> defaultRenderPath_;
    SharedPtr<Technique> defaultTechnique_;
    std::unique_ptr<Zone> defaultZone_;
    std::unique_ptr<Geometry>               dirLightGeometry_;
    std::unique_ptr<Geometry>               spotLightGeometry_;
    std::unique_ptr<Geometry>               pointLightGeometry_;
    std::unique_ptr<VertexBuffer> instancingBuffer_;
    std::unique_ptr<Material> defaultMaterial_;
    SharedPtr<Texture2D> defaultLightRamp_;
    SharedPtr<Texture2D> defaultLightSpot_;
    SharedPtr<TextureCube> faceSelectCubeMap_;
    SharedPtr<TextureCube> indirectionCubeMap_;
    std::vector<SharedPtr<Node> > shadowCameraNodes_;
    std::vector<SharedPtr<OcclusionBuffer> > occlusionBuffers_;
    HashMap<int, std::vector<SharedPtr<Texture2D> > > shadowMaps_;
    HashMap<int, SharedPtr<Texture2D> > colorShadowMaps_;
    HashMap<int, std::vector<Light*> > shadowMapAllocations_;
    ShadowMapFilter shadowMapFilter_ {nullptr};
    HashMap<int64_t, std::vector<SharedPtr<Texture>>> screenBuffers_;
    HashMap<int64_t, unsigned>                        screenBufferAllocations_;
    HashMap<int64_t, unsigned>                        savedScreenBufferAllocations_;
    HashMap<std::pair<Light*, Camera*>, Rect> lightScissorCache_;
    std::vector<SharedPtr<Viewport> > viewports_;
    std::vector<std::pair<WeakPtr<RenderSurface>, WeakPtr<Viewport> > > queuedViewports_;
    std::vector<WeakPtr<View> > views_;
    HashMap<Camera*, WeakPtr<View> > preparedViews_;
    HashSet<Octree*> updatedOctrees_;
    HashSet<const Technique*> shaderErrorDisplayed_;
    Mutex rendererMutex_;
    QStringList deferredLightPSVariations_;
    FrameInfo frame_;
    int                        textureAnisotropy_                = 4;
    TextureFilterMode          textureFilterMode_                = FILTER_TRILINEAR;
    int                        textureQuality_                   = QUALITY_HIGH;
    int                        materialQuality_                  = QUALITY_HIGH;
    int                        shadowMapSize_                    = 1024;
    ShadowQuality              shadowQuality_                    = SHADOWQUALITY_PCF_16BIT;
    float                      shadowSoftness_                   = 1.0f;
    Vector2                    vsmShadowParams_                  = {0.0000001f, 0.9f};
    int                        vsmMultiSample_                   = 1;
    int                        maxShadowMaps_                    = 1;
    int                        minInstances_                     = 2;
    int                        maxSortedInstances_               = 1000;
    int                        maxOccluderTriangles_             = 5000;
    int                        occlusionBufferSize_              = 256;
    float                      occluderSizeThreshold_            = 0.025f;
    unsigned                   numOcclusionBuffers_              = 0;
    unsigned                   numShadowCameras_                 = 0;
    unsigned                   numPrimitives_                    = 0;
    unsigned                   numBatches_                       = 0;
    unsigned                   shadersChangedFrameNumber_        = M_MAX_UNSIGNED;
    unsigned                   numExtraInstancingBufferElements_ = 0;
    uint8_t                    lightStencilValue_                = 0;
    bool                       hdrRendering_                     = false;
    bool                       specularLighting_                 = true;
    bool                       drawShadows_                      = true;
    bool                       reuseShadowMaps_                  = true;
    bool                       dynamicInstancing_                = true;
    bool                       threadedOcclusion_                = false;
    bool                       shadersDirty_                     = true;
    bool                       initialized_                      = false;
    bool                       resetViews_                       = false;
};

}
