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

#pragma once

#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Graphics/Batch.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Graphics/Zone.h"
#include <array>
#include <deque>
#include <vector>
namespace Urho3D
{

class Camera;
class DebugRenderer;
class Light;
class Drawable;
class Graphics;
class OcclusionBuffer;
class Octree;
class Renderer;
class RenderPath;
class RenderSurface;
class Technique;
class Texture;
class Texture2D;
class Viewport;
class Zone;
struct RenderPathCommand;
struct WorkItem;

struct LightQueryShadowEntry {
    /// Shadow cameras.
    Camera* shadowCameras_;
    /// Shadow caster start indices.
    unsigned shadowCasterBegin_;
    /// Shadow caster end indices.
    unsigned shadowCasterEnd_;
    /// Combined bounding box of shadow casters in light projection space. Only used for focused spot lights.
    BoundingBox shadowCasterBox_;
    /// Shadow camera near splits (directional lights only.)
    float shadowNearSplits_;
    /// Shadow camera far splits (directional lights only.)
    float shadowFarSplits_;

};

/// Intermediate light processing result.
struct LightQueryResult
{
    /// Light.
    Light* light_;
    /// Lit geometries.
    std::vector<Drawable*> litGeometries_;
    /// Shadow casters.
    std::vector<Drawable*> shadowCasters_;

    std::array<LightQueryShadowEntry,MAX_LIGHT_SPLITS> shadowEntries_;

    /// Shadow map split count.
    unsigned numSplits_;
};

static const unsigned MAX_VIEWPORT_TEXTURES = 2;
class ViewPrivate;
/// Internal structure for 3D rendering work. Created for each backbuffer and texture viewport, but not for shadow cameras.
class LUTEFISK3D_EXPORT View : public RefCounted
{
    friend void CheckVisibilityWork(const WorkItem* item, unsigned threadIndex);
    friend void ProcessLightWork(const WorkItem* item, unsigned threadIndex);
public:

    View(Context* context);
    virtual ~View();

    /// Define with rendertarget and viewport. Return true if successful.
    bool Define(RenderSurface* renderTarget, Viewport* viewport);
    /// Update and cull objects and construct rendering batches.
    void Update(const FrameInfo& frame);
    /// Render batches.
    void Render();

    /// Return graphics subsystem.
    Graphics* GetGraphics() const { return graphics_; }
    /// Return renderer subsystem.
    Renderer* GetRenderer() const { return renderer_; }
    /// Return scene.
    Scene* GetScene() const { return scene_; }
    /// Return octree.
    Octree* GetOctree() const { return octree_; }
    /// Return viewport camera.
    Camera* GetCamera() const { return camera_; }
    /// Return culling camera. Normally same as the viewport camera.
    Camera* GetCullCamera() const { return cullCamera_; }
    /// Return information of the frame being rendered.
    const FrameInfo& GetFrameInfo() const { return frame_; }
    /// Return the rendertarget. 0 if using the backbuffer.
    RenderSurface* GetRenderTarget() const { return renderTarget_; }
    /// Return whether should draw debug geometry.
    bool GetDrawDebug() const { return drawDebug_; }
    /// Return view rectangle.
    const IntRect& GetViewRect() const { return viewRect_; }

    /// Return view dimensions.
    const IntVector2& GetViewSize() const { return viewSize_; }
    /// Return geometry objects.
    const std::vector<Drawable*>& GetGeometries() const { return geometries_; }
    /// Return occluder objects.
    const std::vector<Drawable*>& GetOccluders() const { return occluders_; }
    /// Return lights.
    const std::vector<Light*>& GetLights() const { return lights_; }
    /// Return light batch queues.
    const std::vector<LightBatchQueue>& GetLightQueues() const { return lightQueues_; }
    /// Return the last used software occlusion buffer.
    OcclusionBuffer* GetOcclusionBuffer() const { return occlusionBuffer_; }
    /// Return number of occluders that were actually rendered. Occluders may be rejected if running out of triangles or if behind other occluders.
    unsigned GetNumActiveOccluders() const { return activeOccluders_; }

    /// Return the source view that was already prepared. Used when viewports specify the same culling camera.
    View* GetSourceView() const { return sourceView_; }
    /// Set global (per-frame) shader parameters. Called by Batch and internally by View.
    void SetGlobalShaderParameters();
    /// Set camera-specific shader parameters. Called by Batch and internally by View.
    void SetCameraShaderParameters(const Urho3D::Camera &camera);
    /// Set command's shader parameters if any. Called internally by View.
    void SetCommandShaderParameters(const RenderPathCommand& command);
    /// Set G-buffer offset and inverse size shader parameters. Called by Batch and internally by View.
    void SetGBufferShaderParameters(const IntVector2& texSize, const IntRect& viewRect);

    /// Draw a fullscreen quad. Shaders and renderstates must have been set beforehand. Quad will be drawn to the middle of depth range, similarly to deferred directional lights.
    void DrawFullscreenQuad(bool setIdentityProjection = false);
    /// Get a named texture from the rendertarget list or from the resource cache, to be either used as a rendertarget or texture binding.
    Texture* FindNamedTexture(const QString& name, bool isRenderTarget, bool isVolumeMap = false);

private:
    /// Query the octree for drawable objects.
    void GetDrawables();
    /// Construct batches from the drawable objects.
    void GetBatches();
    /// Get lit geometries and shadowcasters for visible lights.
    void ProcessLights();
    /// Get batches from lit geometries and shadowcasters.
    void GetLightBatches(Technique *default_tech);
    /// Get unlit batches.
    void GetBaseBatches(Technique *default_tech);
    /// Update geometries and sort batches.
    void UpdateGeometries();
    /// Get pixel lit batches for a certain light and drawable.
    void GetLitBatches(Drawable *drawable, Zone *zone, LightBatchQueue &lightQueue, BatchQueue *availableQueues[], Technique *default_tech);
    /// Execute render commands.
    void ExecuteRenderPathCommands();
    /// Set rendertargets for current render command.
    void SetRenderTargets(RenderPathCommand& command);
    /// Set textures for current render command. Return whether depth write is allowed (depth-stencil not bound as a texture.)
    bool SetTextures(RenderPathCommand& command);
    /// Perform a quad rendering command.
    void RenderQuad(RenderPathCommand& command);
    /// Check if a command is enabled and has content to render. To be called only after render update has completed for the frame.
    bool IsNecessary(const RenderPathCommand& command);
    /// Check if a command reads the destination render target.
    bool CheckViewportRead(const RenderPathCommand& command);
    /// Check if a command writes into the destination render target.
    bool CheckViewportWrite(const RenderPathCommand& command);
    /// Check whether a command should use pingponging instead of resolve from destination render target to viewport texture.
    bool CheckPingpong(unsigned index);
    /// Allocate needed screen buffers.
    void AllocateScreenBuffers();
    /// Blit the viewport from one surface to another.
    void BlitFramebuffer(Texture* source, RenderSurface* destination, bool depthWrite);
    /// Query for occluders as seen from a camera.
    void UpdateOccluders(std::vector<Drawable*>& occluders, Camera* camera);
    /// Draw occluders to occlusion buffer.
    void DrawOccluders(OcclusionBuffer* buffer, const std::vector<Drawable*>& occluders);
    /// Query for lit geometries and shadow casters for a light.
    void ProcessLight(LightQueryResult& query, unsigned threadIndex);
    /// Process shadow casters' visibilities and build their combined view- or projection-space bounding box.
    void ProcessShadowCasters(LightQueryResult& query, const std::vector<Drawable*>& drawables, LightQueryShadowEntry &entry);
    /// Set up initial shadow camera view(s).
    void SetupShadowCameras(LightQueryResult& query);
    /// Set up a directional light shadow camera
    void SetupDirLightShadowCamera(Camera* shadowCamera, Light* light, float nearSplit, float farSplit);
    /// Finalize shadow camera view after shadow casters and the shadow map are known.
    void FinalizeShadowCamera(Camera* shadowCamera, Light* light, const IntRect& shadowViewport, const BoundingBox& shadowCasterBox);
    /// Quantize a directional light shadow camera view to eliminate swimming.
    void QuantizeDirLightShadowCamera(Camera* shadowCamera, const FocusParameters &shadowFocusParameters, const IntRect& shadowViewport, const BoundingBox& viewBox);
    /// Check visibility of one shadow caster.
    bool IsShadowCasterVisible(Drawable* drawable, BoundingBox lightViewBox, Camera* shadowCamera, const Matrix3x4& lightView, const Frustum& lightViewFrustum, const BoundingBox& lightViewFrustumBox);
    /// Return the viewport for a shadow map split.
    IntRect GetShadowMapViewport(Light* light, unsigned splitIndex, Texture2D* shadowMap);
    /// Find and set a new zone for a drawable when it has moved.
    void FindZone(Drawable* drawable);
    /// Return material technique, considering the drawable's LOD distance.
    Technique* GetTechnique(Drawable* drawable, Material* material);
    /// Check if material should render an auxiliary view (if it has a camera attached.)
    void CheckMaterialForAuxView(Material* material);
    /// Set shader defines for a batch queue if used.
    void SetQueueShaderDefines(BatchQueue& queue, const RenderPathCommand& command);
    /// Choose shaders for a batch and add it to queue.
    void AddBatchToQueue(BatchQueue& batchQueue, Batch batch, const Technique* tech, bool allowInstancing = true, bool allowShadows = true);
    /// Prepare instancing buffer by filling it with all instance transforms.
    void PrepareInstancingBuffer();
    /// Set up a light volume rendering batch.
    void SetupLightVolumeBatch(Batch& batch);
    /// Check whether a light queue needs shadow rendering.
    bool NeedRenderShadowMap(const LightBatchQueue& queue);
    /// Render a shadow map.
    void RenderShadowMap(const LightBatchQueue& queue);
    /// Return the proper depth-stencil surface to use for a rendertarget.
    RenderSurface* GetDepthStencil(RenderSurface* renderTarget);
    /// Helper function to get the render surface from a texture. 2D textures will always return the first face only.
    RenderSurface* GetRenderSurfaceFromTexture(Texture* texture, CubeMapFace face = FACE_POSITIVE_X);
    /// Send a view update or render related event through the Renderer subsystem. The parameters are the same for all of them.
    void SendViewEvent(jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> &eventType);
    /// Return the drawable's zone, or camera zone if it has override mode enabled.
    Zone* GetZone(Drawable* drawable)
    {
        if (cameraZoneOverride_)
            return cameraZone_;
        Zone* drawableZone = drawable->GetZone();
        return drawableZone ? drawableZone : cameraZone_;
    }

    /// Return the drawable's light mask, considering also its zone.
    unsigned GetLightMask(Drawable* drawable)
    {
        return drawable->GetLightMask() & GetZone(drawable)->GetLightMask();
    }

    /// Return the drawable's shadow mask, considering also its zone.
    unsigned GetShadowMask(Drawable* drawable)
    {
        return drawable->GetShadowMask() & GetZone(drawable)->GetShadowMask();
    }

    /// Return hash code for a vertex light queue.
    uint64_t GetVertexLightQueueHash(const std::vector<Light*>& vertexLights)
    {
        uint64_t hash = 0;
        for (Light * light : vertexLights)
            hash += uintptr_t(light);
        return hash;
    }
    Context *context_;
    std::unique_ptr<ViewPrivate> d;
    /// Graphics subsystem.
    Graphics *graphics_; // non-owning pointer
    Renderer *renderer_; //!< Renderer subsystem.
    Scene* scene_;       //!< Scene to use.
    Octree* octree_; //!< Octree to use.
    Camera* camera_; //!< Viewport (rendering) camera.
    /// Culling camera. Usually same as the viewport camera.
    Camera* cullCamera_;
    /// Shared source view. Null if this view is using its own culling.
    WeakPtr<View> sourceView_;
    /// Zone the camera is inside, or default zone if not assigned.
    Zone* cameraZone_;
    /// Zone at far clip plane.
    Zone* farClipZone_;
    /// Occlusion buffer for the main camera.
    OcclusionBuffer* occlusionBuffer_;
    /// Destination color rendertarget.
    RenderSurface* renderTarget_;
    /// Substitute rendertarget for deferred rendering. Allocated if necessary.
    RenderSurface* substituteRenderTarget_;
    /// Texture(s) for sampling the viewport contents. Allocated if necessary.
    Texture* viewportTextures_[MAX_VIEWPORT_TEXTURES];
    /// Color rendertarget active for the current renderpath command.
    RenderSurface* currentRenderTarget_;
    /// Last used custom depth render surface.
    RenderSurface* lastCustomDepthSurface_;
    /// Texture containing the latest viewport texture.
    Texture* currentViewportTexture_;
    /// Dummy texture for D3D9 depth only rendering.
    Texture* depthOnlyDummyTexture_;
    /// Viewport rectangle.
    IntRect viewRect_;
    /// Viewport size.
    IntVector2 viewSize_;
    /// Destination rendertarget size.
    IntVector2 rtSize_;
    /// Information of the frame being rendered.
    FrameInfo frame_;
    /// View aspect ratio.
    float aspectRatio_;
    /// Minimum Z value of the visible scene.
    float minZ_;
    /// Maximum Z value of the visible scene.
    float maxZ_;
    /// Material quality level.
    int materialQuality_;
    /// Maximum number of occluder triangles.
    int maxOccluderTriangles_;
    /// Minimum number of instances required in a batch group to render as instanced.
    int minInstances_;
    /// Highest zone priority currently visible.
    int highestZonePriority_;
    /// Geometries updated flag.
    bool geometriesUpdated_;
    /// Camera zone's override flag.
    bool cameraZoneOverride_;
    /// Draw shadows flag.
    bool drawShadows_;
    /// Deferred flag. Inferred from the existence of a light volume command in the renderpath.
    bool deferred_;
    /// Deferred ambient pass flag. This means that the destination rendertarget is being written to at the same time as albedo/normal/depth buffers, and needs to be RGBA on OpenGL.
    bool deferredAmbient_;
    /// Forward light base pass optimization flag. If in use, combine the base pass and first light for all opaque objects.
    bool useLitBase_;
    /// Has scene passes flag. If no scene passes, view can be defined without a valid scene or camera to only perform quad rendering.
    bool hasScenePasses_;
    /// Whether is using a custom readable depth texture without a stencil channel.
    bool noStencil_;
    /// Draw debug geometry flag. Copied from the viewport.
    bool drawDebug_;
    /// Renderpath.
    RenderPath* renderPath_;
    /// Visible geometry objects.
    std::vector<Drawable*> geometries_;
    /// Occluder objects.
    std::vector<Drawable*> occluders_;
    /// Lights.
    std::vector<Light*> lights_;
    /// Number of active occluders.
    unsigned activeOccluders_;
    /// Per-pixel light queues.
    std::vector<LightBatchQueue> lightQueues_;
    int alphaPassQueueIdx_;
    /// Index of the GBuffer pass.
    unsigned gBufferPassIndex_;
    /// Index of the opaque forward base pass.
    unsigned basePassIndex_;
    /// Index of the alpha pass.
    unsigned alphaPassIndex_;
    /// Index of the forward light pass.
    unsigned lightPassIndex_;
    /// Index of the litbase pass.
    unsigned litBasePassIndex_;
    /// Index of the litalpha pass.
    unsigned litAlphaPassIndex_;
    /// Pointer to the light volume command if any.
    const RenderPathCommand* lightVolumeCommand_;
    /// Pointer to the forwardlights command if any.
    const RenderPathCommand* forwardLightsCommand_;
    /// Pointer to the current commmand if it contains shader parameters to be set for a render pass.
    const RenderPathCommand* passCommand_;
    /// Flag for scene being resolved from the backbuffer.
    bool usedResolve_;
};

}
