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

#include "../Core/Profiler.h"
#include "../Core/WorkQueue.h"
#include "../Graphics/Camera.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/GraphicsImpl.h"
#include "../Graphics/Material.h"
#include "../Graphics/OcclusionBuffer.h"
#include "../Graphics/Octree.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/ShaderVariation.h"
#include "../Graphics/Skybox.h"
#include "../Graphics/Technique.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/Texture2DArray.h"
#include "../Graphics/Texture3D.h"
#include "../Graphics/TextureCube.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/View.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#ifdef LUTEFISK3D_UI
#include "../UI/UI.h"
#endif

#include <QDebug>
#include <algorithm>
namespace Urho3D
{

static constexpr const Vector3* directions[] =
{
    &Vector3::RIGHT,
    &Vector3::LEFT,
    &Vector3::UP,
    &Vector3::DOWN,
    &Vector3::FORWARD,
    &Vector3::BACK
};

/// %Frustum octree query for shadowcasters.
class ShadowCasterOctreeQuery : public FrustumOctreeQuery
{
public:
    /// Construct with frustum and query parameters.
    ShadowCasterOctreeQuery(std::vector<Drawable*>& result, const Frustum& frustum, unsigned char drawableFlags = DRAWABLE_ANY,
                            unsigned viewMask = DEFAULT_VIEWMASK) :
        FrustumOctreeQuery(result, frustum, drawableFlags, viewMask)
    {
    }

    /// Intersection test for drawables.
    void TestDrawables(Drawable** start, Drawable** end, bool inside) override
    {
        while (start != end)
        {
            Drawable* drawable = *start++;

            if (drawable->GetCastShadows() && ((drawable->GetDrawableFlags() & drawableFlags_) != 0) &&
                    ((drawable->GetViewMask() & viewMask_) != 0u))
            {
                if (inside || (frustum_.IsInsideFast(drawable->GetWorldBoundingBox()) != 0u))
                    result_.push_back(drawable);
            }
        }
    }
};

/// %Frustum octree query for zones and occluders.
class ZoneOccluderOctreeQuery : public FrustumOctreeQuery
{
public:
    /// Construct with frustum and query parameters.
    ZoneOccluderOctreeQuery(std::vector<Drawable*>& result, const Frustum& frustum, unsigned char drawableFlags = DRAWABLE_ANY,
                            unsigned viewMask = DEFAULT_VIEWMASK) :
        FrustumOctreeQuery(result, frustum, drawableFlags, viewMask)
    {
    }

    /// Intersection test for drawables.
    void TestDrawables(Drawable** start, Drawable** end, bool inside) override
    {
        while (start != end)
        {
            Drawable* drawable = *start++;
            unsigned char flags = drawable->GetDrawableFlags();

            if ((flags == DRAWABLE_ZONE || (flags == DRAWABLE_GEOMETRY &&
                                            drawable->IsOccluder())) && ((drawable->GetViewMask() & viewMask_) != 0u))
            {
                if (inside || (frustum_.IsInsideFast(drawable->GetWorldBoundingBox()) != 0u))
                    result_.push_back(drawable);
            }
        }
    }
};

/// %Frustum octree query with occlusion.
class OccludedFrustumOctreeQuery : public FrustumOctreeQuery
{
public:
    /// Construct with frustum, occlusion buffer and query parameters.
    OccludedFrustumOctreeQuery(std::vector<Drawable*>& result, const Frustum& frustum, OcclusionBuffer* buffer,
                               unsigned char drawableFlags = DRAWABLE_ANY, unsigned viewMask = DEFAULT_VIEWMASK) :
        FrustumOctreeQuery(result, frustum, drawableFlags, viewMask),
        buffer_(buffer)
    {
    }

    /// Intersection test for an octant.
    Intersection TestOctant(const BoundingBox& box, bool inside) override
    {
        if (inside)
            return buffer_->IsVisible(box) ? INSIDE : OUTSIDE;
        else
        {
            Intersection result = frustum_.IsInside(box);
            if (result != OUTSIDE && !buffer_->IsVisible(box))
                result = OUTSIDE;
            return result;
        }
    }

    /// Intersection test for drawables. Note: drawable occlusion is performed later in worker threads.
    void TestDrawables(Drawable** start, Drawable** end, bool inside) override
    {
        while (start != end)
        {
            Drawable* drawable = *start++;

            if (((drawable->GetDrawableFlags() & drawableFlags_) != 0) && ((drawable->GetViewMask() & viewMask_) != 0u))
            {
                if (inside || (frustum_.IsInsideFast(drawable->GetWorldBoundingBox()) != 0u))
                    result_.push_back(drawable);
            }
        }
    }

    /// Occlusion buffer.
    OcclusionBuffer* buffer_;
};
void CheckVisibilityWork(const WorkItem *item, unsigned threadIndex)
{
    View *                 view = reinterpret_cast<View *>(item->aux_);
    const FrameInfo &      frame_info(view->GetFrameInfo());
    Drawable **            start              = reinterpret_cast<Drawable **>(item->start_);
    Drawable **            end                = reinterpret_cast<Drawable **>(item->end_);
    OcclusionBuffer *const occlusion_buffer   = view->GetOcclusionBuffer();
    const Matrix3x4 &      viewMatrix         = view->GetCullCamera()->GetView();
    Vector3                viewZ              = Vector3(viewMatrix.m20_, viewMatrix.m21_, viewMatrix.m22_);
    Vector3                absViewZ           = viewZ.Abs();
    unsigned               cameraViewMask     = view->GetCullCamera()->GetViewMask();
    bool                   cameraZoneOverride = view->cameraZoneOverride_;
    PerThreadSceneResult & result             = view->sceneResults_[threadIndex];
    result.geometries_.reserve(std::distance(start, end));
    while (start != end)
    {
        Drawable *const drawable       = *start++;
        bool            batchesUpdated = false;
        // If draw distance non-zero, update and check it
        float maxDistance = drawable->GetDrawDistance();
        if (maxDistance > 0.0f)
        {
            drawable->UpdateBatches(frame_info);
            batchesUpdated = true;
            if (drawable->GetDistance() > maxDistance)
                continue;
        }
        const BoundingBox &geomBox(drawable->GetWorldBoundingBox());

        uint8_t drawableFlags = drawable->GetDrawableFlags();
        if ((occlusion_buffer != nullptr) && drawable->IsOccludee() && !occlusion_buffer->IsVisible(geomBox))
            continue;
        if (!batchesUpdated)
            drawable->UpdateBatches(frame_info);
        drawable->MarkInView(frame_info);

        // For geometries, find zone, clear lights and calculate view space Z range
        if ((drawableFlags & DRAWABLE_GEOMETRY) != 0u)
        {
            Zone *const drawableZone = drawable->GetZone();
            if (!cameraZoneOverride &&
                    (drawable->IsZoneDirty() || (drawableZone == nullptr) || (drawableZone->GetViewMask() & cameraViewMask) == 0))
                view->FindZone(drawable);

            const Vector3 edge = geomBox.size() * 0.5f;

            // Do not add "infinite" objects like skybox to prevent shadow map focusing behaving erroneously
            if (edge.LengthSquared() < M_LARGE_VALUE * M_LARGE_VALUE)
            {
                const Vector3 center= geomBox.Center();
                float   viewCenterZ = viewZ.DotProduct(center) + viewMatrix.m23_;
                float   viewEdgeZ   = absViewZ.DotProduct(edge);
                float   minZ        = viewCenterZ - viewEdgeZ;
                float   maxZ        = viewCenterZ + viewEdgeZ;
                drawable->SetMinMaxZ(viewCenterZ - viewEdgeZ, viewCenterZ + viewEdgeZ);
                result.minZ_ = std::min(result.minZ_, minZ);
                result.maxZ_ = std::max(result.maxZ_, maxZ);
            }
            else
                drawable->SetMinMaxZ(M_LARGE_VALUE, M_LARGE_VALUE);

            result.geometries_.push_back(drawable);
        }
        else if ((drawableFlags & DRAWABLE_LIGHT) != 0u)
        {
            Light *light = static_cast<Light *>(drawable);
            // Skip lights with zero brightness or black color
            if (!light->GetEffectiveColor().Equals(Color::BLACK))
                result.lights_.push_back(light);
        }
    }
}

void ProcessLightWork(const WorkItem* item, unsigned threadIndex)
{
    View* view = reinterpret_cast<View*>(item->aux_);
    LightQueryResult* query = reinterpret_cast<LightQueryResult*>(item->start_);

    view->ProcessLight(*query, threadIndex);
}
namespace {

void UpdateDrawableGeometriesWork(const WorkItem* item, unsigned threadIndex)
{
    const FrameInfo& frame = *(reinterpret_cast<FrameInfo*>(item->aux_));
    Drawable** start = reinterpret_cast<Drawable**>(item->start_);
    Drawable** end = reinterpret_cast<Drawable**>(item->end_);

    while (start != end)
    {
        Drawable* drawable = *start++;
        // We may leave null pointer holes in the queue if a drawable is found out to require a main thread update
        if (drawable != nullptr)
            drawable->UpdateGeometry(frame);
    }
}

void SortBatchQueueFrontToBackWork(const WorkItem* item, unsigned threadIndex)
{
    BatchQueue* queue = reinterpret_cast<BatchQueue*>(item->start_);

    queue->SortFrontToBack();
}

void SortBatchQueueBackToFrontWork(const WorkItem* item, unsigned threadIndex)
{
    BatchQueue* queue = reinterpret_cast<BatchQueue*>(item->start_);

    queue->SortBackToFront();
}

void SortLightQueueWork(const WorkItem* item, unsigned threadIndex)
{
    LightBatchQueue* start = reinterpret_cast<LightBatchQueue*>(item->start_);
    start->litBaseBatches_.SortFrontToBack();
    start->litBatches_.SortFrontToBack();
}

void SortShadowQueueWork(const WorkItem *item, unsigned threadIndex)
{
    LightBatchQueue *start = reinterpret_cast<LightBatchQueue *>(item->start_);
    for (auto &split : start->shadowSplits_)
        split.shadowBatches_.SortFrontToBack();
}
} // anonymous namespace

StringHash ParseTextureTypeXml(ResourceCache* cache, QString filename);
View::View(Context *context)
    : Object(context),
      graphics_(GetSubsystem<Graphics>()),
      renderer_(GetSubsystem<Renderer>()),
      scene_(nullptr),
      octree_(nullptr),
      camera_(nullptr),
      cullCamera_(nullptr),
      sourceView_(nullptr),
      cameraZone_(nullptr),
      farClipZone_(nullptr),
      occlusionBuffer_(nullptr),
      renderTarget_(nullptr),
      substituteRenderTarget_(nullptr)
{
    // Create octree query and scene results vector for each thread
    unsigned numThreads = GetSubsystem<WorkQueue>()->GetNumThreads() + 1; // Worker threads + main thread
    tempDrawables_.resize(numThreads);
    sceneResults_.resize(numThreads);
    frame_.camera_ = nullptr;
}

bool View::Define(RenderSurface* renderTarget, Viewport* viewport)
{
    sourceView_ = nullptr;
    renderPath_ = viewport->GetRenderPath();
    if (renderPath_ == nullptr)
        return false;
    renderTarget_ = renderTarget;
    drawDebug_ = viewport->GetDrawDebug();

    // Validate the rect and calculate size. If zero rect, use whole rendertarget size
    int rtWidth = renderTarget != nullptr ? renderTarget->GetWidth() : graphics_->GetWidth();
    int rtHeight = renderTarget != nullptr ? renderTarget->GetHeight() : graphics_->GetHeight();
    const IntRect& rect = viewport->GetRect();

    if (rect != IntRect::ZERO)
    {
        viewRect_.left_ = Clamp(rect.left_, 0, rtWidth - 1);
        viewRect_.top_ = Clamp(rect.top_, 0, rtHeight - 1);
        viewRect_.right_ = Clamp(rect.right_, viewRect_.left_ + 1, rtWidth);
        viewRect_.bottom_ = Clamp(rect.bottom_, viewRect_.top_ + 1, rtHeight);
    }
    else
        viewRect_ = IntRect(0, 0, rtWidth, rtHeight);

    viewSize_ = viewRect_.Size();
    rtSize_ = IntVector2(rtWidth, rtHeight);

    // On OpenGL flip the viewport if rendering to a texture for consistent UV addressing with Direct3D9
    if (renderTarget_ != nullptr)
    {
        viewRect_.bottom_ = rtHeight - viewRect_.top_;
        viewRect_.top_ = viewRect_.bottom_ - viewSize_.y_;
    }

    scene_ = viewport->GetScene();
    cullCamera_ = viewport->GetCullCamera();
    camera_ = viewport->GetCamera();
    if (cullCamera_ == nullptr)
        cullCamera_ = camera_;
    else
    {
        // If view specifies a culling camera (view preparation sharing), check if already prepared
        sourceView_ = renderer_->GetPreparedView(cullCamera_);
        if ((sourceView_ != nullptr) && sourceView_->scene_ == scene_ && sourceView_->renderPath_ == renderPath_)
        {
            // Copy properties needed later in rendering
            deferred_           = sourceView_->deferred_;
            deferredAmbient_    = sourceView_->deferredAmbient_;
            useLitBase_         = sourceView_->useLitBase_;
            hasScenePasses_     = sourceView_->hasScenePasses_;
            noStencil_          = sourceView_->noStencil_;
            lightVolumeCommand_ = sourceView_->lightVolumeCommand_;
            octree_             = sourceView_->octree_;
            return true;
        }
        else
        {
            // Mismatch in scene or renderpath, fall back to unique view preparation
            sourceView_ = nullptr;
        }
    }
    // Set default passes
    alphaPassQueueIdx_ = -1;
    gBufferPassIndex_ = M_MAX_UNSIGNED;
    basePassIndex_ = Technique::GetPassIndex("base");
    alphaPassIndex_ = Technique::GetPassIndex("alpha");
    lightPassIndex_ = Technique::GetPassIndex("light");
    litBasePassIndex_ = Technique::GetPassIndex("litbase");
    litAlphaPassIndex_ = Technique::GetPassIndex("litalpha");

    deferred_ = false;
    deferredAmbient_ = false;
    useLitBase_ = false;
    hasScenePasses_ = false;
    noStencil_ = false;
    lightVolumeCommand_ = nullptr;

    scenePasses_.clear();
    geometriesUpdated_ = false;

    for (const RenderPathCommand& command : renderPath_->commands_)
    {
        if (!command.enabled_)
            continue;
        if (!command.depthStencilName_.isEmpty())
        {
            // Using a readable depth texture will disable light stencil optimizations on OpenGL, as for compatibility reasons
            // we are using a depth format without stencil channel
            noStencil_ = true;
            break;
        }
    }

    // Make sure that all necessary batch queues exist
    for (RenderPathCommand& command : renderPath_->commands_)
    {
        if (!command.enabled_)
            continue;

        if (command.type_ == CMD_SCENEPASS)
        {
            hasScenePasses_ = true;

            ScenePassInfo info;
            info.passIndex_ = command.passIndex_ = Technique::GetPassIndex(command.pass_);
            info.allowInstancing_ = command.sortMode_ != SORT_BACKTOFRONT;
            info.markToStencil_ = !noStencil_ && command.markToStencil_;
            info.vertexLights_ = command.vertexLights_;

            // Check scenepass metadata for defining custom passes which interact with lighting
            if (!command.metadata_.isEmpty())
            {
                if (command.metadata_ == "gbuffer")
                    gBufferPassIndex_ = command.passIndex_;
                else if (command.metadata_ == "base" && command.pass_ != "base")
                {
                    basePassIndex_ = command.passIndex_;
                    litBasePassIndex_ = Technique::GetPassIndex("lit" + command.pass_);
                }
                else if (command.metadata_ == "alpha" && command.pass_ != "alpha")
                {
                    alphaPassIndex_ = command.passIndex_;
                    litAlphaPassIndex_ = Technique::GetPassIndex("lit" + command.pass_);
                }
            }

            BatchQueueMap::iterator j = batchQueues_.find(info.passIndex_);
            if (j == batchQueues_.end()) {
                batchQueueStorage_.emplace_back();
                if(-1==alphaPassQueueIdx_ && info.passIndex_==alphaPassIndex_)
                    alphaPassQueueIdx_ = batchQueueStorage_.size()-1;
                j = batchQueues_.emplace(info.passIndex_, batchQueueStorage_.size()-1).first;
            } else if(info.passIndex_==alphaPassIndex_)
                alphaPassQueueIdx_ = j->second;
            info.batchQueueIdx_ = MAP_VALUE(j);

            scenePasses_.push_back(info);
        }
        // Allow a custom forward light pass
        else if (command.type_ == CMD_FORWARDLIGHTS && !command.pass_.isEmpty())
            lightPassIndex_ = command.passIndex_ = Technique::GetPassIndex(command.pass_);
    }

    octree_ = nullptr;
    // Get default zone first in case we do not have zones defined
    cameraZone_ = farClipZone_ = renderer_->GetDefaultZone();

    if (hasScenePasses_)
    {
        if ((scene_ == nullptr) || (cullCamera_ == nullptr) || !cullCamera_->IsEnabledEffective())
            return false;

        // If scene is loading scene content asynchronously, it is incomplete and should not be rendered
        if (scene_->IsAsyncLoading() && scene_->GetAsyncLoadMode() > LOAD_RESOURCES_ONLY)
            return false;

        octree_ = scene_->GetComponent<Octree>();
        if (octree_ == nullptr)
            return false;

        // Do not accept view if camera projection is illegal
        // (there is a possibility of crash if occlusion is used and it can not clip properly)
        if (!cullCamera_->IsProjectionValid())
            return false;
    }


    // Go through commands to check for deferred rendering and other flags

    for (const RenderPathCommand& command : renderPath_->commands_)
    {
        if (!command.enabled_)
            continue;

        // Check if ambient pass and G-buffer rendering happens at the same time
        if (command.type_ == CMD_SCENEPASS && command.outputs_.size() > 1)
        {
            if (CheckViewportWrite(command))
                deferredAmbient_ = true;
        }
        else if (command.type_ == CMD_LIGHTVOLUMES)
        {
            lightVolumeCommand_ = &command;
            deferred_ = true;
        }
        else if (command.type_ == CMD_FORWARDLIGHTS)
            useLitBase_ = command.useLitBase_;
    }


    drawShadows_ = renderer_->GetDrawShadows();
    materialQuality_ = renderer_->GetMaterialQuality();
    maxOccluderTriangles_ = renderer_->GetMaxOccluderTriangles();
    minInstances_ = renderer_->GetMinInstances();

    // Set possible quality overrides from the camera
    // Note that the culling camera is used here (its settings are authoritative) while the render camera
    // will be just used for the final view & projection matrices
    unsigned viewOverrideFlags = cullCamera_ != nullptr ? cullCamera_->GetViewOverrideFlags() : VO_NONE;
    if ((viewOverrideFlags & VO_LOW_MATERIAL_QUALITY) != 0u)
        materialQuality_ = QUALITY_LOW;
    if ((viewOverrideFlags & VO_DISABLE_SHADOWS) != 0u)
        drawShadows_ = false;
    if ((viewOverrideFlags & VO_DISABLE_OCCLUSION) != 0u)
        maxOccluderTriangles_ = 0;

    // Occlusion buffer has constant width. If resulting height would be too large due to aspect ratio, disable occlusion
    if (viewSize_.y_ > viewSize_.x_ * 4)
        maxOccluderTriangles_ = 0;

    return true;
}

void View::Update(const FrameInfo& frame)
{
    // No need to update if using another prepared view
    if (sourceView_ != nullptr)
        return;

    frame_.camera_ = cullCamera_;
    frame_.timeStep_ = frame.timeStep_;
    frame_.frameNumber_ = frame.frameNumber_;
    frame_.viewSize_ = viewSize_;

    using namespace BeginViewUpdate;

    SendViewEvent(E_BEGINVIEWUPDATE);

    int maxSortedInstances = renderer_->GetMaxSortedInstances();

    // Clear buffers, geometry, light, occluder & batch list
    renderTargets_.clear();
    geometries_.clear();
    lights_.clear();
    zones_.clear();
    occluders_.clear();
    activeOccluders_ = 0;
    vertexLightQueues_.clear();
    for (BatchQueue &elem : batchQueueStorage_)
        elem.Clear(maxSortedInstances);

    if (hasScenePasses_ && ((cullCamera_ == nullptr) || (octree_ == nullptr)))
    {
        SendViewEvent(E_ENDVIEWUPDATE);
        return;
    }

    // Set automatic aspect ratio if required
    if ((cullCamera_ != nullptr) && cullCamera_->GetAutoAspectRatio())
        cullCamera_->SetAspectRatioInternal((float)frame_.viewSize_.x_ / (float)frame_.viewSize_.y_);

    GetDrawables();
    GetBatches();
    renderer_->StorePreparedView(this, cullCamera_);

    SendViewEvent(E_ENDVIEWUPDATE);
}

void View::Render()
{
    SendViewEvent(E_BEGINVIEWRENDER);
    if (hasScenePasses_ && ((octree_ == nullptr) || (camera_ == nullptr)))
    {
        SendViewEvent(E_ENDVIEWRENDER);
        return;
    }

    UpdateGeometries();

    // Allocate screen buffers as necessary
    AllocateScreenBuffers();
    SendViewEvent(E_VIEWBUFFERSREADY);

    // Forget parameter sources from the previous view
    graphics_->ClearParameterSources();

    if (renderer_->GetDynamicInstancing() && graphics_->GetInstancingSupport())
        PrepareInstancingBuffer();

    // It is possible, though not recommended, that the same camera is used for multiple main views. Set automatic aspect ratio
    // to ensure correct projection will be used
    if ((camera_ != nullptr) && camera_->GetAutoAspectRatio())
        camera_->SetAspectRatioInternal((float)(viewSize_.x_) / (float)(viewSize_.y_));

    // Bind the face selection and indirection cube maps for point light shadows
    if (renderer_->GetDrawShadows())
    {
        graphics_->SetTexture(TU_FACESELECT, renderer_->GetFaceSelectCubeMap());
        graphics_->SetTexture(TU_INDIRECTION, renderer_->GetIndirectionCubeMap());
    }

    if (renderTarget_ != nullptr)
    {
        // On OpenGL, flip the projection if rendering to a texture so that the texture can be addressed in the same way
        // as a render texture produced on Direct3D9
        if (camera_ != nullptr)
            camera_->SetFlipVertical(true);
    }

    // Render
    ExecuteRenderPathCommands();

    // Reset state after commands
    graphics_->SetFillMode(FILL_SOLID);
    graphics_->SetLineAntiAlias(false);
    graphics_->SetClipPlane(false);
    graphics_->SetColorWrite(true);
    graphics_->SetDepthBias(0.0f, 0.0f);
    graphics_->SetScissorTest(false);
    graphics_->SetStencilTest(false);

    // Draw the associated debug geometry now if enabled
    if (drawDebug_ && (octree_ != nullptr) && (camera_ != nullptr))
    {
        DebugRenderer* debug = octree_->GetComponent<DebugRenderer>();
        if ((debug != nullptr) && debug->IsEnabledEffective() && debug->HasContent())
        {
            // If used resolve from backbuffer, blit first to the backbuffer to ensure correct depth buffer on OpenGL
            // Otherwise use the last rendertarget and blit after debug geometry
            if (usedResolve_ && currentRenderTarget_ != renderTarget_)
            {
                BlitFramebuffer(currentRenderTarget_->GetParentTexture(), renderTarget_, false);
                currentRenderTarget_ = renderTarget_;
            }

            graphics_->SetRenderTarget(0, currentRenderTarget_);
            for (unsigned i = 1; i < MAX_RENDERTARGETS; ++i)
                graphics_->SetRenderTarget(i, (RenderSurface*)nullptr);
            graphics_->SetDepthStencil(GetDepthStencil(currentRenderTarget_));
            IntVector2 rtSizeNow = graphics_->GetRenderTargetDimensions();
            IntRect viewport = (currentRenderTarget_ == renderTarget_) ? viewRect_ : IntRect(0, 0, rtSizeNow.x_,
                                                                                             rtSizeNow.y_);
            graphics_->SetViewport(viewport);

            debug->SetView(camera_);
            debug->Render();
        }
    }

    if (camera_ != nullptr)
        camera_->SetFlipVertical(false);

    // Run framebuffer blitting if necessary. If scene was resolved from backbuffer, do not touch depth
    // (backbuffer should contain proper depth already)
    if (currentRenderTarget_ != renderTarget_)
        BlitFramebuffer(currentRenderTarget_->GetParentTexture(), renderTarget_, !usedResolve_);

    SendViewEvent(E_ENDVIEWRENDER);
}

Graphics* View::GetGraphics() const
{
    return graphics_;
}

Renderer* View::GetRenderer() const
{
    return renderer_;
}

View* View::GetSourceView() const
{
    return sourceView_;
}

void View::SetGlobalShaderParameters()
{
    graphics_->SetShaderParameter(VSP_DELTATIME, frame_.timeStep_);
    graphics_->SetShaderParameter(PSP_DELTATIME, frame_.timeStep_);

    if (scene_ != nullptr)
    {
        float elapsedTime = scene_->GetElapsedTime();
        graphics_->SetShaderParameter(VSP_ELAPSEDTIME, elapsedTime);
        graphics_->SetShaderParameter(PSP_ELAPSEDTIME, elapsedTime);
    }
    SendViewEvent(E_VIEWGLOBALSHADERPARAMETERS);
}

void View::SetCameraShaderParameters(const Camera &camera)
{
    Matrix3x4 cameraEffectiveTransform = camera.GetEffectiveWorldTransform();

    graphics_->SetShaderParameter(VSP_CAMERAPOS, cameraEffectiveTransform.Translation());
    graphics_->SetShaderParameter(VSP_VIEWINV, cameraEffectiveTransform);
    graphics_->SetShaderParameter(VSP_VIEW, camera.GetView());
    graphics_->SetShaderParameter(PSP_CAMERAPOS, cameraEffectiveTransform.Translation());

    float nearClip = camera.GetNearClip();
    float farClip  = camera.GetFarClip();
    graphics_->SetShaderParameter(VSP_NEARCLIP, nearClip);
    graphics_->SetShaderParameter(VSP_FARCLIP, farClip);
    graphics_->SetShaderParameter(PSP_NEARCLIP, nearClip);
    graphics_->SetShaderParameter(PSP_FARCLIP, farClip);

    Vector4 depthMode = Vector4::ZERO;
    if (camera.IsOrthographic())
    {
        depthMode.x_ = 1.0f;
        depthMode.z_ = 0.5f;
        depthMode.w_ = 0.5f;
    }
    else
        depthMode.w_ = 1.0f / farClip;

    graphics_->SetShaderParameter(VSP_DEPTHMODE, depthMode);

    Vector4 depthReconstruct(farClip / (farClip - nearClip), -nearClip / (farClip - nearClip),
                             camera.IsOrthographic() ? 1.0f : 0.0f, camera.IsOrthographic() ? 0.0f : 1.0f);
    graphics_->SetShaderParameter(PSP_DEPTHRECONSTRUCT, depthReconstruct);

    Vector3 nearVector, farVector;
    camera.GetFrustumSize(nearVector, farVector);
    graphics_->SetShaderParameter(VSP_FRUSTUMSIZE, farVector);

    Matrix4 projection = camera.GetGPUProjection();
    // Add constant depth bias manually to the projection matrix due to glPolygonOffset() inconsistency
    float constantBias = 2.0f * graphics_->GetDepthConstantBias();
    projection.m22_ += projection.m32_ * constantBias;
    projection.m23_ += projection.m33_ * constantBias;

    graphics_->SetShaderParameter(VSP_VIEWPROJ, projection * camera.GetView());
}

void View::SetGBufferShaderParameters(const IntVector2& texSize, const IntRect& viewRect)
{
    float texWidth = (float)texSize.x_;
    float texHeight = (float)texSize.y_;
    float widthRange = 0.5f * viewRect.Width() / texWidth;
    float heightRange = 0.5f * viewRect.Height() / texHeight;

    Vector4 bufferUVOffset(((float)viewRect.left_) / texWidth + widthRange,
                           1.0f - (((float)viewRect.top_) / texHeight + heightRange), widthRange, heightRange);
    graphics_->SetShaderParameter(VSP_GBUFFEROFFSETS, bufferUVOffset);

    float invSizeX = 1.0f / texWidth;
    float invSizeY = 1.0f / texHeight;
    graphics_->SetShaderParameter(PSP_GBUFFERINVSIZE, Vector2(invSizeX, invSizeY));
}

void View::GetDrawables()
{
    if ((octree_ == nullptr) || (cullCamera_ == nullptr))
        return;

    URHO3D_PROFILE(GetDrawables);

    WorkQueue* queue = GetSubsystem<WorkQueue>();
    std::vector<Drawable*>& tempDrawables(tempDrawables_[0]);

    // Get zones and occluders first
    {
        ZoneOccluderOctreeQuery
                query(tempDrawables, cullCamera_->GetFrustum(), DRAWABLE_GEOMETRY | DRAWABLE_ZONE, cullCamera_->GetViewMask());
        octree_->GetDrawables(query);
    }
    highestZonePriority_ = M_MIN_INT;
    int bestPriority = M_MIN_INT;
    Node* cameraNode = cullCamera_->GetNode();
    Vector3 cameraPos = cameraNode->GetWorldPosition();

    for (Drawable* drawable : tempDrawables)
    {
        unsigned char flags = drawable->GetDrawableFlags();

        if ((flags & DRAWABLE_ZONE) != 0u)
        {
            Zone* zone = static_cast<Zone*>(drawable);
            zones_.push_back(zone);
            int priority = zone->GetPriority();
            if (priority > highestZonePriority_)
                highestZonePriority_ = priority;
            if (priority > bestPriority && zone->IsInside(cameraPos))
            {
                cameraZone_ = zone;
                bestPriority = priority;
            }
        }
        else
            occluders_.push_back(drawable);
    }

    // Determine the zone at far clip distance. If not found, or camera zone has override mode, use camera zone
    cameraZoneOverride_ = cameraZone_->GetOverride();
    if (!cameraZoneOverride_)
    {
        Vector3 farClipPos = cameraPos + cameraNode->GetWorldDirection() * Vector3(0.0f, 0.0f, cullCamera_->GetFarClip());
        bestPriority = M_MIN_INT;

        for (Zone* elem : zones_)
        {
            int priority = elem->GetPriority();
            if (priority > bestPriority && elem->IsInside(farClipPos))
            {
                farClipZone_ = elem;
                bestPriority = priority;
            }
        }
    }
    if (farClipZone_ == renderer_->GetDefaultZone())
        farClipZone_ = cameraZone_;

    // If occlusion in use, get & render the occluders
    occlusionBuffer_ = nullptr;
    if (maxOccluderTriangles_ > 0)
    {
        UpdateOccluders(occluders_, cullCamera_);
        if (!occluders_.empty())
        {
            URHO3D_PROFILE(DrawOcclusion);

            occlusionBuffer_ = renderer_->GetOcclusionBuffer(cullCamera_);
            DrawOccluders(occlusionBuffer_, occluders_);
        }
    }
    else
        occluders_.clear();

    // Get lights and geometries. Coarse occlusion for octants is used at this point
    if (occlusionBuffer_ != nullptr)
    {
        OccludedFrustumOctreeQuery query
                (tempDrawables, cullCamera_->GetFrustum(), occlusionBuffer_, DRAWABLE_GEOMETRY | DRAWABLE_LIGHT, cullCamera_->GetViewMask());
        octree_->GetDrawables(query);
    }
    else
    {
        FrustumOctreeQuery query(tempDrawables, cullCamera_->GetFrustum(), DRAWABLE_GEOMETRY | DRAWABLE_LIGHT, cullCamera_->GetViewMask());
        octree_->GetDrawables(query);
    }

    // Check drawable occlusion, find zones for moved drawables and collect geometries & lights in worker threads
    {
        for (PerThreadSceneResult& result : sceneResults_)
        {

            result.geometries_.clear();
            result.lights_.clear();
            result.minZ_ = M_INFINITY;
            result.maxZ_ = 0.0f;
        }

        if(!tempDrawables.empty())
        {
            int numWorkItems = queue->GetNumThreads() + 1; // Worker threads + main thread
            int drawablesPerItem = tempDrawables.size() / numWorkItems;

            Drawable ** start_ptr = tempDrawables.data();
            Drawable ** fin_ptr = start_ptr + tempDrawables.size();
            // Create a work item for each thread
            for (int i = 0; i < numWorkItems; ++i)
            {
                SharedPtr<WorkItem> item = queue->GetFreeItem();
                item->priority_ = M_MAX_UNSIGNED;
                item->workFunction_ = CheckVisibilityWork;
                item->aux_ = this;

                Drawable ** end_ptr = fin_ptr;
                if (i < numWorkItems - 1 && end_ptr - start_ptr > drawablesPerItem)
                    end_ptr = start_ptr + drawablesPerItem;

                item->start_ = start_ptr;
                item->end_ = end_ptr;
                queue->AddWorkItem(item);

                start_ptr = end_ptr;
            }

            queue->Complete(M_MAX_UNSIGNED);
        }
    }

    // Combine lights, geometries & scene Z range from the threads
    geometries_.clear();
    lights_.clear();
    minZ_ = M_INFINITY;
    maxZ_ = 0.0f;

    if (sceneResults_.size() > 1)
    {
        for (PerThreadSceneResult& result : sceneResults_)
        {
            geometries_.insert(geometries_.end(),result.geometries_.begin(),result.geometries_.end());
            lights_.insert(lights_.end(),result.lights_.begin(),result.lights_.end());
            minZ_ = std::min(minZ_, result.minZ_);
            maxZ_ = std::max(maxZ_, result.maxZ_);
        }
    }
    else
    {
        // If just 1 thread, copy the results directly
        PerThreadSceneResult& result = sceneResults_[0];
        minZ_ = result.minZ_;
        maxZ_ = result.maxZ_;
        geometries_.swap(result.geometries_);
        lights_.swap(result.lights_);
    }

    if (minZ_ == M_INFINITY)
        minZ_ = 0.0f;

    // Sort the lights to brightest/closest first, and per-vertex lights first so that per-vertex base pass can be evaluated first
    for (Light* light : lights_)
    {
        light->SetIntensitySortValue(cullCamera_->GetDistance(light->GetNode()->GetWorldPosition()));
        light->SetLightQueue(nullptr);
    }

    std::sort(lights_.begin(), lights_.end(), CompareLights);
}

void View::GetBatches()
{
    if ((octree_ == nullptr) || (cullCamera_ == nullptr))
        return;

    nonThreadedGeometries_.clear();
    threadedGeometries_.clear();
    // retrieve default technique.
    const std::vector<TechniqueEntry>& techniques(renderer_->GetDefaultMaterial()->GetTechniques());
    Technique *default_tech = techniques.empty() ? (Technique *)nullptr : techniques.back().technique_;

    ProcessLights();
    GetLightBatches(default_tech);
    GetBaseBatches(default_tech);
}

void View::ProcessLights()
{
    // Process lit geometries and shadow casters for each light
    URHO3D_PROFILE(ProcessLights);

    WorkQueue* queue = GetSubsystem<WorkQueue>();
    lightQueryResults_.resize(lights_.size());

    for (unsigned i = 0; i < lightQueryResults_.size(); ++i)
    {
        SharedPtr<WorkItem> item = queue->GetFreeItem();
        item->priority_ = M_MAX_UNSIGNED;
        item->workFunction_ = ProcessLightWork;
        item->aux_ = this;

        LightQueryResult& query = lightQueryResults_[i];
        query.light_ = lights_[i];

        item->start_ = &query;
        queue->AddWorkItem(item);
    }

    // Ensure all lights have been processed before proceeding
    queue->Complete(M_MAX_UNSIGNED);
}

void View::GetLightBatches(Technique *default_tech)
{
    BatchQueue * alphaQueue = (alphaPassQueueIdx_ == -1) ? nullptr : &batchQueueStorage_[alphaPassQueueIdx_];
    // Build light queues and lit batches
    {
        URHO3D_PROFILE(GetLightBatches);

        // Preallocate light queues: per-pixel lights which have lit geometries
        unsigned numLightQueues = 0;
        unsigned usedLightQueues = 0;
        for (LightQueryResult & i : lightQueryResults_)
        {
            if (!i.light_->GetPerVertex() && !i.litGeometries_.empty())
                ++numLightQueues;
        }

        lightQueues_.resize(numLightQueues);
        maxLightsDrawables_.clear();
        unsigned maxSortedInstances = renderer_->GetMaxSortedInstances();

        for (LightQueryResult & query : lightQueryResults_)
        {

            // If light has no affected geometries, no need to process further
            if (query.litGeometries_.empty())
                continue;

            Light* light = query.light_;

            if (light->GetPerVertex())
            {
                // Per-vertex light
                // Add the vertex light to lit drawables. It will be processed later during base pass batch generation
                for (Drawable* drawable : query.litGeometries_)
                {
                    drawable->AddVertexLight(light);
                }
                continue; // go to next light;
            }
            // Per-pixel light

            unsigned shadowSplits = query.numSplits_;

            // Initialize light queue and store it to the light so that it can be found later
            LightBatchQueue& lightQueue(lightQueues_[usedLightQueues++]);
            light->SetLightQueue(&lightQueue);
            lightQueue.light_ = light;
            lightQueue.negative_ = light->IsNegative();
            lightQueue.shadowMap_ = nullptr;
            lightQueue.litBaseBatches_.Clear(maxSortedInstances);
            lightQueue.litBatches_.Clear(maxSortedInstances);
            lightQueue.volumeBatches_.clear();

            // Allocate shadow map now
            if (shadowSplits > 0)
            {
                lightQueue.shadowMap_ = renderer_->GetShadowMap(light, cullCamera_, viewSize_.x_, viewSize_.y_);
                // If did not manage to get a shadow map, convert the light to unshadowed
                if (lightQueue.shadowMap_ == nullptr)
                    shadowSplits = 0;
            }

            // Setup shadow batch queues
            lightQueue.shadowSplits_.resize(shadowSplits);
            for (unsigned j = 0; j < shadowSplits; ++j)
            {
                ShadowBatchQueue& shadowQueue = lightQueue.shadowSplits_[j];
                LightQueryShadowEntry &entry(query.shadowEntries_[j]);
                Camera* shadowCamera = entry.shadowCameras_;
                shadowQueue.shadowCamera_ = shadowCamera;
                shadowQueue.nearSplit_ = entry.shadowNearSplits_;
                shadowQueue.farSplit_ = entry.shadowFarSplits_;
                shadowQueue.shadowBatches_.Clear(maxSortedInstances);

                // Setup the shadow split viewport and finalize shadow camera parameters
                shadowQueue.shadowViewport_ = GetShadowMapViewport(light, j, lightQueue.shadowMap_);
                FinalizeShadowCamera(shadowCamera, light, shadowQueue.shadowViewport_, entry.shadowCasterBox_);

                // Loop through shadow casters
                std::vector<Drawable*>::const_iterator k = query.shadowCasters_.begin() + entry.shadowCasterBegin_;
                std::vector<Drawable*>::const_iterator fin = query.shadowCasters_.begin() + entry.shadowCasterEnd_;

                for (; k != fin; ++k)
                {
                    Drawable* drawable = *k;
                    // If drawable is not in actual view frustum, mark it in view here and check its geometry update type
                    if (!drawable->IsInView(frame_, true))
                    {
                        drawable->MarkInView(frame_.frameNumber_);
                        UpdateGeometryType type = drawable->GetUpdateGeometryType();
                        if (type == UPDATE_MAIN_THREAD)
                            nonThreadedGeometries_.push_back(drawable);
                        else if (type == UPDATE_WORKER_THREAD)
                            threadedGeometries_.push_back(drawable);
                    }

                    Zone* zone = GetZone(drawable);

                    for (const SourceBatch& srcBatch : drawable->GetBatches())
                    {
                        if ((srcBatch.geometry_ == nullptr) || (srcBatch.numWorldTransforms_ == 0u))
                            continue;

                        Technique* tech = srcBatch.material_ != nullptr ? GetTechnique(drawable, srcBatch.material_) : default_tech;
                        if (tech == nullptr)
                            continue;

                        Pass* pass = tech->GetSupportedPass(Technique::shadowPassIndex);
                        // Skip if material has no shadow pass
                        if (pass == nullptr)
                            continue;


                        AddBatchToQueue(shadowQueue.shadowBatches_,
                                        Batch(srcBatch,zone,&lightQueue,pass), tech);
                    }
                }
            }

            BatchQueue *availableQueues[] = { &lightQueue.litBaseBatches_,&lightQueue.litBatches_,alphaQueue };

            // Process lit geometries
            for (Drawable* drawable : query.litGeometries_) {
                drawable->AddLight(light);

                // If drawable limits maximum lights, only record the light, and check maximum count / build batches later
                if (drawable->GetMaxLights() == 0u)
                    GetLitBatches(drawable, GetZone(drawable),lightQueue, availableQueues,default_tech);
                else
                    maxLightsDrawables_.insert(drawable);
            }

            // In deferred modes, store the light volume batch now
            if (deferred_)
            {
                Batch volumeBatch;
                volumeBatch.geometry_ = renderer_->GetLightGeometry(light);
                volumeBatch.geometryType_ = GEOM_STATIC;
                volumeBatch.worldTransform_ = &light->GetVolumeTransform(cullCamera_);
                volumeBatch.numWorldTransforms_ = 1;
                volumeBatch.lightQueue_ = &lightQueue;
                volumeBatch.distance_ = light->GetDistance();
                volumeBatch.material_           = nullptr;
                volumeBatch.pass_               = nullptr;
                volumeBatch.zone_               = nullptr;
                renderer_->SetLightVolumeBatchShaders(volumeBatch, cullCamera_, lightVolumeCommand_->vertexShaderName_,
                                                      lightVolumeCommand_->pixelShaderName_, lightVolumeCommand_->vertexShaderDefines_,
                                                      lightVolumeCommand_->pixelShaderDefines_);
                lightQueue.volumeBatches_.push_back(volumeBatch);
            }
        }
    }

    // Process drawables with limited per-pixel light count
    if (!maxLightsDrawables_.empty())
    {
        URHO3D_PROFILE(GetMaxLightsBatches);

        for (Drawable* drawable : maxLightsDrawables_)
        {
            Zone *zone=GetZone(drawable);
            drawable->LimitLights();
            const auto& lights(drawable->GetLights());

            for (Light* light : lights)
            {
                // Find the correct light queue again
                LightBatchQueue* queue = light->GetLightQueue();
                if (queue != nullptr) {
                    BatchQueue *availableQueues[3] = { &queue->litBaseBatches_,&queue->litBatches_,alphaQueue };
                    GetLitBatches(drawable,zone, *queue, availableQueues,default_tech);
                }
            }
        }
    }
}
void View::GetBaseBatches(Technique *default_tech)
{
    URHO3D_PROFILE(GetBaseBatches);

    unsigned frameNo = frame_.frameNumber_;
    for (Drawable* drawable : geometries_)
    {
        const std::vector<SourceBatch>& batches(drawable->GetBatches());
        bool vertexLightsProcessed = false;

        UpdateGeometryType type = drawable->GetUpdateGeometryType();
        if (type == UPDATE_MAIN_THREAD)
            nonThreadedGeometries_.push_back(drawable);
        else if (type == UPDATE_WORKER_THREAD)
            threadedGeometries_.push_back(drawable);

        Zone* zone = GetZone(drawable);

        unsigned drawableLightMask = GetLightMask(drawable);

        for (unsigned j = 0,fin=batches.size(); j < fin; ++j)
        {
            const SourceBatch& srcBatch(batches[j]);
            if ((srcBatch.geometry_ == nullptr) || (srcBatch.numWorldTransforms_ == 0u))
                continue;
            Material *srcMaterial = srcBatch.material_.Get();
            // Check here if the material refers to a rendertarget texture with camera(s) attached
            // Only check this for backbuffer views (null rendertarget)
            if ((srcMaterial != nullptr) && srcMaterial->GetAuxViewFrameNumber() != frameNo && (renderTarget_ == nullptr))
                CheckMaterialForAuxView(srcMaterial);

            Technique* tech = srcMaterial != nullptr ? GetTechnique(drawable, srcMaterial) : default_tech;
            if (tech == nullptr)
                continue;

            bool drawableHasBasePass = j < 32 && drawable->HasBasePass(j);
            // Check each of the scene passes
            for (ScenePassInfo& info : scenePasses_)
            {
                LightBatchQueue* lq = nullptr;
                // Skip forward base pass if the corresponding litbase pass already exists
                if ((info.passIndex_ == basePassIndex_) && drawableHasBasePass)
                    continue;

                Pass* pass = tech->GetSupportedPass(info.passIndex_);
                if (pass == nullptr)
                    continue;

                if (info.vertexLights_)
                {
                    const std::vector<Light*>& drawableVertexLights(drawable->GetVertexLights());
                    if (!drawableVertexLights.empty() && !vertexLightsProcessed)
                    {
                        // Limit vertex lights. If this is a deferred opaque batch, remove converted per-pixel lights,
                        // as they will be rendered as light volumes in any case, and drawing them also as vertex lights
                        // would result in double lighting
                        drawable->LimitVertexLights(deferred_ && pass->GetBlendMode() == BLEND_REPLACE);
                        vertexLightsProcessed = true;
                    }
                    if (!drawableVertexLights.empty() ) {
                        uint64_t vertex_lights_hash = GetVertexLightQueueHash(drawableVertexLights);
                        // Find a vertex light queue. If not found, create new
                        HashMap<uint64_t, LightBatchQueue>::iterator i = vertexLightQueues_.find(vertex_lights_hash);
                        if (i == vertexLightQueues_.end())
                        {
                            i = vertexLightQueues_.emplace(vertex_lights_hash, LightBatchQueue()).first;
                            MAP_VALUE(i).light_ = nullptr;
                            MAP_VALUE(i).shadowMap_ = nullptr;
                            MAP_VALUE(i).vertexLights_ = drawableVertexLights;
                        }
                        lq = &MAP_VALUE(i);
                    }
                }
                BatchQueue & que(batchQueueStorage_[info.batchQueueIdx_]);

                bool allowInstancing = info.allowInstancing_;
                if (allowInstancing && info.markToStencil_ && drawableLightMask != (zone->GetLightMask() & 0xff))
                    allowInstancing = false;

                AddBatchToQueue(que, Batch(srcBatch,zone,lq,pass,drawableLightMask,true),
                                tech, allowInstancing);
            }
        }
    }
}

void View::UpdateGeometries()
{
    // Update geometries in the source view if necessary (prepare order may differ from render order)
    if ((sourceView_ != nullptr) && !sourceView_->geometriesUpdated_)
    {
        sourceView_->UpdateGeometries();
        return;
    }
    URHO3D_PROFILE(SortAndUpdateGeometry);

    WorkQueue* queue = GetSubsystem<WorkQueue>();

    // Sort batches
    {
        for (const RenderPathCommand& command : renderPath_->commands_)
        {
            if (!IsNecessary(command))
                continue;

            if (command.type_ == CMD_SCENEPASS)
            {
                SharedPtr<WorkItem> item = queue->GetFreeItem();
                item->priority_ = M_MAX_UNSIGNED;
                item->workFunction_ = command.sortMode_ == SORT_FRONTTOBACK ? SortBatchQueueFrontToBackWork : SortBatchQueueBackToFrontWork;
                item->start_ = &batchQueueStorage_[batchQueues_[command.passIndex_]];
                queue->AddWorkItem(item);
            }
        }

        for (LightBatchQueue & elem : lightQueues_)
        {
            SharedPtr<WorkItem> lightItem = queue->GetFreeItem();
            lightItem->priority_ = M_MAX_UNSIGNED;
            lightItem->workFunction_ = SortLightQueueWork;
            lightItem->start_ = &(elem);
            queue->AddWorkItem(lightItem);

            if (!elem.shadowSplits_.empty())
            {
                SharedPtr<WorkItem> shadowItem = queue->GetFreeItem();
                shadowItem->priority_ = M_MAX_UNSIGNED;
                shadowItem->workFunction_ = SortShadowQueueWork;
                shadowItem->start_ = &elem;
                queue->AddWorkItem(shadowItem);
            }
        }
    }

    // Update geometries. Split into threaded and non-threaded updates.
    {
        if (!threadedGeometries_.empty())
        {
            // In special cases (context loss, multi-view) a drawable may theoretically first have reported a threaded update, but will actually
            // require a main thread update. Check these cases first and move as applicable. The threaded work routine will tolerate the null
            // pointer holes that we leave to the threaded update queue.
            for (Drawable *& drwbl : threadedGeometries_)
            {
                if (drwbl->GetUpdateGeometryType() == UPDATE_MAIN_THREAD)
                {
                    nonThreadedGeometries_.push_back(drwbl);
                    drwbl = nullptr;
                }
            }

            int numWorkItems = queue->GetNumThreads() + 1; // Worker threads + main thread
            int drawablesPerItem = threadedGeometries_.size() / numWorkItems;

            Drawable ** start_ptr = &threadedGeometries_.front();
            Drawable ** fin_ptr = start_ptr + threadedGeometries_.size();
            for (int i = 0; i < numWorkItems; ++i)
            {
                Drawable ** end_ptr = fin_ptr;
                if (i < numWorkItems - 1 && end_ptr - start_ptr > drawablesPerItem)
                    end_ptr = start_ptr + drawablesPerItem;

                SharedPtr<WorkItem> item = queue->GetFreeItem();
                item->priority_ = M_MAX_UNSIGNED;
                item->workFunction_ = UpdateDrawableGeometriesWork;
                item->aux_ = const_cast<FrameInfo*>(&frame_);
                item->start_ = start_ptr;
                item->end_ = end_ptr;
                queue->AddWorkItem(item);

                start_ptr = end_ptr;
            }
        }

        // While the work queue is processed, update non-threaded geometries
        for (Drawable* drwbl : nonThreadedGeometries_)
            drwbl->UpdateGeometry(frame_);
    }

    // Finally ensure all threaded work has completed
    queue->Complete(M_MAX_UNSIGNED);
    geometriesUpdated_ = true;
}

void View::GetLitBatches(Drawable* drawable, Zone *zone,LightBatchQueue& lightQueue,
                         BatchQueue* availableQueues[3], Technique *default_tech)
{
    const Light* light = lightQueue.light_;
    const std::vector<SourceBatch>& batches(drawable->GetBatches());

    // Shadows on transparencies can only be rendered if shadow maps are not reused
    const bool allowTransparentShadows = !renderer_->GetReuseShadowMaps();
    const bool allowLitBase = useLitBase_ && !lightQueue.negative_ && light == drawable->GetFirstLight() &&
            drawable->GetVertexLights().empty() && !zone->GetAmbientGradient();
    const bool hasAlphaQueue = availableQueues[2]!=nullptr;
    int i=-1;
    int queueIndex;
    const bool hasG_BUFFER_PASS = gBufferPassIndex_ != M_MAX_UNSIGNED;
    if(allowLitBase) {
        for (const SourceBatch& srcBatch : batches)
        {
            ++i;
            Technique* tech = srcBatch.material_ != nullptr ? GetTechnique(drawable, srcBatch.material_) : default_tech;
            if ((srcBatch.geometry_ == nullptr) || (srcBatch.numWorldTransforms_ == 0u) || (tech == nullptr))
                continue;

            // Do not create pixel lit forward passes for materials that render into the G-buffer
            if (hasG_BUFFER_PASS && tech->HasPass(gBufferPassIndex_))
                continue;

            bool useInstancing = true;

            // Check for lit base pass. Because it uses the replace blend mode, it must be ensured to be the first light
            // Also vertex lighting or ambient gradient require the non-lit base pass, so skip in those cases
            Pass * dest_pass = nullptr;
            queueIndex = 1;
            if (i < 32)
            {
                dest_pass = tech->GetSupportedPass(litBasePassIndex_);
                if (dest_pass != nullptr)
                {
                    queueIndex = 0;
                    drawable->SetBasePass(i);
                }
            }
            if(queueIndex==1)
                dest_pass = tech->GetSupportedPass(lightPassIndex_);
            bool isBase = (queueIndex==0);

            // If no lit pass, check for lit alpha
            if (dest_pass == nullptr)
            {
                if(!hasAlphaQueue)
                    continue; // no alpha queue, skip it then.
                dest_pass = tech->GetSupportedPass(litAlphaPassIndex_);
                // Skip if material does not receive light at all
                if (dest_pass == nullptr)
                    continue;
                useInstancing = false; // Transparent batches can not be instanced
                queueIndex = 2;
            }

            AddBatchToQueue(*availableQueues[queueIndex],
                            Batch(srcBatch,zone,&lightQueue,dest_pass,static_cast<uint8_t>(isBase)), tech,
                            useInstancing, allowTransparentShadows);
        }

    }
    else {
        for (const SourceBatch& srcBatch : batches)
        {
            Technique* tech = srcBatch.material_ != nullptr ? GetTechnique(drawable, srcBatch.material_) : default_tech;
            if ((srcBatch.geometry_ == nullptr) || (srcBatch.numWorldTransforms_ == 0u) || (tech == nullptr))
                continue;

            // Do not create pixel lit forward passes for materials that render into the G-buffer
            if (gBufferPassIndex_ != M_MAX_UNSIGNED && tech->HasPass(gBufferPassIndex_))
                continue;

            bool useInstancing = true;
            Pass * dest_pass = tech->GetSupportedPass(lightPassIndex_);

            queueIndex = 1;
            // If no lit pass, check for lit alpha
            if (dest_pass == nullptr)
            {
                if(!hasAlphaQueue)
                    continue; // no alpha queue, skip it then.
                dest_pass = tech->GetSupportedPass(litAlphaPassIndex_);
                // Skip if material does not receive light at all
                if (dest_pass == nullptr)
                    continue;
                useInstancing = false; // Transparent batches can not be instanced
                queueIndex = 2;
            }
            AddBatchToQueue(*availableQueues[queueIndex],
                            Batch(srcBatch,zone,&lightQueue,dest_pass), tech,
                            useInstancing, allowTransparentShadows);
        }
    }
}

void View::ExecuteRenderPathCommands()
{
    View* actualView = sourceView_ != nullptr ? sourceView_ : this;
    // If not reusing shadowmaps, render all of them first
    if (!renderer_->GetReuseShadowMaps() && renderer_->GetDrawShadows() && !actualView->lightQueues_.empty())
    {
        URHO3D_PROFILE(RenderShadowMaps);

        for (LightBatchQueue & elem : actualView->lightQueues_)
        {
            if (NeedRenderShadowMap(elem))
                RenderShadowMap(elem);
        }
    }

    {
        URHO3D_PROFILE(ExecuteRenderPath);

        // Set for safety in case of empty renderpath
        currentRenderTarget_ = substituteRenderTarget_ != nullptr ? substituteRenderTarget_ : renderTarget_;
        currentViewportTexture_ = nullptr;

        bool viewportModified = false;
        bool isPingponging = false;
        usedResolve_ = false;

        unsigned lastCommandIndex = 0;
        for (unsigned i = 0; i < renderPath_->commands_.size(); ++i)
        {
            RenderPathCommand& command = renderPath_->commands_[i];
            if (actualView->IsNecessary(command))
                lastCommandIndex = i;
        }

        for (unsigned i = 0; i < renderPath_->commands_.size(); ++i)
        {
            RenderPathCommand& command = renderPath_->commands_[i];
            if (!actualView->IsNecessary(command))
                continue;

            bool viewportRead = actualView->CheckViewportRead(command);
            bool viewportWrite = actualView->CheckViewportWrite(command);
            bool beginPingpong = actualView->CheckPingpong(i);

            // Has the viewport been modified and will be read as a texture by the current command?
            if (viewportRead && viewportModified)
            {
                // Start pingponging without a blit if already rendering to the substitute render target
                if ((currentRenderTarget_ != nullptr) && currentRenderTarget_ == substituteRenderTarget_ && beginPingpong)
                    isPingponging = true;

                // If not using pingponging, simply resolve/copy to the first viewport texture
                if (!isPingponging)
                {
                    if (currentRenderTarget_ == nullptr)
                    {
                        graphics_->ResolveToTexture(dynamic_cast<Texture2D*>(viewportTextures_[0]), viewRect_);
                        currentViewportTexture_ = viewportTextures_[0];
                        viewportModified = false;
                        usedResolve_ = true;
                    }
                    else
                    {
                        if (viewportWrite)
                        {
                            BlitFramebuffer(currentRenderTarget_->GetParentTexture(),
                                            GetRenderSurfaceFromTexture(viewportTextures_[0]), false);
                            currentViewportTexture_ = viewportTextures_[0];
                            viewportModified = false;
                        }
                        else
                        {
                            // If the current render target is already a texture, and we are not writing to it, can read that
                            // texture directly instead of blitting. However keep the viewport dirty flag in case a later command
                            // will do both read and write, and then we need to blit / resolve
                            currentViewportTexture_ = currentRenderTarget_->GetParentTexture();
                        }
                    }
                }
                else
                {
                    // Swap the pingpong double buffer sides. Texture 0 will be read next
                    viewportTextures_[1] = viewportTextures_[0];
                    viewportTextures_[0] = currentRenderTarget_->GetParentTexture();
                    currentViewportTexture_ = viewportTextures_[0];
                    viewportModified = false;
                }
            }

            if (beginPingpong)
                isPingponging = true;

            // Determine viewport write target
            if (viewportWrite)
            {
                if (isPingponging)
                {
                    currentRenderTarget_ = GetRenderSurfaceFromTexture(viewportTextures_[1]);
                    // If the render path ends into a quad, it can be redirected to the final render target
                    // However, on OpenGL we can not reliably do this in case the final target is the backbuffer, and we want to
                    // render depth buffer sensitive debug geometry afterward (backbuffer and textures can not share depth)
                    if (i == lastCommandIndex && command.type_ == CMD_QUAD && (renderTarget_ != nullptr))
                        currentRenderTarget_ = renderTarget_;
                }
                else
                    currentRenderTarget_ = substituteRenderTarget_ != nullptr ? substituteRenderTarget_ : renderTarget_;
            }

            switch (command.type_)
            {
            case CMD_CLEAR:
            {
                URHO3D_PROFILE(ClearRenderTarget);

                Color clearColor = command.clearColor_;
                if (command.useFogColor_)
                    clearColor = actualView->farClipZone_->GetFogColor();

                SetRenderTargets(command);
                graphics_->Clear(command.clearFlags_, clearColor, command.clearDepth_, command.clearStencil_);
            }
                break;

            case CMD_SCENEPASS:
            {
                BatchQueue& queue = actualView->batchQueueStorage_[actualView->batchQueues_[command.passIndex_]];
                if (!queue.IsEmpty())
                {
                    URHO3D_PROFILE(RenderScenePass);

                    SetRenderTargets(command);
                    bool allowDepthWrite = SetTextures(command);
                    graphics_->SetClipPlane(camera_->GetUseClipping(), camera_->GetClipPlane(), camera_->GetView(), camera_->GetProjection());
                    queue.Draw(this, camera_, command.markToStencil_, false, allowDepthWrite);
                }
                break;
            }

            case CMD_QUAD:
            {
                URHO3D_PROFILE(RenderQuad);

                SetRenderTargets(command);
                SetTextures(command);
                RenderQuad(command);
            }
                break;

            case CMD_FORWARDLIGHTS:
                // Render shadow maps + opaque objects' additive lighting
                if (!actualView->lightQueues_.empty())
                {
                    URHO3D_PROFILE(RenderLights);

                    SetRenderTargets(command);

                    for (LightBatchQueue & elem : actualView->lightQueues_)
                    {
                        // If reusing shadowmaps, render each of them before the lit batches
                        if (renderer_->GetReuseShadowMaps() && (elem.shadowMap_ != nullptr))
                        {
                            RenderShadowMap(elem);
                            SetRenderTargets(command);
                        }

                        bool allowDepthWrite = SetTextures(command);
                        graphics_->SetClipPlane(camera_->GetUseClipping(), camera_->GetClipPlane(), camera_->GetView(), camera_->GetProjection());

                        // Draw base (replace blend) batches first
                        elem.litBaseBatches_.Draw(this, camera_, false, false, allowDepthWrite);

                        // Then, if there are additive passes, optimize the light and draw them
                        if (!elem.litBatches_.IsEmpty())
                        {
                            renderer_->OptimizeLightByScissor(elem.light_, camera_);
                            if (!noStencil_)
                                renderer_->OptimizeLightByStencil(elem.light_, camera_);
                            elem.litBatches_.Draw(this, camera_, false, true, allowDepthWrite);
                        }
                    }

                    graphics_->SetScissorTest(false);
                    graphics_->SetStencilTest(false);
                }
                break;

            case CMD_LIGHTVOLUMES:
                // Render shadow maps + light volumes
                if (!actualView->lightQueues_.empty())
                {
                    URHO3D_PROFILE(RenderLightVolumes);

                    SetRenderTargets(command);
                    for (LightBatchQueue & elem : actualView->lightQueues_)
                    {
                        // If reusing shadowmaps, render each of them before the lit batches
                        if (renderer_->GetReuseShadowMaps() && (elem.shadowMap_ != nullptr))
                        {
                            RenderShadowMap(elem);
                            SetRenderTargets(command);
                        }

                        SetTextures(command);

                        for (Batch & btch : elem.volumeBatches_)
                        {
                            SetupLightVolumeBatch(btch);
                            btch.Draw(this, camera_, false);
                        }
                    }

                    graphics_->SetScissorTest(false);
                    graphics_->SetStencilTest(false);
                }
                break;

            case CMD_RENDERUI:
            {
                assert(false);
#ifdef LUTEFISK3D_UI
                SetRenderTargets(command);
                GetSubsystem<UI>()->Render(false);
#endif
            }
                break;
            case CMD_SENDEVENT:
                {
                    using namespace RenderPathEvent;

                    VariantMap& eventData = GetEventDataMap();
                    eventData[P_NAME] = command.eventName_;
                    renderer_->SendEvent(E_RENDERPATHEVENT, eventData);
                }
                break;
            default:
                break;
            }

            // If current command output to the viewport, mark it modified
            if (viewportWrite)
                viewportModified = true;
        }
    }
}

void View::SetRenderTargets(RenderPathCommand& command)
{
    unsigned index = 0;
    bool useColorWrite = true;
    bool useCustomDepth = false;
    bool useViewportOutput = false;

    while (index < command.outputs_.size())
    {
        if (command.outputs_[index].first.compare("viewport", Qt::CaseInsensitive) == 0)
        {
            graphics_->SetRenderTarget(index, currentRenderTarget_);
            useViewportOutput = true;
        }
        else
        {
            Texture* texture = FindNamedTexture(command.outputs_[index].first, true, false);

            // Check for depth only rendering (by specifying a depth texture as the sole output)
            if ((index == 0u) && command.outputs_.size() == 1 && (texture != nullptr) && (texture->GetFormat() == Graphics::GetReadableDepthFormat() ||
                                                                      texture->GetFormat() == Graphics::GetDepthStencilFormat()))
            {
                useColorWrite = false;
                useCustomDepth = true;
                graphics_->SetRenderTarget(0, GetRenderSurfaceFromTexture(depthOnlyDummyTexture_));
                graphics_->SetDepthStencil(GetRenderSurfaceFromTexture(texture));
            }
            else
                graphics_->SetRenderTarget(index, GetRenderSurfaceFromTexture(texture, command.outputs_[index].second));
        }

        ++index;
    }

    while (index < MAX_RENDERTARGETS)
    {
        graphics_->SetRenderTarget(index, (RenderSurface*)nullptr);
        ++index;
    }

    if (command.depthStencilName_.length() != 0)
    {
        Texture* depthTexture = FindNamedTexture(command.depthStencilName_, true, false);
        if (depthTexture != nullptr)
        {
            useCustomDepth = true;
            graphics_->SetDepthStencil(GetRenderSurfaceFromTexture(depthTexture));
        }
    }

    // When rendering to the final destination rendertarget, use the actual viewport. Otherwise texture rendertargets should use
    // their full size as the viewport
    IntVector2 rtSizeNow = graphics_->GetRenderTargetDimensions();
    IntRect viewport = (useViewportOutput && currentRenderTarget_ == renderTarget_) ? viewRect_ : IntRect(0, 0, rtSizeNow.x_,
                                                                                                          rtSizeNow.y_);

    if (!useCustomDepth)
        graphics_->SetDepthStencil(GetDepthStencil(graphics_->GetRenderTarget(0)));
    graphics_->SetViewport(viewport);
    graphics_->SetColorWrite(useColorWrite);
}

bool View::SetTextures(RenderPathCommand& command)
{

    bool allowDepthWrite = true;

    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        if (command.textureNames_[i].isEmpty())
            continue;

        // Bind the rendered output
        if (command.textureNames_[i].compare("viewport", Qt::CaseInsensitive) == 0)
        {
            graphics_->SetTexture(i, currentViewportTexture_);
            continue;
        }

        Texture* texture = FindNamedTexture(command.textureNames_[i], false, i == TU_VOLUMEMAP);

        if (texture != nullptr)
        {
            graphics_->SetTexture(i, texture);
            // Check if the current depth stencil is being sampled
            if ((graphics_->GetDepthStencil() != nullptr) && texture == graphics_->GetDepthStencil()->GetParentTexture())
                allowDepthWrite = false;
        }
        else
        {
            // If requesting a texture fails, clear the texture name to prevent redundant attempts
            command.textureNames_[i] = QString::null;
        }
    }

    return allowDepthWrite;
}

void View::RenderQuad(RenderPathCommand& command)
{
    if (command.vertexShaderName_.isEmpty() || command.pixelShaderName_.isEmpty())
        return;

    // If shader can not be found, clear it from the command to prevent redundant attempts
    ShaderVariation* vs = graphics_->GetShader(VS, command.vertexShaderName_, command.vertexShaderDefines_);
    if (vs == nullptr)
        command.vertexShaderName_ = QString::null;
    ShaderVariation* ps = graphics_->GetShader(PS, command.pixelShaderName_, command.pixelShaderDefines_);
    if (ps == nullptr)
        command.pixelShaderName_ = QString::null;

    // Set shaders & shader parameters and textures
    graphics_->SetShaders(vs, ps);

    SetGlobalShaderParameters();
    if(camera_ != nullptr)
        SetCameraShaderParameters(*camera_);

    // During renderpath commands the G-Buffer or viewport texture is assumed to always be viewport-sized
    IntRect viewport = graphics_->GetViewport();
    IntVector2 viewSize = IntVector2(viewport.Width(), viewport.Height());
    SetGBufferShaderParameters(viewSize, IntRect(0, 0, viewSize.x_, viewSize.y_));

    // Set per-rendertarget inverse size / offset shader parameters as necessary
    for (const RenderTargetInfo& rtInfo : renderPath_->renderTargets_)
    {
        if (!rtInfo.enabled_)
            continue;

        StringHash nameHash(rtInfo.name_);
        if (!renderTargets_.contains(nameHash))
            continue;

        QString invSizeName = rtInfo.name_ + "InvSize";
        QString offsetsName = rtInfo.name_ + "Offsets";
        float width = (float)renderTargets_[nameHash]->GetWidth();
        float height = (float)renderTargets_[nameHash]->GetHeight();

        const Vector2& pixelUVOffset = Graphics::GetPixelUVOffset();
        graphics_->SetShaderParameter(invSizeName, Vector2(1.0f / width, 1.0f / height));
        graphics_->SetShaderParameter(offsetsName, Vector2(pixelUVOffset.x_ / width, pixelUVOffset.y_ / height));
    }

    // Set command's shader parameters last to allow them to override any of the above
    const VariantMap& parameters = command.shaderParameters_;
    for (VariantMap::const_iterator parameter=parameters.begin(),fin=parameters.end(); parameter!=fin; ++parameter)
        graphics_->SetShaderParameter(MAP_KEY(parameter), MAP_VALUE(parameter));

    graphics_->SetBlendMode(command.blendMode_);
    graphics_->SetDepthTest(CMP_ALWAYS);
    graphics_->SetDepthWrite(false);
    graphics_->SetFillMode(FILL_SOLID);
    graphics_->SetLineAntiAlias(false);
    graphics_->SetClipPlane(false);
    graphics_->SetScissorTest(false);
    graphics_->SetStencilTest(false);

    DrawFullscreenQuad(false);
}

bool View::IsNecessary(const RenderPathCommand& command)
{

    return command.enabled_ && !command.outputs_.empty() && (command.type_ != CMD_SCENEPASS ||
            !batchQueueStorage_[batchQueues_[command.passIndex_]].IsEmpty());
}

bool View::CheckViewportRead(const RenderPathCommand& command)
{
    for (const QString & nm : command.textureNames_)
    {
        if (!nm.isEmpty() && (nm.compare("viewport", Qt::CaseInsensitive) == 0))
            return true;
    }
    return false;
}

bool View::CheckViewportWrite(const RenderPathCommand& command)
{
    for (const std::pair<QString, CubeMapFace> &outp : command.outputs_)
    {
        if (outp.first.compare("viewport", Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}


bool View::CheckPingpong(unsigned index)
{
    // Current command must be a viewport-reading & writing quad to begin the pingpong chain
    RenderPathCommand& current = renderPath_->commands_[index];
    if (current.type_ != CMD_QUAD || !CheckViewportRead(current) || !CheckViewportWrite(current))
        return false;

    // If there are commands other than quads that target the viewport, we must keep rendering to the final target and resolving
    // to a viewport texture when necessary instead of pingponging, as a scene pass is not guaranteed to fill the entire viewport
    for (unsigned i = index + 1; i < renderPath_->commands_.size(); ++i)
    {
        RenderPathCommand& command = renderPath_->commands_[i];
        if (!IsNecessary(command))
            continue;
        if (CheckViewportWrite(command))
        {
            if (command.type_ != CMD_QUAD)
                return false;
        }
    }

    return true;
}

void View::AllocateScreenBuffers()
{
    View* actualView = sourceView_ != nullptr ? sourceView_ : this;
    bool hasScenePassToRTs = false;
    bool hasCustomDepth = false;
    bool hasViewportRead = false;
    bool hasPingpong = false;
    bool needSubstitute = false;
    unsigned numViewportTextures = 0;
    depthOnlyDummyTexture_ = nullptr;

    // Check for commands with special meaning: has custom depth, renders a scene pass to other than the destination viewport,
    // read the viewport, or pingpong between viewport textures. These may trigger the need to substitute the destination RT
    for (unsigned i = 0; i < renderPath_->commands_.size(); ++i)
    {
        const RenderPathCommand& command = renderPath_->commands_[i];
        if (!actualView->IsNecessary(command))
            continue;
        if (!hasViewportRead && CheckViewportRead(command))
            hasViewportRead = true;
        if (!hasPingpong && CheckPingpong(i))
            hasPingpong = true;
        if (command.depthStencilName_.length() != 0)
            hasCustomDepth = true;
        if (!hasScenePassToRTs && command.type_ == CMD_SCENEPASS)
        {
            for (const std::pair<QString, CubeMapFace> & outp : command.outputs_)
            {
                if (outp.first.compare("viewport", Qt::CaseInsensitive) != 0)
                {
                    hasScenePassToRTs = true;
                    break;
                }
            }
        }
    }
    // Due to FBO limitations, in OpenGL deferred modes need to render to texture first and then blit to the backbuffer
    // Also, if rendering to a texture with full deferred rendering, it must be RGBA to comply with the rest of the buffers,
    // unless using OpenGL 3
    if (((deferred_ || hasScenePassToRTs) && (renderTarget_ == nullptr)) )
        needSubstitute = true;
    // Also need substitute if rendering to backbuffer using a custom (readable) depth buffer
    if ((renderTarget_ == nullptr) && hasCustomDepth)
        needSubstitute = true;

    // If backbuffer is antialiased when using deferred rendering, need to reserve a buffer
    if (deferred_ && (renderTarget_ == nullptr) && graphics_->GetMultiSample() > 1)
        needSubstitute = true;
    // If viewport is smaller than whole texture/backbuffer in deferred rendering, need to reserve a buffer, as the G-buffer
    // textures will be sized equal to the viewport
    if (viewSize_.x_ < rtSize_.x_ || viewSize_.y_ < rtSize_.y_)
    {
        if (deferred_ || hasScenePassToRTs || hasCustomDepth)
            needSubstitute = true;
    }

    // Follow final rendertarget format, or use RGB to match the backbuffer format
    gl::GLenum format = renderTarget_ != nullptr ? renderTarget_->GetParentTexture()->GetFormat() : Graphics::GetRGBFormat();

    // If HDR rendering is enabled use RGBA16f and reserve a buffer
    bool hdrRendering = renderer_->GetHDRRendering();

    if (hdrRendering)
    {
        format = Graphics::GetRGBAFloat16Format();
        needSubstitute = true;
    }

    if (hasViewportRead)
    {
        ++numViewportTextures;

        // If we have viewport read and target is a cube map, must allocate a substitute target instead as BlitFramebuffer()
        // does not support reading a cube map
        if ((renderTarget_ != nullptr) && renderTarget_->GetParentTexture()->GetType() == TextureCube::GetTypeStatic())
            needSubstitute = true;

        // If rendering to a texture, but the viewport is less than the whole texture, use a substitute to ensure
        // postprocessing shaders will never read outside the viewport
        if ((renderTarget_ != nullptr) && (viewSize_.x_ < renderTarget_->GetWidth() || viewSize_.y_ < renderTarget_->GetHeight()))
            needSubstitute = true;

        if (hasPingpong && !needSubstitute)
            ++numViewportTextures;
    }

    // Allocate screen buffers. Enable filtering in case the quad commands need that
    // Follow the sRGB mode of the destination render target
    bool sRGB = renderTarget_ != nullptr ? renderTarget_->GetParentTexture()->GetSRGB() : graphics_->GetSRGB();
    int multiSample = renderTarget_ != nullptr ? renderTarget_->GetMultiSample() : graphics_->GetMultiSample();
    bool autoResolve = renderTarget_ != nullptr ? renderTarget_->GetAutoResolve() : true;
    substituteRenderTarget_ = needSubstitute ? GetRenderSurfaceFromTexture(renderer_->GetScreenBuffer(viewSize_.x_, viewSize_.y_,
        format, multiSample, autoResolve, false, true, sRGB)) : (RenderSurface*)nullptr;
    for (unsigned i = 0; i < MAX_VIEWPORT_TEXTURES; ++i)
    {
        viewportTextures_[i] = i < numViewportTextures ? renderer_->GetScreenBuffer(viewSize_.x_, viewSize_.y_, format, multiSample,
            autoResolve, false, true, sRGB) : (Texture*)nullptr;
    }
    // If using a substitute render target and pingponging, the substitute can act as the second viewport texture
    if (numViewportTextures == 1 && (substituteRenderTarget_ != nullptr))
        viewportTextures_[1] = substituteRenderTarget_->GetParentTexture();

    // Allocate extra render targets defined by the render path
    for (const RenderTargetInfo& rtInfo : renderPath_->renderTargets_)
    {
        if (!rtInfo.enabled_)
            continue;

        float width = rtInfo.size_.x_;
        float height = rtInfo.size_.y_;

        if (rtInfo.sizeMode_ == SIZE_VIEWPORTDIVISOR)
        {
            width = (float)viewSize_.x_ / Max(width, M_EPSILON);
            height = (float)viewSize_.y_ / Max(height, M_EPSILON);
        }
        else if (rtInfo.sizeMode_ == SIZE_VIEWPORTMULTIPLIER)
        {
            width = (float)viewSize_.x_ * width;
            height = (float)viewSize_.y_ * height;
        }

        int intWidth = (int)(width + 0.5f);
        int intHeight = (int)(height + 0.5f);

        // If the rendertarget is persistent, key it with a hash derived from the RT name and the view's pointer
        renderTargets_[rtInfo.name_] = renderer_->GetScreenBuffer(intWidth, intHeight, rtInfo.format_, rtInfo.multiSample_, rtInfo.autoResolve_,
                rtInfo.cubemap_, rtInfo.filtered_, rtInfo.sRGB_, rtInfo.persistent_ ? StringHash(rtInfo.name_).Value()
                + ptrHash(this) : 0);
    }
}

void View::BlitFramebuffer(Texture* source, RenderSurface* destination, bool depthWrite)
{
    if (source == nullptr)
        return;

    URHO3D_PROFILE(BlitFramebuffer);

    // If blitting to the destination rendertarget, use the actual viewport. Intermediate textures on the other hand
    // are always viewport-sized
    IntVector2 srcSize(source->GetWidth(), source->GetHeight());
    IntVector2 destSize = destination != nullptr ? IntVector2(destination->GetWidth(), destination->GetHeight()) : IntVector2(
                                            graphics_->GetWidth(), graphics_->GetHeight());

    IntRect srcRect = (GetRenderSurfaceFromTexture(source) == renderTarget_) ? viewRect_ : IntRect(0, 0, srcSize.x_, srcSize.y_);
    IntRect destRect = (destination == renderTarget_) ? viewRect_ : IntRect(0, 0, destSize.x_, destSize.y_);

    graphics_->SetBlendMode(BLEND_REPLACE);
    graphics_->SetDepthTest(CMP_ALWAYS);
    graphics_->SetDepthWrite(depthWrite);
    graphics_->SetFillMode(FILL_SOLID);
    graphics_->SetLineAntiAlias(false);
    graphics_->SetClipPlane(false);
    graphics_->SetScissorTest(false);
    graphics_->SetStencilTest(false);
    graphics_->SetRenderTarget(0, destination);
    for (unsigned i = 1; i < MAX_RENDERTARGETS; ++i)
        graphics_->SetRenderTarget(i, (RenderSurface*)nullptr);
    graphics_->SetDepthStencil(GetDepthStencil(destination));
    graphics_->SetViewport(destRect);

    static const QString shaderName("CopyFramebuffer");
    graphics_->SetShaders(graphics_->GetShader(VS, shaderName), graphics_->GetShader(PS, shaderName));

    SetGBufferShaderParameters(srcSize, srcRect);

    graphics_->SetTexture(TU_DIFFUSE, source);
    DrawFullscreenQuad(true);
}

void View::DrawFullscreenQuad(bool setIdentityProjection)
{
    Geometry* geometry = renderer_->GetQuadGeometry();

    // If no camera, no choice but to use identity projection
    if (camera_ == nullptr)
        setIdentityProjection = true;

    if (setIdentityProjection)
    {
        Matrix3x4 model = Matrix3x4::IDENTITY;
        Matrix4 projection = Matrix4::IDENTITY;
        if ((camera_ != nullptr) && camera_->GetFlipVertical())
            projection.m11_ = -1.0f;
        model.m23_ = 0.0f;

        graphics_->SetShaderParameter(VSP_MODEL, model);
        graphics_->SetShaderParameter(VSP_VIEWPROJ, projection);
    }
    else
        graphics_->SetShaderParameter(VSP_MODEL, Light::GetFullscreenQuadTransform(camera_));

    graphics_->SetCullMode(CULL_NONE);
    graphics_->ClearTransformSources();

    geometry->Draw(graphics_);
}

void View::UpdateOccluders(std::vector<Drawable*>& occluders, Camera* camera)
{
    float occluderSizeThreshold_ = renderer_->GetOccluderSizeThreshold();
    float halfViewSize = camera->GetHalfViewSize();
    float invOrthoSize = 1.0f / camera->GetOrthoSize();

    for (std::vector<Drawable*>::iterator i = occluders.begin(); i != occluders.end();)
    {
        Drawable* occluder = *i;
        bool erase = false;

        if (!occluder->IsInView(frame_, true))
            occluder->UpdateBatches(frame_);

        // Check occluder's draw distance (in main camera view)
        float maxDistance = occluder->GetDrawDistance();
        if (maxDistance <= 0.0f || occluder->GetDistance() <= maxDistance)
        {
            // Check that occluder is big enough on the screen
            const BoundingBox& box = occluder->GetWorldBoundingBox();
            float diagonal = box.size().Length();
            float compare;
            if (!camera->IsOrthographic())
            {
                // Occluders which are near the camera are more useful then occluders at the end of the camera's draw distance
                float cameraMaxDistanceFraction = occluder->GetDistance() / camera->GetFarClip();
                compare = diagonal * halfViewSize / (occluder->GetDistance() * cameraMaxDistanceFraction);

                // Give higher priority to occluders which the camera is inside their AABB
                const Vector3& cameraPos = camera->GetNode() != nullptr ? camera->GetNode()->GetWorldPosition() : Vector3::ZERO;
                if (box.IsInside(cameraPos) != 0u)
                    compare *= diagonal;    // size^2
            }
            else
                compare = diagonal * invOrthoSize;

            if (compare < occluderSizeThreshold_)
                erase = true;
            else
            {
                // Best occluders have big triangles (low density)
                float density = occluder->GetNumOccluderTriangles() / diagonal;
                // Lower value is higher priority
                occluder->SetSortValue(density / compare);
            }
        }
        else
            erase = true;

        if (erase)
            i = occluders.erase(i);
        else
            ++i;
    }

    // Sort occluders so that if triangle budget is exceeded, best occluders have been drawn
    if (!occluders.empty())
        std::sort(occluders.begin(), occluders.end(), CompareDrawables);
}

void View::DrawOccluders(OcclusionBuffer* buffer, const std::vector<Drawable*>& occluders)
{
    buffer->SetMaxTriangles((unsigned)maxOccluderTriangles_);
    buffer->Clear();
    if (!buffer->IsThreaded())
    {
        // If not threaded, draw occluders one by one and test the next occluder against already rasterized depth
        for (unsigned i = 0; i < occluders.size(); ++i)
        {
            Drawable* occluder = occluders[i];
            if (i > 0)
            {
                // For subsequent occluders, do a test against the pixel-level occlusion buffer to see if rendering is necessary
                if (!buffer->IsVisible(occluder->GetWorldBoundingBox()))
                    continue;
            }

            // Check for running out of triangles
            ++activeOccluders_;
            bool success = occluder->DrawOcclusion(buffer);
            // Draw triangles submitted by this occluder
            buffer->DrawTriangles();
            if (!success)
                break;
        }
    }
    else
    {
        // In threaded mode submit all triangles first, then render (cannot test in this case)
        for (Drawable* occld : occluders)
        {
            // Check for running out of triangles
            ++activeOccluders_;
            if (!occld->DrawOcclusion(buffer))
                break;
        }

        buffer->DrawTriangles();
    }

    // Finally build the depth mip levels
    buffer->BuildDepthHierarchy();
}

void View::ProcessLight(LightQueryResult& query, unsigned threadIndex)
{
    Light* light = query.light_;
    LightType type = light->GetLightType();
    const Frustum& frustum = cullCamera_->GetFrustum();

    // Check if light should be shadowed
    bool isShadowed = drawShadows_ && light->GetCastShadows() && !light->GetPerVertex() && light->GetShadowIntensity() < 1.0f;
    // If shadow distance non-zero, check it
    if (isShadowed && light->GetShadowDistance() > 0.0f && light->GetDistance() > light->GetShadowDistance())
        isShadowed = false;

    // Get lit geometries. They must match the light mask and be inside the main camera frustum to be considered
    std::vector<Drawable*>& tempDrawables = tempDrawables_[threadIndex];
    query.litGeometries_.clear();

    switch (type)
    {
    case LIGHT_DIRECTIONAL:
        for (Drawable* drawabl : geometries_)
        {
            if ((GetLightMask(drawabl) & light->GetLightMask()) != 0u)
                query.litGeometries_.push_back(drawabl);
        }
        break;

    case LIGHT_SPOT:
    {
        FrustumOctreeQuery octreeQuery(tempDrawables, light->GetFrustum(), DRAWABLE_GEOMETRY,
                                       cullCamera_->GetViewMask());
        octree_->GetDrawables(octreeQuery);
        for (Drawable* td : tempDrawables)
        {
            if (td->IsInView(frame_) && ((GetLightMask(td) & light->GetLightMask()) != 0u))
                query.litGeometries_.push_back(td);
        }
    }
        break;

    case LIGHT_POINT:
    {
        SphereOctreeQuery octreeQuery(tempDrawables, Sphere(light->GetNode()->GetWorldPosition(), light->GetRange()),
                                      DRAWABLE_GEOMETRY, cullCamera_->GetViewMask());
        octree_->GetDrawables(octreeQuery);
        for (Drawable* td : tempDrawables)
        {
            if (td->IsInView(frame_) && ((GetLightMask(td) & light->GetLightMask()) != 0u))
                query.litGeometries_.push_back(td);
        }
    }
        break;
    }

    // If no lit geometries or not shadowed, no need to process shadow cameras
    if (query.litGeometries_.empty() || !isShadowed)
    {
        query.numSplits_ = 0;
        return;
    }

    // Determine number of shadow cameras and setup their initial positions
    SetupShadowCameras(query);

    // Process each split for shadow casters
    query.shadowCasters_.clear();
    for (unsigned i = 0; i < query.numSplits_; ++i)
    {
        LightQueryShadowEntry &entry(query.shadowEntries_[i]);
        Camera* shadowCamera = entry.shadowCameras_;
        const Frustum& shadowCameraFrustum = shadowCamera->GetFrustum();
        entry.shadowCasterBegin_ = entry.shadowCasterEnd_ = query.shadowCasters_.size();

        // For point light check that the face is visible: if not, can skip the split
        if (type == LIGHT_POINT && frustum.IsInsideFast(BoundingBox(shadowCameraFrustum)) == OUTSIDE)
            continue;

        // For directional light check that the split is inside the visible scene: if not, can skip the split
        if (type == LIGHT_DIRECTIONAL)
        {
            if (minZ_ > entry.shadowFarSplits_)
                continue;
            if (maxZ_ < entry.shadowNearSplits_)
                continue;

            // Reuse lit geometry query for all except directional lights
            ShadowCasterOctreeQuery query(tempDrawables, shadowCameraFrustum, DRAWABLE_GEOMETRY, cullCamera_->GetViewMask());
            octree_->GetDrawables(query);
        }

        // Check which shadow casters actually contribute to the shadowing
        ProcessShadowCasters(query, tempDrawables, entry);
    }

    // If no shadow casters, the light can be rendered unshadowed. At this point we have not allocated a shadow map yet, so the
    // only cost has been the shadow camera setup & queries
    if (query.shadowCasters_.empty())
        query.numSplits_ = 0;
}

void View::ProcessShadowCasters(LightQueryResult& query, const std::vector<Drawable*>& drawables, LightQueryShadowEntry &entry)
{
    Light* light = query.light_;

    Camera* shadowCamera = entry.shadowCameras_;
    const Frustum& shadowCameraFrustum = shadowCamera->GetFrustum();
    const Matrix3x4& lightView = shadowCamera->GetView();
    const Matrix4& lightProj = shadowCamera->GetProjection();
    LightType type = light->GetLightType();

    entry.shadowCasterBox_.Clear();

    // Transform scene frustum into shadow camera's view space for shadow caster visibility check. For point & spot lights,
    // we can use the whole scene frustum. For directional lights, use the intersection of the scene frustum and the split
    // frustum, so that shadow casters do not get rendered into unnecessary splits
    Frustum lightViewFrustum;
    if (type != LIGHT_DIRECTIONAL)
        lightViewFrustum = cullCamera_->GetSplitFrustum(minZ_, maxZ_).Transformed(lightView);
    else
        lightViewFrustum = cullCamera_->GetSplitFrustum(std::max(minZ_, entry.shadowNearSplits_),
                                                        std::min(maxZ_, entry.shadowFarSplits_)).Transformed(lightView);

    BoundingBox lightViewFrustumBox(lightViewFrustum);

    // Check for degenerate split frustum: in that case there is no need to get shadow casters
    if (lightViewFrustum.vertices_[0] == lightViewFrustum.vertices_[4])
        return;

    BoundingBox lightViewBox;
    BoundingBox lightProjBox;

    for (Drawable* drawable : drawables)
    {
        // In case this is a point or spot light query result reused for optimization, we may have non-shadowcasters included.
        // Check for that first
        if (!drawable->GetCastShadows())
            continue;
        // Check shadow mask
        if ((GetShadowMask(drawable) & light->GetLightMask()) == 0u)
            continue;
        // For point light, check that this drawable is inside the split shadow camera frustum
        if (type == LIGHT_POINT && shadowCameraFrustum.IsInsideFast(drawable->GetWorldBoundingBox()) == OUTSIDE)
            continue;

        // Check shadow distance
        // Note: as lights are processed threaded, it is possible a drawable's UpdateBatches() function is called several
        // times. However, this should not cause problems as no scene modification happens at this point.
        if (!drawable->IsInView(frame_, true))
            drawable->UpdateBatches(frame_);
        float maxShadowDistance = drawable->GetShadowDistance();
        float drawDistance = drawable->GetDrawDistance();
        if (drawDistance > 0.0f && (maxShadowDistance <= 0.0f || drawDistance < maxShadowDistance))
            maxShadowDistance = drawDistance;
        if (maxShadowDistance > 0.0f && drawable->GetDistance() > maxShadowDistance)
            continue;

        // Project shadow caster bounding box to light view space for visibility check
        lightViewBox = drawable->GetWorldBoundingBox().Transformed(lightView);

        if (IsShadowCasterVisible(drawable, lightViewBox, shadowCamera, lightView, lightViewFrustum, lightViewFrustumBox))
        {
            // Merge to shadow caster bounding box (only needed for focused spot lights) and add to the list
            if (type == LIGHT_SPOT && light->GetShadowFocus().focus_)
            {
                lightProjBox = lightViewBox.Projected(lightProj);
                entry.shadowCasterBox_.Merge(lightProjBox);
            }
            query.shadowCasters_.push_back(drawable);
        }
    }

    entry.shadowCasterEnd_ = query.shadowCasters_.size();
}

bool View::IsShadowCasterVisible(Drawable* drawable, BoundingBox lightViewBox, Camera* shadowCamera, const Matrix3x4& lightView,
                                 const Frustum& lightViewFrustum, const BoundingBox& lightViewFrustumBox)
{
    if (shadowCamera->IsOrthographic())
    {
        // Extrude the light space bounding box up to the far edge of the frustum's light space bounding box
        lightViewBox.max_.z_ = Max(lightViewBox.max_.z_,lightViewFrustumBox.max_.z_);
        return lightViewFrustum.IsInsideFast(lightViewBox) != OUTSIDE;
    }
    else
    {
        // If light is not directional, can do a simple check: if object is visible, its shadow is too
        if (drawable->IsInView(frame_))
            return true;

        // For perspective lights, extrusion direction depends on the position of the shadow caster
        Vector3 center = lightViewBox.Center();
        Ray extrusionRay(center, center);

        float extrusionDistance = shadowCamera->GetFarClip();
        float originalDistance = Clamp(center.Length(), M_EPSILON, extrusionDistance);

        // Because of the perspective, the bounding box must also grow when it is extruded to the distance
        float sizeFactor = extrusionDistance / originalDistance;

        // Calculate the endpoint box and merge it to the original. Because it's axis-aligned, it will be larger
        // than necessary, so the test will be conservative
        Vector3 newCenter = extrusionDistance * extrusionRay.direction_;
        Vector3 newHalfSize = lightViewBox.size() * sizeFactor * 0.5f;
        BoundingBox extrudedBox(newCenter - newHalfSize, newCenter + newHalfSize);
        lightViewBox.Merge(extrudedBox);

        return lightViewFrustum.IsInsideFast(lightViewBox) != OUTSIDE;
    }
}

IntRect View::GetShadowMapViewport(Light* light, unsigned splitIndex, Texture2D* shadowMap)
{
    unsigned width = shadowMap->GetWidth();
    unsigned height = shadowMap->GetHeight();

    switch (light->GetLightType())
    {
    case LIGHT_DIRECTIONAL:
    {
        int numSplits = light->GetNumShadowSplits();
        if (numSplits == 1)
            return IntRect(0, 0, width, height);
        else if (numSplits == 2)
            return IntRect(splitIndex * width / 2, 0, (splitIndex + 1) * width / 2, height);
        else
            return IntRect((splitIndex & 1) * width / 2, (splitIndex / 2) * height / 2, ((splitIndex & 1) + 1) * width / 2,
                           (splitIndex / 2 + 1) * height / 2);
    }

    case LIGHT_SPOT:
        return IntRect(0, 0, width, height);

    case LIGHT_POINT:
        return IntRect((splitIndex & 1) * width / 2, (splitIndex / 2) * height / 3, ((splitIndex & 1) + 1) * width / 2,
                       (splitIndex / 2 + 1) * height / 3);
    }

    return IntRect();
}

void View::SetupShadowCameras(LightQueryResult& query)
{
    Light* light = query.light_;

    int splits = 0;

    switch (light->GetLightType())
    {
    case LIGHT_DIRECTIONAL:
    {
        const CascadeParameters& cascade = light->GetShadowCascade();

        float nearSplit = cullCamera_->GetNearClip();
        float farSplit;
        int numSplits = light->GetNumShadowSplits();

        while (splits < numSplits)
        {
            // If split is completely beyond camera far clip, we are done
            if (nearSplit > cullCamera_->GetFarClip())
                break;

            farSplit = std::min(cullCamera_->GetFarClip(), cascade.splits_[splits]);
            if (farSplit <= nearSplit)
                break;

            // Setup the shadow camera for the split
            LightQueryShadowEntry &entry(query.shadowEntries_[splits]);
            Camera* shadowCamera = renderer_->GetShadowCamera();
            entry.shadowCameras_ = shadowCamera;
            entry.shadowNearSplits_ = nearSplit;
            entry.shadowFarSplits_ = farSplit;
            SetupDirLightShadowCamera(shadowCamera, light, nearSplit, farSplit);

            nearSplit = farSplit;
            ++splits;
        }
    }
        break;

    case LIGHT_SPOT:
    {
        Camera* shadowCamera = renderer_->GetShadowCamera();
        query.shadowEntries_[0].shadowCameras_ = shadowCamera;
        Node* cameraNode = shadowCamera->GetNode();
        Node* lightNode = light->GetNode();

        cameraNode->SetTransform(lightNode->GetWorldPosition(), lightNode->GetWorldRotation());
        shadowCamera->SetNearClip(light->GetShadowNearFarRatio() * light->GetRange());
        shadowCamera->SetFarClip(light->GetRange());
        shadowCamera->SetFov(light->GetFov());
        shadowCamera->SetAspectRatio(light->GetAspectRatio());

        splits = 1;
    }
        break;

    case LIGHT_POINT:
    {
        for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
        {
            Camera* shadowCamera = renderer_->GetShadowCamera();
            LightQueryShadowEntry &entry(query.shadowEntries_[i]);
            entry.shadowCameras_ = shadowCamera;
            Node* cameraNode = shadowCamera->GetNode();

            // When making a shadowed point light, align the splits along X, Y and Z axes regardless of light rotation
            cameraNode->SetPosition(light->GetNode()->GetWorldPosition());
            cameraNode->SetDirection(*directions[i]);
            shadowCamera->SetNearClip(light->GetShadowNearFarRatio() * light->GetRange());
            shadowCamera->SetFarClip(light->GetRange());
            shadowCamera->SetFov(90.0f);
            shadowCamera->SetAspectRatio(1.0f);
        }

        splits = MAX_CUBEMAP_FACES;
    }
        break;
    }

    query.numSplits_ = splits;
}

void View::SetupDirLightShadowCamera(Camera* shadowCamera, Light* light, float nearSplit, float farSplit)
{
    Node* shadowCameraNode = shadowCamera->GetNode();
    Node* lightNode = light->GetNode();
    float extrusionDistance = Min(cullCamera_->GetFarClip(), light->GetShadowMaxExtrusion());
    const FocusParameters& parameters = light->GetShadowFocus();

    // Calculate initial position & rotation
    Vector3 pos = cullCamera_->GetNode()->GetWorldPosition() - extrusionDistance * lightNode->GetWorldDirection();
    shadowCameraNode->SetTransform(pos, lightNode->GetWorldRotation());

    // Calculate main camera shadowed frustum in light's view space
    farSplit = std::min(farSplit, cullCamera_->GetFarClip());
    // Use the scene Z bounds to limit frustum size if applicable
    if (parameters.focus_)
    {
        nearSplit = std::max(minZ_, nearSplit);
        farSplit = std::min(maxZ_, farSplit);
    }

    Frustum splitFrustum = cullCamera_->GetSplitFrustum(nearSplit, farSplit);
    Polyhedron frustumVolume;
    frustumVolume.Define(splitFrustum);
    // If focusing enabled, clip the frustum volume by the combined bounding box of the lit geometries within the frustum
    if (parameters.focus_)
    {
        BoundingBox litGeometriesBox;
        unsigned lightMask = light->GetLightMask();
        for (Drawable* drawable : geometries_)
        {
            if (drawable->GetMinZ() <= farSplit && drawable->GetMaxZ() >= nearSplit &&
                ((GetLightMask(drawable) & lightMask) != 0u))
                litGeometriesBox.Merge(drawable->GetWorldBoundingBox());
        }
        if (litGeometriesBox.Defined())
        {
            frustumVolume.Clip(litGeometriesBox);
            // If volume became empty, restore it to avoid zero size
            if (frustumVolume.Empty())
                frustumVolume.Define(splitFrustum);
        }
    }

    // Transform frustum volume to light space
    const Matrix3x4& lightView = shadowCamera->GetView();
    frustumVolume.Transform(lightView);

    // Fit the frustum volume inside a bounding box. If uniform size, use a sphere instead
    BoundingBox shadowBox;
    if (!parameters.nonUniform_)
        shadowBox.Define(Sphere(frustumVolume));
    else
        shadowBox.Define(frustumVolume);

    shadowCamera->SetOrthographic(true);
    shadowCamera->SetAspectRatio(1.0f);
    shadowCamera->SetNearClip(0.0f);
    shadowCamera->SetFarClip(shadowBox.max_.z_);

    // Center shadow camera on the bounding box. Can not snap to texels yet as the shadow map viewport is unknown
    QuantizeDirLightShadowCamera(shadowCamera, parameters, IntRect(0, 0, 0, 0), shadowBox);
}

void View::FinalizeShadowCamera(Camera* shadowCamera, Light* light, const IntRect& shadowViewport,
                                const BoundingBox& shadowCasterBox)
{
    const FocusParameters& parameters(light->GetShadowFocus());
    float shadowMapWidth = (float)(shadowViewport.Width());
    LightType type = light->GetLightType();

    if (type == LIGHT_DIRECTIONAL)
    {
        BoundingBox shadowBox;
        shadowBox.max_.y_ = shadowCamera->GetOrthoSize() * 0.5f;
        shadowBox.max_.x_ = shadowCamera->GetAspectRatio() * shadowBox.max_.y_;
        shadowBox.min_.y_ = -shadowBox.max_.y_;
        shadowBox.min_.x_ = -shadowBox.max_.x_;

        // Requantize and snap to shadow map texels
        QuantizeDirLightShadowCamera(shadowCamera, parameters, shadowViewport, shadowBox);
    }

    if (type == LIGHT_SPOT && parameters.focus_)
    {
        float viewSizeX = Max(Abs(shadowCasterBox.min_.x_), Abs(shadowCasterBox.max_.x_));
        float viewSizeY = Max(Abs(shadowCasterBox.min_.y_), Abs(shadowCasterBox.max_.y_));
        float viewSize = Max(viewSizeX, viewSizeY);
        // Scale the quantization parameters, because view size is in projection space (-1.0 - 1.0)
        float invOrthoSize = 1.0f / shadowCamera->GetOrthoSize();
        float quantize = parameters.quantize_ * invOrthoSize;
        float minView = parameters.minView_ * invOrthoSize;

        viewSize = Max(ceilf(viewSize / quantize) * quantize, minView);
        if (viewSize < 1.0f)
            shadowCamera->SetZoom(1.0f / viewSize);
    }

    // Perform a finalization step for all lights: ensure zoom out of 2 pixels to eliminate border filtering issues
    // For point lights use 4 pixels, as they must not cross sides of the virtual cube map (maximum 3x3 PCF)
    float shadowCamZoom = shadowCamera->GetZoom();
    if (shadowCamZoom >= 1.0f)
    {
        if (light->GetLightType() != LIGHT_POINT)
            shadowCamera->SetZoom(shadowCamZoom * ((shadowMapWidth - 2.0f) / shadowMapWidth));
        else
        {
            shadowCamera->SetZoom(shadowCamZoom * ((shadowMapWidth - 3.0f) / shadowMapWidth));
        }
    }
}

void View::QuantizeDirLightShadowCamera(Camera* shadowCamera, const FocusParameters& shadowFocusParameters, const IntRect& shadowViewport,
                                        const BoundingBox& viewBox)
{
    Node* shadowCameraNode = shadowCamera->GetNode();
    float shadowMapWidth = (float)(shadowViewport.Width());

    float minX = viewBox.min_.x_;
    float minY = viewBox.min_.y_;
    float maxX = viewBox.max_.x_;
    float maxY = viewBox.max_.y_;

    Vector2 center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    Vector2 viewSize(maxX - minX, maxY - minY);

    // Quantize size to reduce swimming
    // Note: if size is uniform and there is no focusing, quantization is unnecessary
    if (shadowFocusParameters.nonUniform_)
    {
        viewSize.x_ = ceilf(sqrtf(viewSize.x_ / shadowFocusParameters.quantize_));
        viewSize.y_ = ceilf(sqrtf(viewSize.y_ / shadowFocusParameters.quantize_));
        viewSize.x_ = Max(viewSize.x_ * viewSize.x_ * shadowFocusParameters.quantize_, shadowFocusParameters.minView_);
        viewSize.y_ = Max(viewSize.y_ * viewSize.y_ * shadowFocusParameters.quantize_, shadowFocusParameters.minView_);
    }
    else if (shadowFocusParameters.focus_)
    {
        viewSize.x_ = Max(viewSize.x_, viewSize.y_);
        viewSize.x_ = ceilf(sqrtf(viewSize.x_ / shadowFocusParameters.quantize_));
        viewSize.x_ = Max(viewSize.x_ * viewSize.x_ * shadowFocusParameters.quantize_, shadowFocusParameters.minView_);
        viewSize.y_ = viewSize.x_;
    }

    shadowCamera->SetOrthoSize(viewSize);

    // Center shadow camera to the view space bounding box
    Quaternion rot(shadowCameraNode->GetWorldRotation());
    Vector3 adjust(center.x_, center.y_, 0.0f);
    shadowCameraNode->Translate(rot * adjust, TS_WORLD);

    // If the shadow map viewport is known, snap to whole texels
    if (shadowMapWidth > 0.0f)
    {
        Vector3 viewPos(rot.Inverse() * shadowCameraNode->GetWorldPosition());
        // Take into account that shadow map border will not be used
        float invActualSize = 1.0f / (shadowMapWidth - 2.0f);
        Vector2 texelSize(viewSize.x_ * invActualSize, viewSize.y_ * invActualSize);
        Vector3 snap(-fmodf(viewPos.x_, texelSize.x_), -fmodf(viewPos.y_, texelSize.y_), 0.0f);
        shadowCameraNode->Translate(rot * snap, TS_WORLD);
    }
}

void View::FindZone(Drawable* drawable)
{
    Vector3 center = drawable->GetWorldBoundingBox().Center();
    int bestPriority = M_MIN_INT;
    Zone* newZone = nullptr;

    // If bounding box center is in view, the zone assignment is conclusive also for next frames. Otherwise it is temporary
    // (possibly incorrect) and must be re-evaluated on the next frame
    bool temporary = cullCamera_->GetFrustum().IsInside(center) == 0u;

    // First check if the current zone remains a conclusive result
    Zone* lastZone = drawable->GetZone();

    if ((lastZone != nullptr) && ((lastZone->GetViewMask() & cullCamera_->GetViewMask()) != 0u) && lastZone->GetPriority() >= highestZonePriority_ &&
            ((drawable->GetZoneMask() & lastZone->GetZoneMask()) != 0u) && lastZone->IsInside(center))
        newZone = lastZone;
    else
    {
        for (Zone* zone : zones_)
        {
            int priority = zone->GetPriority();
            if (priority > bestPriority && ((drawable->GetZoneMask() & zone->GetZoneMask()) != 0u) && zone->IsInside(center))
            {
                newZone = zone;
                bestPriority = priority;
            }
        }
    }

    drawable->SetZone(newZone, temporary);
}

Technique* View::GetTechnique(Drawable* drawable, Material* material)
{
    assert(material!=nullptr);

    const std::vector<TechniqueEntry>& techniques = material->GetTechniques();
    if (techniques.empty())
        return nullptr;                      // No techniques no choice at all
    if (techniques.size() == 1)
        return techniques[0].technique_; // If only one technique, no choice
    float lodDistance = drawable->GetLodDistance();

    // Check for suitable technique. Techniques should be ordered like this:
    // Most distant & highest quality
    // Most distant & lowest quality
    // Second most distant & highest quality
    // ...
    for (const TechniqueEntry& entry : techniques)
    {
        Technique* tech = entry.technique_;

        if ((tech == nullptr) || (!tech->IsSupported()) || materialQuality_ < entry.qualityLevel_)
            continue;
        if (lodDistance >= entry.lodDistance_)
            return tech;
    }

    // If no suitable technique found, fallback to the last
    return techniques.back().technique_;
}

void View::CheckMaterialForAuxView(Material* material)
{
    for (const auto & i : material->GetTextures())
    {
        Texture* texture = ELEMENT_VALUE(i).Get();
        if ((texture != nullptr) && texture->GetUsage() == TEXTURE_RENDERTARGET)
        {
            // Have to check cube & 2D textures separately
            if (texture->GetType() == Texture2D::GetTypeStatic())
            {
                Texture2D* tex2D = static_cast<Texture2D*>(texture);
                RenderSurface* target = tex2D->GetRenderSurface();
                if ((target != nullptr) && target->GetUpdateMode() == SURFACE_UPDATEVISIBLE)
                    target->QueueUpdate();
            }
            else if (texture->GetType() == TextureCube::GetTypeStatic())
            {
                TextureCube* texCube = static_cast<TextureCube*>(texture);
                for (unsigned j = 0; j < MAX_CUBEMAP_FACES; ++j)
                {
                    RenderSurface* target = texCube->GetRenderSurface((CubeMapFace)j);
                    if ((target != nullptr) && target->GetUpdateMode() == SURFACE_UPDATEVISIBLE)
                        target->QueueUpdate();
                }
            }
        }
    }

    // Flag as processed so we can early-out next time we come across this material on the same frame
    material->MarkForAuxView(frame_.frameNumber_);
}

void View::AddBatchToQueue(BatchQueue& batchQueue, Batch batch, const Technique* tech, bool allowInstancing, bool allowShadows)
{

    assert(batchQueue.batchGroups_.size()>=batchQueue.batchGroupStorage_.size());
    Renderer * ren = renderer_.Get();
    if (batch.material_ == nullptr)
        batch.material_ = ren->GetDefaultMaterial();

    // Convert to instanced if possible
    if (allowInstancing && batch.geometryType_ == GEOM_STATIC && (batch.geometry_->GetIndexBuffer() != nullptr))
        batch.geometryType_ = GEOM_INSTANCED;

    if (batch.geometryType_ == GEOM_INSTANCED)
    {
        BatchGroup *grp_ptr;
        BatchGroupKey key(batch);

        BatchQueue::BatchGroupMap::iterator i = batchQueue.batchGroups_.find(key);
        if (i == batchQueue.batchGroups_.end())
        {
            // Create a new group based on the batch
            // In case the group remains below the instancing limit, do not enable instancing shaders yet
            batchQueue.batchGroupStorage_.emplace_back(batch);
            BatchGroup &newGroup(batchQueue.batchGroupStorage_.back());
            grp_ptr = &batchQueue.batchGroupStorage_.back();
            newGroup.geometryType_ = GEOM_STATIC;
            ren->SetBatchShaders(newGroup, tech, allowShadows);
            newGroup.CalculateSortKey();
            batchQueue.batchGroups_.emplace(key, batchQueue.batchGroupStorage_.size()-1);
        }
        else
            grp_ptr = &batchQueue.batchGroupStorage_[MAP_VALUE(i)];
        BatchGroup &group(*grp_ptr);
        int oldSize = group.instances_.size();
        group.AddTransforms(batch.distance_,batch.numWorldTransforms_,batch.worldTransform_,batch.instancingData_);
        // Convert to using instancing shaders when the instancing limit is reached
        if (oldSize < minInstances_ && (int)group.instances_.size() >= minInstances_)
        {
            group.geometryType_ = GEOM_INSTANCED;
            ren->SetBatchShaders(group, tech, allowShadows);
            group.CalculateSortKey();
        }
    }
    else
    {
        ren->SetBatchShaders(batch, tech, allowShadows);
        batch.CalculateSortKey();

        // If batch is static with multiple world transforms and cannot instance, we must push copies of the batch individually
        if (batch.geometryType_ == GEOM_STATIC && batch.numWorldTransforms_ > 1)
        {
            unsigned numTransforms = batch.numWorldTransforms_;
            batch.numWorldTransforms_ = 1;
            for (unsigned i = 0; i < numTransforms; ++i)
            {
                // Move the transform pointer to generate copies of the batch which only refer to 1 world transform
                batchQueue.batches_.emplace_back(batch);
                ++batch.worldTransform_;
            }
        }
        else
            batchQueue.batches_.emplace_back(batch);
    }
}

void View::PrepareInstancingBuffer()
{
    // Prepare instancing buffer from the source view
    /// \todo If rendering the same view several times back-to-back, would not need to refill the buffer
    if (sourceView_ != nullptr)
    {
        sourceView_->PrepareInstancingBuffer();
        return;
    }
    URHO3D_PROFILE(PrepareInstancingBuffer);

    unsigned totalInstances = 0;

    for (const BatchQueue &elem : batchQueueStorage_)
        totalInstances += elem.GetNumInstances();

    for (const LightBatchQueue & elem : lightQueues_)
    {
        for (const ShadowBatchQueue & split : elem.shadowSplits_)
            totalInstances += split.shadowBatches_.GetNumInstances();
        totalInstances += elem.litBaseBatches_.GetNumInstances();
        totalInstances += elem.litBatches_.GetNumInstances();
    }

    if ((totalInstances == 0u) || !renderer_->ResizeInstancingBuffer(totalInstances))
        return;

    VertexBuffer* instancingBuffer = renderer_->GetInstancingBuffer();
    unsigned freeIndex = 0;
    void* dest = instancingBuffer->Lock(0, totalInstances, true);
    if (dest == nullptr)
        return;

    const unsigned stride = instancingBuffer->GetVertexSize();
    for (BatchQueue &elem : batchQueueStorage_)
        elem.SetInstancingData(dest, stride, freeIndex);

    for (LightBatchQueue & elem : lightQueues_)
    {
        for (ShadowBatchQueue &sq : elem.shadowSplits_)
            sq.shadowBatches_.SetInstancingData(dest,stride, freeIndex);
        elem.litBaseBatches_.SetInstancingData(dest, stride, freeIndex);
        elem.litBatches_.SetInstancingData(dest, stride, freeIndex);
    }

    instancingBuffer->Unlock();
}

void View::SetupLightVolumeBatch(Batch& batch)
{
    Light* light = batch.lightQueue_->light_;
    LightType type = light->GetLightType();
    Vector3 cameraPos = camera_->GetNode()->GetWorldPosition();
    float lightDist;

    graphics_->SetBlendMode(light->IsNegative() ? BLEND_SUBTRACT : BLEND_ADD);
    graphics_->SetDepthBias(0.0f, 0.0f);
    graphics_->SetDepthWrite(false);
    graphics_->SetFillMode(FILL_SOLID);
    graphics_->SetLineAntiAlias(false);
    graphics_->SetClipPlane(false);

    if (type != LIGHT_DIRECTIONAL)
    {
        if (type == LIGHT_POINT)
            lightDist = Sphere(light->GetNode()->GetWorldPosition(), light->GetRange() * 1.25f).Distance(cameraPos);
        else
            lightDist = light->GetFrustum().Distance(cameraPos);

        // Draw front faces if not inside light volume
        if (lightDist < camera_->GetNearClip() * 2.0f)
        {
            renderer_->SetCullMode(CULL_CW, camera_);
            graphics_->SetDepthTest(CMP_GREATER);
        }
        else
        {
            renderer_->SetCullMode(CULL_CCW, camera_);
            graphics_->SetDepthTest(CMP_LESSEQUAL);
        }
    }
    else
    {
        // In case the same camera is used for multiple views with differing aspect ratios (not recommended)
        // refresh the directional light's model transform before rendering
        light->GetVolumeTransform(camera_);
        graphics_->SetCullMode(CULL_NONE);
        graphics_->SetDepthTest(CMP_ALWAYS);
    }

    graphics_->SetScissorTest(false);
    if (!noStencil_)
        graphics_->SetStencilTest(true, CMP_NOTEQUAL, OP_KEEP, OP_KEEP, OP_KEEP, 0, light->GetLightMask());
    else
        graphics_->SetStencilTest(false);
}

bool View::NeedRenderShadowMap(const LightBatchQueue& queue)
{
    // Must have a shadow map, and either forward or deferred lit batches
    return (queue.shadowMap_ != nullptr) && (!queue.litBatches_.IsEmpty() || !queue.litBaseBatches_.IsEmpty() ||
        !queue.volumeBatches_.empty());
}
void View::RenderShadowMap(const LightBatchQueue& queue)
{
    URHO3D_PROFILE(RenderShadowMap);

    Texture2D* shadowMap = queue.shadowMap_;
    graphics_->SetTexture(TU_SHADOWMAP, nullptr);

    graphics_->SetFillMode(FILL_SOLID);
    graphics_->SetClipPlane(false);
    graphics_->SetStencilTest(false);
    // Set shadow depth bias
    BiasParameters parameters = queue.light_->GetShadowBias();
    // The shadow map is a depth stencil texture
    if (shadowMap->GetUsage() == TEXTURE_DEPTHSTENCIL)
    {
        graphics_->SetColorWrite(false);
        graphics_->SetDepthStencil(shadowMap);
        graphics_->SetRenderTarget(0, shadowMap->GetRenderSurface()->GetLinkedRenderTarget());
        // Disable other render targets
        for (unsigned i = 1; i < MAX_RENDERTARGETS; ++i)
            graphics_->SetRenderTarget(i, (RenderSurface*)nullptr);
        graphics_->SetViewport(IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));
        graphics_->Clear(CLEAR_DEPTH);
    }
    else // if the shadow map is a color rendertarget
    {
        graphics_->SetColorWrite(true);
        graphics_->SetRenderTarget(0, shadowMap);
        // Disable other render targets
        for (unsigned i = 1; i < MAX_RENDERTARGETS; ++i)
            graphics_->SetRenderTarget(i, (RenderSurface*) nullptr);
        graphics_->SetDepthStencil(renderer_->GetDepthStencil(shadowMap->GetWidth(), shadowMap->GetHeight(),
            shadowMap->GetMultiSample(), shadowMap->GetAutoResolve()));
        graphics_->SetViewport(IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));
        graphics_->Clear(CLEAR_DEPTH | CLEAR_COLOR, Color::WHITE);

        parameters = BiasParameters(0.0f, 0.0f);
    }

    // Render each of the splits
    for (unsigned i = 0; i < queue.shadowSplits_.size(); ++i)
    {
        const ShadowBatchQueue& shadowQueue = queue.shadowSplits_[i];
        float multiplier = 1.0f;
        // For directional light cascade splits, adjust depth bias according to the far clip ratio of the splits
        if (i > 0 && queue.light_->GetLightType() == LIGHT_DIRECTIONAL)
        {
            multiplier = Max(shadowQueue.shadowCamera_->GetFarClip() / queue.shadowSplits_[0].shadowCamera_->GetFarClip(), 1.0f);
            multiplier = 1.0f + (multiplier - 1.0f) * queue.light_->GetShadowCascade().biasAutoAdjust_;
            // Quantize multiplier to prevent creation of too many rasterizer states on D3D11
            multiplier = (int)(multiplier * 10.0f) / 10.0f;
        }

        // Perform further modification of depth bias on OpenGL ES, as shadow calculations' precision is limited
        float addition = 0.0f;

        graphics_->SetDepthBias(multiplier * parameters.constantBias_ + addition, multiplier * parameters.slopeScaledBias_);

        if (!shadowQueue.shadowBatches_.IsEmpty())
        {
            graphics_->SetViewport(shadowQueue.shadowViewport_);
            shadowQueue.shadowBatches_.Draw(this, shadowQueue.shadowCamera_, false, false, true);
        }
    }

    // Scale filter blur amount to shadow map viewport size so that different shadow map resolutions don't behave differently
    float blurScale = queue.shadowSplits_[0].shadowViewport_.Width() / 1024.0f;
    renderer_->ApplyShadowMapFilter(this, shadowMap, blurScale);

    // reset some parameters
    graphics_->SetColorWrite(true);
    graphics_->SetDepthBias(0.0f, 0.0f);
}

RenderSurface* View::GetDepthStencil(RenderSurface* renderTarget)
{
    // If using the backbuffer, return the backbuffer depth-stencil
    if (renderTarget == nullptr)
        return nullptr;
    // Then check for linked depth-stencil
    RenderSurface* depthStencil = renderTarget->GetLinkedDepthStencil();
    // Finally get one from Renderer
    if (depthStencil == nullptr)
        depthStencil = renderer_->GetDepthStencil(renderTarget->GetWidth(), renderTarget->GetHeight(),
            renderTarget->GetMultiSample(), renderTarget->GetAutoResolve());
    return depthStencil;
}

RenderSurface* View::GetRenderSurfaceFromTexture(Texture* texture, CubeMapFace face)
{
    if (texture == nullptr)
        return nullptr;

    if (texture->GetType() == Texture2D::GetTypeStatic())
        return static_cast<Texture2D*>(texture)->GetRenderSurface();
    else if (texture->GetType() == TextureCube::GetTypeStatic())
        return static_cast<TextureCube*>(texture)->GetRenderSurface(face);
    else
        return nullptr;
}

void View::SendViewEvent(StringHash eventType)
{
    using namespace BeginViewRender;

    VariantMap& eventData = GetEventDataMap();

    eventData[P_VIEW] = this;
    eventData[P_SURFACE] = renderTarget_;
    eventData[P_TEXTURE] = (renderTarget_ != nullptr ? renderTarget_->GetParentTexture() : nullptr);
    eventData[P_SCENE] = scene_;
    eventData[P_CAMERA] = cullCamera_;

    renderer_->SendEvent(eventType, eventData);
}
Texture* View::FindNamedTexture(const QString& name, bool isRenderTarget, bool isVolumeMap)
{
    // Check rendertargets first
    StringHash nameHash(name);
    if (renderTargets_.contains(nameHash))
        return renderTargets_[nameHash];

    // Then the resource system
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    // Check existing resources first. This does not load resources, so we can afford to guess the resource type wrong
    // without having to rely on the file extension
    Texture* texture = cache->GetExistingResource<Texture2D>(name);
    if (texture == nullptr)
        texture = cache->GetExistingResource<TextureCube>(name);
    if (texture == nullptr)
        texture = cache->GetExistingResource<Texture3D>(name);
    if (texture == nullptr)
        texture = cache->GetExistingResource<Texture2DArray>(name);
    if (texture != nullptr)
        return texture;

    // If not a rendertarget (which will never be loaded from a file), finally also try to load the texture
    // This will log an error if not found; the texture binding will be cleared in that case to not constantly spam the log
    if (isRenderTarget)
        return nullptr;
    if (GetExtension(name) == ".xml")
    {
        // Assume 3D textures are only bound to the volume map unit, otherwise it's a cube texture
        StringHash type = ParseTextureTypeXml(cache, name);
        if (!type && isVolumeMap)
            type = Texture3D::GetTypeStatic();

        if (type == Texture3D::GetTypeStatic())
            return cache->GetResource<Texture3D>(name);
        if (type == Texture2DArray::GetTypeStatic())
            return cache->GetResource<Texture2DArray>(name);
        else
            return cache->GetResource<TextureCube>(name);
    }
    return cache->GetResource<Texture2D>(name);
}

}
