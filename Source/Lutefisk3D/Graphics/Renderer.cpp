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

#include "../Graphics/Camera.h"
#include "../Core/CoreEvents.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/GraphicsImpl.h"
#include "../Graphics/IndexBuffer.h"
#include "../IO/Log.h"
#include "../Graphics/Material.h"
#include "../Graphics/OcclusionBuffer.h"
#include "../Graphics/Octree.h"
#include "../Core/Profiler.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RenderPath.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../Graphics/ShaderVariation.h"
#include "../Graphics/Technique.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/TextureCube.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/View.h"
#include "../Resource/XMLFile.h"
#include "../Graphics/Zone.h"



namespace Urho3D
{

static const float dirLightVertexData[] =
{
    -1, 1, 0,
    1, 1, 0,
    1, -1, 0,
    -1, -1, 0,
};

static const unsigned short dirLightIndexData[] =
{
    0, 1, 2,
    2, 3, 0,
};

static const float pointLightVertexData[] =
{
    -0.423169f, -1.000000f, 0.423169f,
    -0.423169f, -1.000000f, -0.423169f,
    0.423169f, -1.000000f, -0.423169f,
    0.423169f, -1.000000f, 0.423169f,
    0.423169f, 1.000000f, -0.423169f,
    -0.423169f, 1.000000f, -0.423169f,
    -0.423169f, 1.000000f, 0.423169f,
    0.423169f, 1.000000f, 0.423169f,
    -1.000000f, 0.423169f, -0.423169f,
    -1.000000f, -0.423169f, -0.423169f,
    -1.000000f, -0.423169f, 0.423169f,
    -1.000000f, 0.423169f, 0.423169f,
    0.423169f, 0.423169f, -1.000000f,
    0.423169f, -0.423169f, -1.000000f,
    -0.423169f, -0.423169f, -1.000000f,
    -0.423169f, 0.423169f, -1.000000f,
    1.000000f, 0.423169f, 0.423169f,
    1.000000f, -0.423169f, 0.423169f,
    1.000000f, -0.423169f, -0.423169f,
    1.000000f, 0.423169f, -0.423169f,
    0.423169f, -0.423169f, 1.000000f,
    0.423169f, 0.423169f, 1.000000f,
    -0.423169f, 0.423169f, 1.000000f,
    -0.423169f, -0.423169f, 1.000000f
};

static const unsigned short pointLightIndexData[] =
{
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23,
    0, 10, 9,
    0, 9, 1,
    13, 2, 1,
    13, 1, 14,
    23, 0, 3,
    23, 3, 20,
    17, 3, 2,
    17, 2, 18,
    21, 7, 6,
    21, 6, 22,
    7, 16, 19,
    7, 19, 4,
    5, 8, 11,
    5, 11, 6,
    4, 12, 15,
    4, 15, 5,
    22, 11, 10,
    22, 10, 23,
    8, 15, 14,
    8, 14, 9,
    12, 19, 18,
    12, 18, 13,
    16, 21, 20,
    16, 20, 17,
    0, 23, 10,
    1, 9, 14,
    2, 13, 18,
    3, 17, 20,
    6, 11, 22,
    5, 15, 8,
    4, 19, 12,
    7, 21, 16
};

static const float spotLightVertexData[] =
{
    0.00001f, 0.00001f, 0.00001f,
    0.00001f, -0.00001f, 0.00001f,
    -0.00001f, -0.00001f, 0.00001f,
    -0.00001f, 0.00001f, 0.00001f,
    1.00000f, 1.00000f, 0.99999f,
    1.00000f, -1.00000f, 0.99999f,
    -1.00000f,  -1.00000f, 0.99999f,
    -1.00000f, 1.00000f, 0.99999f,
};

static const unsigned short spotLightIndexData[] =
{
    3, 0, 1,
    3, 1, 2,
    0, 4, 5,
    0, 5, 1,
    3, 7, 4,
    3, 4, 0,
    7, 3, 2,
    7, 2, 6,
    6, 2, 1,
    6, 1, 5,
    7, 5, 4,
    7, 6, 5
};


static const char* geometryVSVariations[] =
{
    "",
    "SKINNED ",
    "INSTANCED ",
    "BILLBOARD ",
    "DIRBILLBOARD "
};

static const char* lightVSVariations[] =
{
    "PERPIXEL DIRLIGHT ",
    "PERPIXEL SPOTLIGHT ",
    "PERPIXEL POINTLIGHT ",
    "PERPIXEL DIRLIGHT SHADOW ",
    "PERPIXEL SPOTLIGHT SHADOW ",
    "PERPIXEL POINTLIGHT SHADOW ",
};

static const char* vertexLightVSVariations[] =
{
    "",
    "NUMVERTEXLIGHTS=1 ",
    "NUMVERTEXLIGHTS=2 ",
    "NUMVERTEXLIGHTS=3 ",
    "NUMVERTEXLIGHTS=4 ",
};

static const char* deferredLightVSVariations[] =
{
    "",
    "DIRLIGHT ",
    "ORTHO ",
    "DIRLIGHT ORTHO "
};

static const char* lightPSVariations[] =
{
    "PERPIXEL DIRLIGHT ",
    "PERPIXEL SPOTLIGHT ",
    "PERPIXEL POINTLIGHT ",
    "PERPIXEL POINTLIGHT CUBEMASK ",
    "PERPIXEL DIRLIGHT SPECULAR ",
    "PERPIXEL SPOTLIGHT SPECULAR ",
    "PERPIXEL POINTLIGHT SPECULAR ",
    "PERPIXEL POINTLIGHT CUBEMASK SPECULAR ",
    "PERPIXEL DIRLIGHT SHADOW ",
    "PERPIXEL SPOTLIGHT SHADOW ",
    "PERPIXEL POINTLIGHT SHADOW ",
    "PERPIXEL POINTLIGHT CUBEMASK SHADOW ",
    "PERPIXEL DIRLIGHT SPECULAR SHADOW ",
    "PERPIXEL SPOTLIGHT SPECULAR SHADOW ",
    "PERPIXEL POINTLIGHT SPECULAR SHADOW ",
    "PERPIXEL POINTLIGHT CUBEMASK SPECULAR SHADOW "
};

static const char* heightFogVariations[] =
{
    "",
    "HEIGHTFOG "
};

static const unsigned INSTANCING_BUFFER_MASK = MASK_INSTANCEMATRIX1 | MASK_INSTANCEMATRIX2 | MASK_INSTANCEMATRIX3;
static const unsigned MAX_BUFFER_AGE = 1000;

Renderer::Renderer(Context* context) :
    Object(context),
    defaultZone_(new Zone(context)),
    shadowMapFilterInstance_(0),
    shadowMapFilter_(0),
    textureAnisotropy_(4),
    textureFilterMode_(FILTER_TRILINEAR),
    textureQuality_(QUALITY_HIGH),
    materialQuality_(QUALITY_HIGH),
    shadowMapSize_(1024),
    shadowQuality_(SHADOWQUALITY_PCF_16BIT),
    shadowSoftness_(2.0f),
    vsmShadowParams_(0.0000001f, 0.2f),
    maxShadowMaps_(1),
    minInstances_(2),
    maxSortedInstances_(1000),
    maxOccluderTriangles_(5000),
    occlusionBufferSize_(256),
    occluderSizeThreshold_(0.025f),
    mobileShadowBiasMul_(2.0f),
    mobileShadowBiasAdd_(0.0001f),
    numOcclusionBuffers_(0),
    numShadowCameras_(0),
    shadersChangedFrameNumber_(M_MAX_UNSIGNED),
    hdrRendering_(false),
    specularLighting_(true),
    drawShadows_(true),
    reuseShadowMaps_(true),
    dynamicInstancing_(true),
    threadedOcclusion_(false),
    shadersDirty_(true),
    initialized_(false),
    resetViews_(false)
{
    SubscribeToEvent(E_SCREENMODE, URHO3D_HANDLER(Renderer, HandleScreenMode));

    // Try to initialize right now, but skip if screen mode is not yet set
    Initialize();
}

Renderer::~Renderer()
{
}

void Renderer::SetNumViewports(unsigned num)
{
    viewports_.resize(num);
}

void Renderer::SetViewport(unsigned index, Viewport* viewport)
{
    if (index >= viewports_.size())
        viewports_.resize(index + 1);

    viewports_[index] = viewport;
}

void Renderer::SetDefaultRenderPath(RenderPath* renderPath)
{
    if (renderPath)
        defaultRenderPath_ = renderPath;
}

void Renderer::SetDefaultRenderPath(XMLFile* xmlFile)
{
    SharedPtr<RenderPath> newRenderPath(new RenderPath());
    if (newRenderPath->Load(xmlFile))
        defaultRenderPath_ = newRenderPath;
}

void Renderer::SetHDRRendering(bool enable)
{
    hdrRendering_ = enable;
}

void Renderer::SetSpecularLighting(bool enable)
{
    specularLighting_ = enable;
}

void Renderer::SetTextureAnisotropy(int level)
{
    textureAnisotropy_ = Max(level, 1);
}

void Renderer::SetTextureFilterMode(TextureFilterMode mode)
{
    textureFilterMode_ = mode;
}

void Renderer::SetTextureQuality(int quality)
{
    quality = Clamp(quality, QUALITY_LOW, QUALITY_HIGH);

    if (quality != textureQuality_)
    {
        textureQuality_ = quality;
        ReloadTextures();
    }
}

void Renderer::SetMaterialQuality(int quality)
{
    quality = Clamp(quality, QUALITY_LOW, QUALITY_MAX);

    if (quality != materialQuality_)
    {
        materialQuality_ = quality;
        shadersDirty_ = true;
        // Reallocate views to not store eg. pass information that might be unnecessary on the new material quality level
        resetViews_ = true;
    }
}

void Renderer::SetDrawShadows(bool enable)
{
    if (!graphics_ || 0==(unsigned)graphics_->GetShadowMapFormat())
        return;

    drawShadows_ = enable;
    if (!drawShadows_)
        ResetShadowMaps();
}

void Renderer::SetShadowMapSize(int size)
{
    if (!graphics_)
        return;

    size = NextPowerOfTwo(Max(size, SHADOW_MIN_PIXELS));
    if (size != shadowMapSize_)
    {
        shadowMapSize_ = size;
        ResetShadowMaps();
    }
}

void Renderer::SetShadowQuality(ShadowQuality quality)
{
    if (!graphics_)
        return;


    // If no hardware PCF, do not allow to select one-sample quality
    if (!graphics_->GetHardwareShadowSupport())
    {
        if (quality == SHADOWQUALITY_SIMPLE_16BIT)
            quality = SHADOWQUALITY_PCF_16BIT;

        if (quality == SHADOWQUALITY_SIMPLE_24BIT)
            quality = SHADOWQUALITY_PCF_24BIT;
    }
    // if high resolution is not allowed
    if (0==graphics_->GetHiresShadowMapFormat())
    {
        if (quality == SHADOWQUALITY_SIMPLE_24BIT)
            quality = SHADOWQUALITY_SIMPLE_16BIT;

        if (quality == SHADOWQUALITY_PCF_24BIT)
            quality = SHADOWQUALITY_PCF_16BIT;
    }
    if (quality != shadowQuality_)
    {
        shadowQuality_ = quality;
        shadersDirty_ = true;
        if (quality == SHADOWQUALITY_BLUR_VSM)
            SetShadowMapFilter(this, static_cast<ShadowMapFilter>(&Renderer::BlurShadowMap));
        else
            SetShadowMapFilter(0, 0);
        ResetShadowMaps();
    }
}

void Renderer::SetShadowSoftness(float shadowSoftness)
{
    shadowSoftness_ = Max(shadowSoftness, 0.0f);
}

void Renderer::SetVSMShadowParameters(float minVariance, float lightBleedingReduction)
{
    vsmShadowParams_.x_ = Max(minVariance, 0.0f);
    vsmShadowParams_.y_ = Clamp(lightBleedingReduction, 0.0f, 1.0f);
}

void Renderer::SetShadowMapFilter(Object* instance, ShadowMapFilter functionPtr)
{
    shadowMapFilterInstance_ = instance;
    shadowMapFilter_ = functionPtr;
}

void Renderer::SetReuseShadowMaps(bool enable)
{
    reuseShadowMaps_ = enable;
}

void Renderer::SetMaxShadowMaps(int shadowMaps)
{
    if (shadowMaps < 1)
        return;

    maxShadowMaps_ = shadowMaps;
    for (auto & elem : shadowMaps_)
    {
        if ((int)ELEMENT_VALUE(elem).size() > maxShadowMaps_)
            ELEMENT_VALUE(elem).resize(maxShadowMaps_);
    }
}

void Renderer::SetDynamicInstancing(bool enable)
{
    if (!instancingBuffer_)
        enable = false;

    dynamicInstancing_ = enable;
}

void Renderer::SetMinInstances(int instances)
{
    minInstances_ = Max(instances, 2);
}


void Renderer::SetMaxSortedInstances(int instances)
{
    maxSortedInstances_ = Max(instances, 0);
}

void Renderer::SetMaxOccluderTriangles(int triangles)
{
    maxOccluderTriangles_ = Max(triangles, 0);
}

void Renderer::SetOcclusionBufferSize(int size)
{
    occlusionBufferSize_ = Max(size, 1);
    occlusionBuffers_.clear();
}

void Renderer::SetMobileShadowBiasMul(float mul)
{
    mobileShadowBiasMul_ = mul;
}

void Renderer::SetMobileShadowBiasAdd(float add)
{
    mobileShadowBiasAdd_ = add;
}

void Renderer::SetOccluderSizeThreshold(float screenSize)
{
    occluderSizeThreshold_ = Max(screenSize, 0.0f);
}

void Renderer::SetThreadedOcclusion(bool enable)
{
    if (enable != threadedOcclusion_)
    {
        threadedOcclusion_ = enable;
        occlusionBuffers_.clear();
    }
}
void Renderer::ReloadShaders()
{
    shadersDirty_ = true;
}

void Renderer::ApplyShadowMapFilter(View* view, Texture2D* shadowMap)
{
    if (shadowMapFilterInstance_ && shadowMapFilter_)
        (shadowMapFilterInstance_->*shadowMapFilter_)(view, shadowMap);
}
Viewport* Renderer::GetViewport(unsigned index) const
{
    return index < viewports_.size() ? viewports_[index] : (Viewport*)nullptr;
}

RenderPath* Renderer::GetDefaultRenderPath() const
{
    return defaultRenderPath_;
}

unsigned Renderer::GetNumGeometries(bool allViews) const
{
    unsigned numGeometries = 0;
    unsigned lastView = allViews ? views_.size() : 1;

    for (unsigned i = 0; i < lastView; ++i)
    {
        // Use the source view's statistics if applicable
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        numGeometries += view->GetGeometries().size();
    }

    return numGeometries;
}

unsigned Renderer::GetNumLights(bool allViews) const
{
    unsigned numLights = 0;
    unsigned lastView = allViews ? views_.size() : 1;

    for (unsigned i = 0; i < lastView; ++i)
    {
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        numLights += view->GetLights().size();
    }

    return numLights;
}

unsigned Renderer::GetNumShadowMaps(bool allViews) const
{
    unsigned numShadowMaps = 0;
    unsigned lastView = allViews ? views_.size() : 1;

    for (unsigned i = 0; i < lastView; ++i)
    {
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        const std::vector<LightBatchQueue>& lightQueues = view->GetLightQueues();

        for (const LightBatchQueue & lightQueue : lightQueues)
        {
            if (lightQueue.shadowMap_)
                ++numShadowMaps;
        }
    }

    return numShadowMaps;
}

unsigned Renderer::GetNumOccluders(bool allViews) const
{
    unsigned numOccluders = 0;
    unsigned lastView = allViews ? views_.size() : 1;

    for (unsigned i = 0; i < lastView; ++i)
    {
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        numOccluders += view->GetNumActiveOccluders();
    }

    return numOccluders;
}

void Renderer::Update(float timeStep)
{
    URHO3D_PROFILE(UpdateViews);

    views_.clear();
    preparedViews_.clear();

    // If device lost, do not perform update. This is because any dynamic vertex/index buffer updates happen already here,
    // and if the device is lost, the updates queue up, causing memory use to rise constantly
    if (!graphics_ || !graphics_->IsInitialized() || graphics_->IsDeviceLost())
        return;

    // Set up the frameinfo structure for this frame
    frame_.frameNumber_ = GetSubsystem<Time>()->GetFrameNumber();
    frame_.timeStep_ = timeStep;
    frame_.camera_ = nullptr;
    numShadowCameras_ = 0;
    numOcclusionBuffers_ = 0;
    updatedOctrees_.clear();

    // Reload shaders now if needed
    if (shadersDirty_)
        LoadShaders();

    // Queue update of the main viewports. Use reverse order, as rendering order is also reverse
    // to render auxiliary views before dependant main views
    for (unsigned i = viewports_.size() - 1; i < viewports_.size(); --i)
        QueueViewport(nullptr, viewports_[i]);

    // Gather other render surfaces that are autoupdated
    SendEvent(E_RENDERSURFACEUPDATE);

    // Process gathered views. This may queue further views (render surfaces that are only updated when visible)
    for (unsigned i = 0; i < queuedViewports_.size(); ++i)
    {
        WeakPtr<RenderSurface>& renderTarget = queuedViewports_[i].first;
        WeakPtr<Viewport>& viewport = queuedViewports_[i].second;

        // Null pointer means backbuffer view. Differentiate between that and an expired rendersurface
        if ((renderTarget.NotNull() && renderTarget.Expired()) || viewport.Expired())
            continue;

        // (Re)allocate the view structure if necessary
        if (!viewport->GetView() || resetViews_)
            viewport->AllocateView();

        View* view = viewport->GetView();
        assert(view);
        // Check if view can be defined successfully (has either valid scene, camera and octree, or no scene passes)
        if (!view->Define(renderTarget, viewport))
            continue;

        views_.push_back(WeakPtr<View>(view));

        const IntRect& viewRect = viewport->GetRect();
        Scene* scene = viewport->GetScene();
        if (!scene)
            continue;

        Octree* octree = scene->GetComponent<Octree>();

        // Update octree (perform early update for drawables which need that, and reinsert moved drawables.)
        // However, if the same scene is viewed from multiple cameras, update the octree only once
        if (!updatedOctrees_.contains(octree))
        {
            frame_.camera_ = viewport->GetCamera();
            frame_.viewSize_ = viewRect.Size();
            if (frame_.viewSize_ == IntVector2::ZERO)
                frame_.viewSize_ = IntVector2(graphics_->GetWidth(), graphics_->GetHeight());
            octree->Update(frame_);
            updatedOctrees_.insert(octree);

            // Set also the view for the debug renderer already here, so that it can use culling
            /// \todo May result in incorrect debug geometry culling if the same scene is drawn from multiple viewports
            DebugRenderer* debug = scene->GetComponent<DebugRenderer>();
            if (debug && viewport->GetDrawDebug())
                debug->SetView(viewport->GetCamera());
        }

        // Update view. This may queue further views. View will send update begin/end events once its state is set

        ResetShadowMapAllocations(); // Each view can reuse the same shadow maps
        view->Update(frame_);

    }

    queuedViewports_.clear();
    resetViews_ = false;
}

void Renderer::Render()
{
    // Engine does not render when window is closed or device is lost
    assert(graphics_ && graphics_->IsInitialized() && !graphics_->IsDeviceLost());

    URHO3D_PROFILE(RenderViews);

    // If the indirection textures have lost content (OpenGL mode only), restore them now
    if (faceSelectCubeMap_ && faceSelectCubeMap_->IsDataLost())
        SetIndirectionTextureData();

    graphics_->SetDefaultTextureFilterMode(textureFilterMode_);
    graphics_->SetTextureAnisotropy(textureAnisotropy_);

    // If no views, just clear the screen
    if (views_.empty())
    {
        graphics_->SetBlendMode(BLEND_REPLACE);
        graphics_->SetColorWrite(true);
        graphics_->SetDepthWrite(true);
        graphics_->SetScissorTest(false);
        graphics_->SetStencilTest(false);
        graphics_->ResetRenderTargets();
        graphics_->Clear(CLEAR_COLOR | CLEAR_DEPTH | CLEAR_STENCIL, defaultZone_->GetFogColor());

        numPrimitives_ = 0;
        numBatches_ = 0;
    }
    else
    {
        // Render views from last to first (each main view is rendered after the auxiliary views it depends on)
        for (unsigned i = views_.size() - 1; i < views_.size(); --i)
        {
            if (!views_[i])
                continue;

            using namespace BeginViewRender;

            RenderSurface* renderTarget = views_[i]->GetRenderTarget();

            VariantMap& eventData = GetEventDataMap();
            eventData[P_VIEW] = views_[i];
            eventData[P_SURFACE] = renderTarget;
            eventData[P_TEXTURE] = (renderTarget ? renderTarget->GetParentTexture() : nullptr);
            eventData[P_SCENE] = views_[i]->GetScene();
            eventData[P_CAMERA] = views_[i]->GetCamera();
            SendEvent(E_BEGINVIEWRENDER, eventData);

            // Screen buffers can be reused between views, as each is rendered completely
            PrepareViewRender();
            views_[i]->Render();

            SendEvent(E_ENDVIEWRENDER, eventData);
        }

        // Copy the number of batches & primitives from Graphics so that we can account for 3D geometry only
        numPrimitives_ = graphics_->GetNumPrimitives();
        numBatches_ = graphics_->GetNumBatches();
    }

    // Remove unused occlusion buffers and renderbuffers
    RemoveUnusedBuffers();
}

void Renderer::DrawDebugGeometry(bool depthTest)
{
    URHO3D_PROFILE(RendererDrawDebug);

    /// \todo Because debug geometry is per-scene, if two cameras show views of the same area, occlusion is not shown correctly
    HashSet<Drawable*> processedGeometries;
    HashSet<Light*> processedLights;

    for (unsigned i = 0; i < views_.size(); ++i)
    {
        View* view = views_[i];
        if (!view || !view->GetDrawDebug())
            continue;
        Octree* octree = view->GetOctree();
        if (!octree)
            continue;
        DebugRenderer* debug = octree->GetComponent<DebugRenderer>();
        if (!debug || !debug->IsEnabledEffective())
            continue;

        // Process geometries / lights only once
        const std::vector<Drawable*>& geometries = view->GetGeometries();
        const std::vector<Light*>& lights = view->GetLights();

        for (unsigned i = 0; i < geometries.size(); ++i)
        {
            if (!processedGeometries.contains(geometries[i]))
            {
                geometries[i]->DrawDebugGeometry(debug, depthTest);
                processedGeometries.insert(geometries[i]);
            }
        }
        for (unsigned i = 0; i < lights.size(); ++i)
        {
            if (!processedLights.contains(lights[i]))
            {
                lights[i]->DrawDebugGeometry(debug, depthTest);
                processedLights.insert(lights[i]);
            }
        }
    }
}

void Renderer::QueueRenderSurface(RenderSurface* renderTarget)
{
    if (renderTarget)
    {
        unsigned numViewports = renderTarget->GetNumViewports();

        for (unsigned i = 0; i < numViewports; ++i)
            QueueViewport(renderTarget, renderTarget->GetViewport(i));
    }
}

void Renderer::QueueViewport(RenderSurface* renderTarget, Viewport* viewport)
{
    if (viewport)
    {
        std::pair<WeakPtr<RenderSurface>, WeakPtr<Viewport> > newView=std::make_pair(WeakPtr<RenderSurface>(renderTarget),
                    WeakPtr<Viewport>(viewport));

        // Prevent double add of the same rendertarget/viewport combination
        if (std::find(queuedViewports_.begin(),queuedViewports_.end(),newView)==queuedViewports_.end())
            queuedViewports_.push_back(newView);
    }
}

Geometry* Renderer::GetLightGeometry(Light* light)
{
    switch (light->GetLightType())
    {
    case LIGHT_DIRECTIONAL:
        return dirLightGeometry_;
    case LIGHT_SPOT:
        return spotLightGeometry_;
    case LIGHT_POINT:
        return pointLightGeometry_;
    }

    return nullptr;
}

Geometry* Renderer::GetQuadGeometry()
{
    return dirLightGeometry_;
}

Texture2D* Renderer::GetShadowMap(Light* light, Camera* camera, unsigned viewWidth, unsigned viewHeight)
{
    LightType type = light->GetLightType();
    const FocusParameters& parameters = light->GetShadowFocus();
    float size = (float)shadowMapSize_ * light->GetShadowResolution();
    // Automatically reduce shadow map size when far away
    if (parameters.autoSize_ && type != LIGHT_DIRECTIONAL)
    {
        const Matrix3x4& view = camera->GetView();
        const Matrix4& projection = camera->GetProjection();
        BoundingBox lightBox;
        float lightPixels;

        if (type == LIGHT_POINT)
        {
            // Calculate point light pixel size from the projection of its diagonal
            Vector3 center = view * light->GetNode()->GetWorldPosition();
            float extent = 0.58f * light->GetRange();
            lightBox.Define(center + Vector3(extent, extent, extent), center - Vector3(extent, extent, extent));
        }
        else
        {
            // Calculate spot light pixel size from the projection of its frustum far vertices
            Frustum lightFrustum = light->GetFrustum().Transformed(view);
            lightBox.Define(&lightFrustum.vertices_[4], 4);
        }

        Vector2 projectionSize = lightBox.Projected(projection).Size();
        lightPixels = Max(0.5f * (float)viewWidth * projectionSize.x_, 0.5f * (float)viewHeight * projectionSize.y_);

        // Clamp pixel amount to a sufficient minimum to avoid self-shadowing artifacts due to loss of precision
        if (lightPixels < SHADOW_MIN_PIXELS)
            lightPixels = SHADOW_MIN_PIXELS;

        size = std::min(size, lightPixels);
    }

    /// \todo Allow to specify maximum shadow maps per resolution, as smaller shadow maps take less memory
    int width = NextPowerOfTwo((unsigned)size);
    int height = width;

    // Adjust the size for directional or point light shadow map atlases
    if (type == LIGHT_DIRECTIONAL)
    {
        unsigned numSplits = light->GetNumShadowSplits();
        if (numSplits > 1)
            width *= 2;
        if (numSplits > 2)
            height *= 2;
    }
    else if (type == LIGHT_POINT)
    {
        width *= 2;
        height *= 3;
    }

    int searchKey = (width << 16) | height;
    auto entry = shadowMaps_.find(searchKey);
    if (entry != shadowMaps_.end())
    {
        std::vector<SharedPtr<Texture2D> > &shadowmap(MAP_VALUE(entry));
        // If shadow maps are reused, always return the first
        if (reuseShadowMaps_)
            return shadowmap[0];
        else
        {
            // If not reused, check allocation count and return existing shadow map if possible
            unsigned allocated = shadowMapAllocations_[searchKey].size();
            if (allocated < shadowmap.size())
            {
                shadowMapAllocations_[searchKey].push_back(light);
                return shadowmap[allocated];
            }
            else if ((int)allocated >= maxShadowMaps_)
                return nullptr;
        }
    }

    // Find format and usage of the shadow map
    gl::GLenum shadowMapFormat = gl::GL_NONE;
    TextureUsage shadowMapUsage = TEXTURE_DEPTHSTENCIL;

    switch (shadowQuality_)
    {
    case SHADOWQUALITY_SIMPLE_16BIT:
    case SHADOWQUALITY_PCF_16BIT:
        shadowMapFormat = graphics_->GetShadowMapFormat();
        break;

    case SHADOWQUALITY_SIMPLE_24BIT:
    case SHADOWQUALITY_PCF_24BIT:
        shadowMapFormat = graphics_->GetHiresShadowMapFormat();
        break;

    case SHADOWQUALITY_VSM:
    case SHADOWQUALITY_BLUR_VSM:
        shadowMapFormat = graphics_->GetRGFloat32Format();
        shadowMapUsage = TEXTURE_RENDERTARGET;
        break;
    }

    if (0==shadowMapFormat)
        return nullptr;

    SharedPtr<Texture2D> newShadowMap(new Texture2D(context_));
    int retries = 3;
    gl::GLenum dummyColorFormat = graphics_->GetDummyColorFormat();

    while (retries)
    {
        if (!newShadowMap->SetSize(width, height, shadowMapFormat, shadowMapUsage))
        {
            width >>= 1;
            height >>= 1;
            --retries;
        }
        else
        {
            #ifndef GL_ES_VERSION_2_0
            // OpenGL (desktop) and D3D11: shadow compare mode needs to be specifically enabled for the shadow map
            newShadowMap->SetFilterMode(FILTER_BILINEAR);
            newShadowMap->SetShadowCompare(shadowMapUsage == TEXTURE_DEPTHSTENCIL);
            #endif
            // Create dummy color texture for the shadow map if necessary: Direct3D9, or OpenGL when working around an OS X +
            // Intel driver bug
            if (shadowMapUsage == TEXTURE_DEPTHSTENCIL && 0!=dummyColorFormat)
            {
                // If no dummy color rendertarget for this size exists yet, create one now
                if (!colorShadowMaps_.contains(searchKey))
                {
                    colorShadowMaps_[searchKey] = new Texture2D(context_);
                    colorShadowMaps_[searchKey]->SetSize(width, height, dummyColorFormat, TEXTURE_RENDERTARGET);
                }
                // Link the color rendertarget to the shadow map
                newShadowMap->GetRenderSurface()->SetLinkedRenderTarget(colorShadowMaps_[searchKey]->GetRenderSurface());
            }
            break;
        }
    }

    // If failed to set size, store a null pointer so that we will not retry
    if (!retries)
        newShadowMap.Reset();

    shadowMaps_[searchKey].push_back(newShadowMap);
    if (!reuseShadowMaps_)
        shadowMapAllocations_[searchKey].push_back(light);

    return newShadowMap;
}

Texture* Renderer::GetScreenBuffer(int width, int height, gl::GLenum format, bool cubemap, bool filtered, bool srgb, unsigned persistentKey)
{
    bool depthStencil = (format == Graphics::GetDepthStencilFormat()) || (format == Graphics::GetReadableDepthFormat());
    if (depthStencil)
    {
        filtered = false;
        srgb = false;
    }

    if (cubemap)
        height = width;
    long long searchKey = ((long long)format << 32) | (width << 16) | height;
    if (filtered)
        searchKey |= 0x8000000000000000LL;
    if (srgb)
        searchKey |= 0x4000000000000000LL;
    if (cubemap)
        searchKey |= 0x2000000000000000LL;

    // Add persistent key if defined
    if (persistentKey)
        searchKey += ((long long)persistentKey << 32);

    // If new size or format, initialize the allocation stats
    if (screenBuffers_.find(searchKey) == screenBuffers_.end())
        screenBufferAllocations_[searchKey] = 0;

    // Reuse depth-stencil buffers whenever the size matches, instead of allocating new
    unsigned allocations = screenBufferAllocations_[searchKey];
    if(!depthStencil)
        ++screenBufferAllocations_[searchKey];

    if (allocations >= screenBuffers_[searchKey].size())
    {
        SharedPtr<Texture> newBuffer;

        if (!cubemap)
        {
            SharedPtr<Texture2D> newTex2D(new Texture2D(context_));
            newTex2D->SetSize(width, height, format, depthStencil ? TEXTURE_DEPTHSTENCIL : TEXTURE_RENDERTARGET);
        // OpenGL hack: clear persistent floating point screen buffers to ensure the initial contents aren't illegal (NaN)?
        // Otherwise eg. the AutoExposure post process will not work correctly
        if (persistentKey && Texture::GetDataType(format) == gl::GL_FLOAT)
        {
            // Note: this loses current rendertarget assignment
            graphics_->ResetRenderTargets();
            graphics_->SetRenderTarget(0, newTex2D);
            graphics_->SetDepthStencil((RenderSurface*)nullptr);
            graphics_->SetViewport(IntRect(0, 0, width, height));
            graphics_->Clear(CLEAR_COLOR);
        }
            newBuffer = newTex2D;
        }
        else
        {
            SharedPtr<TextureCube> newTexCube(new TextureCube(context_));
            newTexCube->SetSize(width, format, TEXTURE_RENDERTARGET);

            newBuffer = newTexCube;
        }

        newBuffer->SetSRGB(srgb);
        newBuffer->SetFilterMode(filtered ? FILTER_BILINEAR : FILTER_NEAREST);
        newBuffer->ResetUseTimer();
        screenBuffers_[searchKey].push_back(newBuffer);

        URHO3D_LOGDEBUG(QString("Allocated new screen buffer size %1x%2 format %3").arg(width).arg(height).arg((unsigned)format));
        return newBuffer;
    }
    else
    {
        Texture* buffer = screenBuffers_[searchKey][allocations];
        buffer->ResetUseTimer();
        return buffer;
    }
}

RenderSurface* Renderer::GetDepthStencil(int width, int height)
{
    // Return the default depth-stencil surface if applicable
    // (when using OpenGL Graphics will allocate right size surfaces on demand to emulate Direct3D9)
    if (width == graphics_->GetWidth() && height == graphics_->GetHeight() && graphics_->GetMultiSample() <= 1)
        return nullptr;
    else
    {
        return static_cast<Texture2D*>(GetScreenBuffer(width, height, Graphics::GetDepthStencilFormat(), false, false, false))->
            GetRenderSurface();
    }
}

OcclusionBuffer* Renderer::GetOcclusionBuffer(Camera* camera)
{
    assert(numOcclusionBuffers_ <= occlusionBuffers_.size());
    if (numOcclusionBuffers_ == occlusionBuffers_.size())
    {
        SharedPtr<OcclusionBuffer> newBuffer(new OcclusionBuffer(context_));
        occlusionBuffers_.push_back(newBuffer);
    }

    int width = occlusionBufferSize_;
    int height = (int)((float)occlusionBufferSize_ / camera->GetAspectRatio() + 0.5f);

    OcclusionBuffer* buffer = occlusionBuffers_[numOcclusionBuffers_++];
    buffer->SetSize(width, height, threadedOcclusion_);
    buffer->SetView(camera);
    buffer->ResetUseTimer();

    return buffer;
}

Camera* Renderer::GetShadowCamera()
{
    MutexLock lock(rendererMutex_);

    assert(numShadowCameras_ <= shadowCameraNodes_.size());
    if (numShadowCameras_ == shadowCameraNodes_.size())
    {
        SharedPtr<Node> newNode(new Node(context_));
        newNode->CreateComponent<Camera>();
        shadowCameraNodes_.push_back(newNode);
    }

    Camera* camera = shadowCameraNodes_[numShadowCameras_++]->GetComponent<Camera>();
    camera->SetOrthographic(false);
    camera->SetZoom(1.0f);

    return camera;
}

void Renderer::StorePreparedView(View* view, Camera* camera)
{
    if (view && camera)
        preparedViews_[camera] = view;
}

View* Renderer::GetPreparedView(Camera* camera)
{
    HashMap<Camera*, WeakPtr<View> >::iterator i = preparedViews_.find(camera);
    return i != preparedViews_.end() ? MAP_VALUE(i) : (View*)nullptr;
}

View* Renderer::GetActualView(View* view)
{
    if (view && view->GetSourceView())
        return view->GetSourceView();
    else
        return view;
}

void Renderer::SetBatchShaders(Batch& batch, const Technique* tech, bool allowShadows)
{
    // Check if shaders are unloaded or need reloading
    Pass* pass = batch.pass_;
    std::vector<SharedPtr<ShaderVariation> >& vertexShaders = pass->GetVertexShaders();
    std::vector<SharedPtr<ShaderVariation> >& pixelShaders = pass->GetPixelShaders();
    if (vertexShaders.empty() || pixelShaders.empty() || pass->GetShadersLoadedFrameNumber() != shadersChangedFrameNumber_)
    {
        // First release all previous shaders, then load
        pass->ReleaseShaders();
        LoadPassShaders(pass);
    }

    // Make sure shaders are loaded now
    if (vertexShaders.size() && pixelShaders.size())
    {
        bool heightFog = batch.zone_ && batch.zone_->GetHeightFog();

        // If instancing is not supported, but was requested, choose static geometry vertex shader instead
        if (batch.geometryType_ == GEOM_INSTANCED && !GetDynamicInstancing())
            batch.geometryType_ = GEOM_STATIC;

        if (batch.geometryType_ == GEOM_STATIC_NOINSTANCING)
            batch.geometryType_ = GEOM_STATIC;

        //  Check whether is a pixel lit forward pass. If not, there is only one pixel shader
        if (pass->GetLightingMode() == LIGHTING_PERPIXEL)
        {
            LightBatchQueue* lightQueue = batch.lightQueue_;
            if (!lightQueue)
            {
                // Do not log error, as it would result in a lot of spam
                batch.vertexShader_ = nullptr;
                batch.pixelShader_ = nullptr;
                return;
            }

            Light* light = lightQueue->light_;
            unsigned vsi = 0;
            unsigned psi = 0;
            vsi = batch.geometryType_ * MAX_LIGHT_VS_VARIATIONS;

            bool materialHasSpecular = batch.material_ ? batch.material_->GetSpecular() : true;
            if (specularLighting_ && light->GetSpecularIntensity() > 0.0f && materialHasSpecular)
                psi += LPS_SPEC;
            if (allowShadows && lightQueue->shadowMap_)
            {
                vsi += LVS_SHADOW;
                psi += LPS_SHADOW;
            }

            switch (light->GetLightType())
            {
            case LIGHT_DIRECTIONAL:
                vsi += LVS_DIR;
                break;

            case LIGHT_SPOT:
                psi += LPS_SPOT;
                vsi += LVS_SPOT;
                break;

            case LIGHT_POINT:
                if (light->GetShapeTexture())
                    psi += LPS_POINTMASK;
                else
                    psi += LPS_POINT;
                vsi += LVS_POINT;
                break;
            }

            if (heightFog)
                psi += MAX_LIGHT_PS_VARIATIONS;

            batch.vertexShader_ = vertexShaders[vsi];
            batch.pixelShader_ = pixelShaders[psi];
        }
        else
        {
            // Check if pass has vertex lighting support
            if (pass->GetLightingMode() == LIGHTING_PERVERTEX)
            {
                unsigned numVertexLights = 0;
                if (batch.lightQueue_)
                    numVertexLights = batch.lightQueue_->vertexLights_.size();

                unsigned vsi = batch.geometryType_ * MAX_VERTEXLIGHT_VS_VARIATIONS + numVertexLights;
                batch.vertexShader_ = vertexShaders[vsi];
            }
            else
            {
                unsigned vsi = batch.geometryType_;
                batch.vertexShader_ = vertexShaders[vsi];
            }

            batch.pixelShader_ = pixelShaders[heightFog ? 1 : 0];
        }
    }

    // Log error if shaders could not be assigned, but only once per technique
    if (!batch.vertexShader_ || !batch.pixelShader_)
    {
        if (!shaderErrorDisplayed_.contains(tech))
        {
            shaderErrorDisplayed_.insert(tech);
            URHO3D_LOGERROR("Technique " + tech->GetName() + " has missing shaders");
        }
    }
}

void Renderer::SetLightVolumeBatchShaders(Batch& batch, Camera* camera, const QString& vsName, const QString& psName, const QString& vsDefines, const QString& psDefines)
{
    assert(deferredLightPSVariations_.size());

    unsigned vsi = DLVS_NONE;
    unsigned psi = DLPS_NONE;
    Light* light = batch.lightQueue_->light_;

    switch (light->GetLightType())
    {
    case LIGHT_DIRECTIONAL:
        vsi += DLVS_DIR;
        break;

    case LIGHT_SPOT:
        psi += DLPS_SPOT;
        break;

    case LIGHT_POINT:
        if (light->GetShapeTexture())
            psi += DLPS_POINTMASK;
        else
            psi += DLPS_POINT;
        break;
    }

    if (batch.lightQueue_->shadowMap_)
        psi += DLPS_SHADOW;

    if (specularLighting_ && light->GetSpecularIntensity() > 0.0f)
        psi += DLPS_SPEC;

    if (camera->IsOrthographic())
    {
        vsi += DLVS_ORTHO;
        psi += DLPS_ORTHO;
    }

    if (vsDefines.length())
        batch.vertexShader_ = graphics_->GetShader(VS, vsName, deferredLightVSVariations[vsi] + vsDefines);
    else
    batch.vertexShader_ = graphics_->GetShader(VS, vsName, deferredLightVSVariations[vsi]);
    if (psDefines.length())
        batch.pixelShader_ = graphics_->GetShader(PS, psName, deferredLightPSVariations_[psi] + psDefines);
    else
    batch.pixelShader_ = graphics_->GetShader(PS, psName, deferredLightPSVariations_[psi]);
}

void Renderer::SetCullMode(CullMode mode, const Camera *camera)
{
    // If a camera is specified, check whether it reverses culling due to vertical flipping or reflection
    if (camera && camera->GetReverseCulling())
    {
        if (mode == CULL_CW)
            mode = CULL_CCW;
        else if (mode == CULL_CCW)
            mode = CULL_CW;
    }

    graphics_->SetCullMode(mode);
}

bool Renderer::ResizeInstancingBuffer(unsigned numInstances)
{
    if (!instancingBuffer_ || !dynamicInstancing_)
        return false;

    unsigned oldSize = instancingBuffer_->GetVertexCount();
    if (numInstances <= oldSize)
        return true;

    unsigned newSize = INSTANCING_BUFFER_DEFAULT_SIZE;
    while (newSize < numInstances)
        newSize <<= 1;

    if (!instancingBuffer_->SetSize(newSize, INSTANCING_BUFFER_MASK, true))
    {
        URHO3D_LOGERROR("Failed to resize instancing buffer to " + QString::number(newSize));
        // If failed, try to restore the old size
        instancingBuffer_->SetSize(oldSize, INSTANCING_BUFFER_MASK, true);
        return false;
    }

    URHO3D_LOGDEBUG("Resized instancing buffer to " + QString::number(newSize));
    return true;
}

void Renderer::SaveScreenBufferAllocations()
{
    savedScreenBufferAllocations_ = screenBufferAllocations_;
}

void Renderer::RestoreScreenBufferAllocations()
{
    screenBufferAllocations_ = savedScreenBufferAllocations_;
}


void Renderer::OptimizeLightByScissor(Light* light, Camera* camera)
{
    if (light && light->GetLightType() != LIGHT_DIRECTIONAL)
        graphics_->SetScissorTest(true, GetLightScissor(light, camera));
    else
        graphics_->SetScissorTest(false);
}

void Renderer::OptimizeLightByStencil(Light* light, Camera* camera)
{
    if (light)
    {
        LightType type = light->GetLightType();
        if (type == LIGHT_DIRECTIONAL)
        {
            graphics_->SetStencilTest(false);
            return;
        }

        Geometry* geometry = GetLightGeometry(light);
        const Matrix3x4& view = camera->GetView();
        const Matrix4& projection = camera->GetProjection();
        Vector3 cameraPos = camera->GetNode()->GetWorldPosition();
        float lightDist;

        if (type == LIGHT_POINT)
            lightDist = Sphere(light->GetNode()->GetWorldPosition(), light->GetRange() * 1.25f).Distance(cameraPos);
        else
            lightDist = light->GetFrustum().Distance(cameraPos);

        // If the camera is actually inside the light volume, do not draw to stencil as it would waste fillrate
        if (lightDist < M_EPSILON)
        {
            graphics_->SetStencilTest(false);
            return;
        }

        // If the stencil value has wrapped, clear the whole stencil first
        if (!lightStencilValue_)
        {
            graphics_->Clear(CLEAR_STENCIL);
            lightStencilValue_ = 1;
        }

        // If possible, render the stencil volume front faces. However, close to the near clip plane render back faces instead
        // to avoid clipping.
        if (lightDist < camera->GetNearClip() * 2.0f)
        {
            SetCullMode(CULL_CW, camera);
            graphics_->SetDepthTest(CMP_GREATER);
        }
        else
        {
            SetCullMode(CULL_CCW, camera);
            graphics_->SetDepthTest(CMP_LESSEQUAL);
        }

        graphics_->SetColorWrite(false);
        graphics_->SetDepthWrite(false);
        graphics_->SetStencilTest(true, CMP_ALWAYS, OP_REF, OP_KEEP, OP_KEEP, lightStencilValue_);
        graphics_->SetShaders(graphics_->GetShader(VS, "Stencil"), graphics_->GetShader(PS, "Stencil"));
        graphics_->SetShaderParameter(VSP_VIEW, view);
        graphics_->SetShaderParameter(VSP_VIEWINV, camera->GetEffectiveWorldTransform());
        graphics_->SetShaderParameter(VSP_VIEWPROJ, projection * view);
        graphics_->SetShaderParameter(VSP_MODEL, light->GetVolumeTransform(camera));

        geometry->Draw(graphics_);

        graphics_->ClearTransformSources();
        graphics_->SetColorWrite(true);
        graphics_->SetStencilTest(true, CMP_EQUAL, OP_KEEP, OP_KEEP, OP_KEEP, lightStencilValue_);

        // Increase stencil value for next light
        ++lightStencilValue_;
    }
    else
        graphics_->SetStencilTest(false);
}

const Rect& Renderer::GetLightScissor(Light* light, Camera* camera)
{
    std::pair<Light*, Camera*> combination(light, camera);

    HashMap<std::pair<Light*, Camera*>, Rect>::iterator i = lightScissorCache_.find(combination);
    if (i != lightScissorCache_.end())
        return MAP_VALUE(i);

    const Matrix3x4& view = camera->GetView();
    const Matrix4& projection = camera->GetProjection();

    assert(light->GetLightType() != LIGHT_DIRECTIONAL);
    if (light->GetLightType() == LIGHT_SPOT)
    {
        Frustum viewFrustum(light->GetFrustum().Transformed(view));
        return lightScissorCache_[combination] = viewFrustum.Projected(projection);
    }
    else // LIGHT_POINT
    {
        BoundingBox viewBox(light->GetWorldBoundingBox().Transformed(view));
        return lightScissorCache_[combination] = viewBox.Projected(projection);
    }
}

void Renderer::PrepareViewRender()
{
    ResetScreenBufferAllocations();
    lightScissorCache_.clear();
    lightStencilValue_ = 1;
}

void Renderer::RemoveUnusedBuffers()
{
    for (unsigned i = occlusionBuffers_.size() - 1; i < occlusionBuffers_.size(); --i)
    {
        if (occlusionBuffers_[i]->GetUseTimer() > MAX_BUFFER_AGE)
        {
            URHO3D_LOGDEBUG("Removed unused occlusion buffer");
            occlusionBuffers_.erase(occlusionBuffers_.begin()+i);
        }
    }

    for (auto i = screenBuffers_.begin(); i != screenBuffers_.end();)
    {
        std::vector<SharedPtr<Texture> >& buffers = MAP_VALUE(i);
        for (unsigned j = buffers.size() - 1; j < buffers.size(); --j)
        {
            Texture* buffer = buffers[j];
            if (buffer->GetUseTimer() > MAX_BUFFER_AGE)
            {
                URHO3D_LOGDEBUG(QString("Removed unused screen buffer size %1x%2 format %3")
                         .arg(buffer->GetWidth()).arg(buffer->GetHeight()).arg((unsigned)buffer->GetFormat()));
                buffers.erase(buffers.begin()+j);
            }
        }
        if (buffers.empty())
        {
            screenBufferAllocations_.remove(MAP_KEY(i));
            i = screenBuffers_.erase(i);
        }
        else
            ++i;
    }
}

void Renderer::ResetShadowMapAllocations()
{
    for (auto & elem : shadowMapAllocations_)
        ELEMENT_VALUE(elem).clear();
}

void Renderer::ResetScreenBufferAllocations()
{
    for (auto & elem : screenBufferAllocations_)
        ELEMENT_VALUE(elem) = 0;
}

void Renderer::Initialize()
{
    Graphics* graphics = GetSubsystem<Graphics>();
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    if (!graphics || !graphics->IsInitialized() || !cache)
        return;

    URHO3D_PROFILE(InitRenderer);

    graphics_ = graphics;

    if (0==graphics_->GetShadowMapFormat())
        drawShadows_ = false;
    // Validate the shadow quality level
    SetShadowQuality(shadowQuality_);

    defaultLightRamp_ = cache->GetResource<Texture2D>("Textures/Ramp.png");
    defaultLightSpot_ = cache->GetResource<Texture2D>("Textures/Spot.png");
    defaultMaterial_ = new Material(context_);

    defaultRenderPath_ = new RenderPath();
    defaultRenderPath_->Load(cache->GetResource<XMLFile>("RenderPaths/Forward.xml"));

    CreateGeometries();
    CreateInstancingBuffer();

    viewports_.resize(1);
    ResetShadowMaps();
    ResetBuffers();

    shadersDirty_ = true;
    initialized_ = true;

    SubscribeToEvent(E_RENDERUPDATE, URHO3D_HANDLER(Renderer, HandleRenderUpdate));

    URHO3D_LOGINFO("Initialized renderer");
}

void Renderer::LoadShaders()
{
    URHO3D_LOGDEBUG("Reloading shaders");

    // Release old material shaders, mark them for reload
    ReleaseMaterialShaders();
    shadersChangedFrameNumber_ = GetSubsystem<Time>()->GetFrameNumber();

    // Construct new names for deferred light volume pixel shaders based on rendering options
    deferredLightPSVariations_.reserve(MAX_DEFERRED_LIGHT_PS_VARIATIONS);
    for (unsigned i = 0; i < MAX_DEFERRED_LIGHT_PS_VARIATIONS; ++i)
    {
        QString entry = lightPSVariations[i % DLPS_ORTHO];
        if (i & DLPS_SHADOW)
            entry += GetShadowVariations();
        if (i & DLPS_ORTHO)
            entry += "ORTHO";
        deferredLightPSVariations_ << entry;
    }

    shadersDirty_ = false;
}

void Renderer::LoadPassShaders(Pass* pass)
{

    URHO3D_PROFILE(LoadPassShaders);


    std::vector<SharedPtr<ShaderVariation> >& vertexShaders = pass->GetVertexShaders();
    std::vector<SharedPtr<ShaderVariation> >& pixelShaders = pass->GetPixelShaders();

    // Forget all the old shaders
    vertexShaders.clear();
    pixelShaders.clear();
    QString extraShaderDefines = " ";
    if (pass->GetName() == "shadow"
        && (shadowQuality_ == SHADOWQUALITY_VSM || shadowQuality_ == SHADOWQUALITY_BLUR_VSM))
    {
        extraShaderDefines = " VSM_SHADOW ";
    }

    if (pass->GetLightingMode() == LIGHTING_PERPIXEL)
    {
        // Load forward pixel lit variations
        vertexShaders.resize(MAX_GEOMETRYTYPES * MAX_LIGHT_VS_VARIATIONS);
        pixelShaders.resize(MAX_LIGHT_PS_VARIATIONS * 2);

        for (unsigned j = 0; j < MAX_GEOMETRYTYPES * MAX_LIGHT_VS_VARIATIONS; ++j)
        {
            unsigned g = j / MAX_LIGHT_VS_VARIATIONS;
            unsigned l = j % MAX_LIGHT_VS_VARIATIONS;

            vertexShaders[j] = graphics_->GetShader(VS, pass->GetVertexShader(),
                pass->GetVertexShaderDefines() + extraShaderDefines + lightVSVariations[l] + geometryVSVariations[g]);
        }
        for (unsigned j = 0; j < MAX_LIGHT_PS_VARIATIONS * 2; ++j)
        {
            unsigned l = j % MAX_LIGHT_PS_VARIATIONS;
            unsigned h = j / MAX_LIGHT_PS_VARIATIONS;

            if (l & LPS_SHADOW)
            {
                pixelShaders[j] = graphics_->GetShader(PS, pass->GetPixelShader(),
                    pass->GetPixelShaderDefines() + extraShaderDefines + lightPSVariations[l] + GetShadowVariations() +
                    heightFogVariations[h]);
            }
            else
                pixelShaders[j] = graphics_->GetShader(PS, pass->GetPixelShader(),
                    pass->GetPixelShaderDefines() + extraShaderDefines + lightPSVariations[l] + heightFogVariations[h]);
        }
    }
    else
    {
        // Load vertex light variations
        if (pass->GetLightingMode() == LIGHTING_PERVERTEX)
        {
            vertexShaders.resize(MAX_GEOMETRYTYPES * MAX_VERTEXLIGHT_VS_VARIATIONS);
            for (unsigned j = 0; j < MAX_GEOMETRYTYPES * MAX_VERTEXLIGHT_VS_VARIATIONS; ++j)
            {
                unsigned g = j / MAX_VERTEXLIGHT_VS_VARIATIONS;
                unsigned l = j % MAX_VERTEXLIGHT_VS_VARIATIONS;
                vertexShaders[j] = graphics_->GetShader(VS, pass->GetVertexShader(),
                    pass->GetVertexShaderDefines() + extraShaderDefines + vertexLightVSVariations[l] + geometryVSVariations[g]);
            }
        }
        else
        {
            vertexShaders.resize(MAX_GEOMETRYTYPES);
            for (unsigned j = 0; j < MAX_GEOMETRYTYPES; ++j)
            {
                vertexShaders[j] = graphics_->GetShader(VS, pass->GetVertexShader(),
                    pass->GetVertexShaderDefines() + extraShaderDefines + geometryVSVariations[j]);
            }
        }

        pixelShaders.resize(2);
        for (unsigned j = 0; j < 2; ++j)
        {
            pixelShaders[j] =
                graphics_->GetShader(PS, pass->GetPixelShader(), pass->GetPixelShaderDefines() + extraShaderDefines + heightFogVariations[j]);
        }
    }

    pass->MarkShadersLoaded(shadersChangedFrameNumber_);
}

void Renderer::ReleaseMaterialShaders()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    std::vector<Material*> materials;

    cache->GetResources<Material>(materials);

    for (unsigned i = 0; i < materials.size(); ++i)
        materials[i]->ReleaseShaders();
}

void Renderer::ReloadTextures()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    std::vector<Resource*> textures;

    cache->GetResources(textures, Texture2D::GetTypeStatic());
    for (unsigned i = 0; i < textures.size(); ++i)
        cache->ReloadResource(textures[i]);

    cache->GetResources(textures, TextureCube::GetTypeStatic());
    for (unsigned i = 0; i < textures.size(); ++i)
        cache->ReloadResource(textures[i]);
}

void Renderer::CreateGeometries()
{
    SharedPtr<VertexBuffer> dlvb(new VertexBuffer(context_));
    dlvb->SetShadowed(true);
    dlvb->SetSize(4, MASK_POSITION);
    dlvb->SetData(dirLightVertexData);

    SharedPtr<IndexBuffer> dlib(new IndexBuffer(context_));
    dlib->SetShadowed(true);
    dlib->SetSize(6, false);
    dlib->SetData(dirLightIndexData);

    dirLightGeometry_ = new Geometry(context_);
    dirLightGeometry_->SetVertexBuffer(0, dlvb);
    dirLightGeometry_->SetIndexBuffer(dlib);
    dirLightGeometry_->SetDrawRange(TRIANGLE_LIST, 0, dlib->GetIndexCount());

    SharedPtr<VertexBuffer> slvb(new VertexBuffer(context_));
    slvb->SetShadowed(true);
    slvb->SetSize(8, MASK_POSITION);
    slvb->SetData(spotLightVertexData);

    SharedPtr<IndexBuffer> slib(new IndexBuffer(context_));
    slib->SetShadowed(true);
    slib->SetSize(36, false);
    slib->SetData(spotLightIndexData);

    spotLightGeometry_ = new Geometry(context_);
    spotLightGeometry_->SetVertexBuffer(0, slvb);
    spotLightGeometry_->SetIndexBuffer(slib);
    spotLightGeometry_->SetDrawRange(TRIANGLE_LIST, 0, slib->GetIndexCount());

    SharedPtr<VertexBuffer> plvb(new VertexBuffer(context_));
    plvb->SetShadowed(true);
    plvb->SetSize(24, MASK_POSITION);
    plvb->SetData(pointLightVertexData);

    SharedPtr<IndexBuffer> plib(new IndexBuffer(context_));
    plib->SetShadowed(true);
    plib->SetSize(132, false);
    plib->SetData(pointLightIndexData);

    pointLightGeometry_ = new Geometry(context_);
    pointLightGeometry_->SetVertexBuffer(0, plvb);
    pointLightGeometry_->SetIndexBuffer(plib);
    pointLightGeometry_->SetDrawRange(TRIANGLE_LIST, 0, plib->GetIndexCount());

    #if !defined(URHO3D_OPENGL) || !defined(GL_ES_VERSION_2_0)
    if (0!=graphics_->GetShadowMapFormat())
    {
        faceSelectCubeMap_ = new TextureCube(context_);
        faceSelectCubeMap_->SetNumLevels(1);
        faceSelectCubeMap_->SetSize(1, graphics_->GetRGBAFormat());
        faceSelectCubeMap_->SetFilterMode(FILTER_NEAREST);

        indirectionCubeMap_ = new TextureCube(context_);
        indirectionCubeMap_->SetNumLevels(1);
        indirectionCubeMap_->SetSize(256, graphics_->GetRGBAFormat());
        indirectionCubeMap_->SetFilterMode(FILTER_BILINEAR);
        indirectionCubeMap_->SetAddressMode(COORD_U, ADDRESS_CLAMP);
        indirectionCubeMap_->SetAddressMode(COORD_V, ADDRESS_CLAMP);
        indirectionCubeMap_->SetAddressMode(COORD_W, ADDRESS_CLAMP);

        SetIndirectionTextureData();
    }
    #endif
}

void Renderer::SetIndirectionTextureData()
{
    unsigned char data[256 * 256 * 4];

    for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
    {
        unsigned axis = i / 2;
        data[0] = (axis == 0) ? 255 : 0;
        data[1] = (axis == 1) ? 255 : 0;
        data[2] = (axis == 2) ? 255 : 0;
        data[3] = 0;
        faceSelectCubeMap_->SetData((CubeMapFace)i, 0, 0, 0, 1, 1, data);
    }

    for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
    {
        unsigned char faceX = (i & 1) * 255;
        unsigned char faceY = (i / 2) * 255 / 3;
        unsigned char* dest = data;
        for (unsigned y = 0; y < 256; ++y)
        {
            for (unsigned x = 0; x < 256; ++x)
            {
                dest[0] = x;
                dest[1] = 255 - y;
                dest[2] = faceX;
                dest[3] = 255 * 2 / 3 - faceY;
                dest += 4;
            }
        }

        indirectionCubeMap_->SetData((CubeMapFace)i, 0, 0, 0, 256, 256, data);
    }

    faceSelectCubeMap_->ClearDataLost();
    indirectionCubeMap_->ClearDataLost();
}

void Renderer::CreateInstancingBuffer()
{
    // Do not create buffer if instancing not supported
    if (!graphics_->GetInstancingSupport())
    {
        instancingBuffer_.Reset();
        dynamicInstancing_ = false;
        return;
    }

    instancingBuffer_ = new VertexBuffer(context_);
    if (!instancingBuffer_->SetSize(INSTANCING_BUFFER_DEFAULT_SIZE, INSTANCING_BUFFER_MASK, true))
    {
        instancingBuffer_.Reset();
        dynamicInstancing_ = false;
    }
}

void Renderer::ResetShadowMaps()
{
    shadowMaps_.clear();
    shadowMapAllocations_.clear();
    colorShadowMaps_.clear();
}

void Renderer::ResetBuffers()
{
    occlusionBuffers_.clear();
    screenBuffers_.clear();
    screenBufferAllocations_.clear();
}
QString Renderer::GetShadowVariations() const
{
    switch (shadowQuality_)
    {
        case SHADOWQUALITY_SIMPLE_16BIT:
            return "SIMPLE_SHADOW ";
        case SHADOWQUALITY_SIMPLE_24BIT:
            return "SIMPLE_SHADOW ";
        case SHADOWQUALITY_PCF_16BIT:
            return "PCF_SHADOW ";
        case SHADOWQUALITY_PCF_24BIT:
            return "PCF_SHADOW ";
        case SHADOWQUALITY_VSM:
            return "VSM_SHADOW ";
        case SHADOWQUALITY_BLUR_VSM:
            return "VSM_SHADOW ";
    }
    return "";
};

void Renderer::HandleScreenMode(StringHash eventType, VariantMap& eventData)
{
    if (!initialized_)
        Initialize();
    else
        resetViews_ = true;
}

void Renderer::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace RenderUpdate;

    Update(eventData[P_TIMESTEP].GetFloat());
}

void Renderer::BlurShadowMap(View* view, Texture2D* shadowMap)
{
    graphics_->SetBlendMode(BLEND_REPLACE);
    graphics_->SetDepthTest(CMP_ALWAYS);
    graphics_->SetClipPlane(false);
    graphics_->SetScissorTest(false);

    // Get a temporary render buffer
    Texture2D* tmpBuffer = static_cast<Texture2D*>(GetScreenBuffer(shadowMap->GetWidth(), shadowMap->GetHeight(), shadowMap->GetFormat(), false, false, false));
    graphics_->SetRenderTarget(0, tmpBuffer->GetRenderSurface());
    graphics_->SetDepthStencil(GetDepthStencil(shadowMap->GetWidth(), shadowMap->GetHeight()));
    graphics_->SetViewport(IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));

    // Get shaders
    static const QString shaderName("ShadowBlur");
    ShaderVariation* vs = graphics_->GetShader(VS, shaderName);
    ShaderVariation* ps = graphics_->GetShader(PS, shaderName);
    graphics_->SetShaders(vs, ps);

    view->SetGBufferShaderParameters(IntVector2(shadowMap->GetWidth(), shadowMap->GetHeight()), IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));

    // Horizontal blur of the shadow map
    static const StringHash blurOffsetParam("BlurOffsets");
    graphics_->SetShaderParameter(blurOffsetParam, Vector2(shadowSoftness_ / shadowMap->GetWidth(), 0.0f));

    graphics_->SetTexture(TU_DIFFUSE, shadowMap);
    view->DrawFullscreenQuad(false);

    // Vertical blur
    graphics_->SetRenderTarget(0, shadowMap);
    graphics_->SetViewport(IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));

    graphics_->SetShaderParameter(blurOffsetParam, Vector2(0.0f, shadowSoftness_ / shadowMap->GetHeight()));

    graphics_->SetTexture(TU_DIFFUSE, tmpBuffer);
    view->DrawFullscreenQuad(false);
}
}
