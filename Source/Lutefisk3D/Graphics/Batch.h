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

#include "Lutefisk3D/Graphics/Drawable.h"
#include "Lutefisk3D/Graphics/Material.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Container/Ptr.h"

#include <stdint.h>

namespace Urho3D
{

class Camera;
class Drawable;
class Geometry;
class Light;
class Material;
class Matrix3x4;
class Pass;
class ShaderVariation;
class Texture2D;
class VertexBuffer;
class View;
class Zone;
struct SourceBatch;
struct LightBatchQueue;

/// Queued 3D geometry draw call.
struct LUTEFISK3D_EXPORT Batch
{
    /// Construct with defaults.
    Batch() :
        geometry_(nullptr),
        lightQueue_(nullptr),
        isBase_(false)
    {
    }

    /// Construct from a drawable's source batch.
    Batch(const SourceBatch& rhs,bool is_base=false) :
        distance_(rhs.distance_),
        geometry_(rhs.geometry_),
        material_(rhs.material_),
        worldTransform_(rhs.worldTransform_),
        numWorldTransforms_(rhs.numWorldTransforms_),
        instancingData_(rhs.instancingData_),
        lightQueue_(nullptr),
        geometryType_(rhs.geometryType_),
        renderOrder_(rhs.material_ ? rhs.material_->GetRenderOrder() : DEFAULT_RENDER_ORDER),
        isBase_(is_base)
    {
    }
    Batch(const SourceBatch& rhs, Zone *z, LightBatchQueue *l, Pass *p,
          uint8_t lmask=uint8_t(DEFAULT_LIGHTMASK),bool is_base=false) :
        sortKey_(0),
        distance_(rhs.distance_),
        geometry_(rhs.geometry_),
        material_(rhs.material_),
        worldTransform_(rhs.worldTransform_),
        numWorldTransforms_(rhs.numWorldTransforms_),
        zone_(z),
        lightQueue_(l),
        pass_(p),
        vertexShader_(nullptr),
        pixelShader_(nullptr),
        geometryType_(rhs.geometryType_),
        renderOrder_(rhs.material_ ? rhs.material_->GetRenderOrder() : DEFAULT_RENDER_ORDER),
        lightMask_(lmask),
        isBase_(is_base)
    {
    }

    /// Calculate state sorting key, which consists of base pass flag, light, pass and geometry.
    void CalculateSortKey();
    /// Prepare for rendering.
    void Prepare(View* view, const Camera *camera, bool setModelTransform, bool allowDepthWrite) const;
    /// Prepare and draw.
    void Draw(View* view, Camera* camera, bool allowDepthWrite) const;

    /// State sorting key.
    uint64_t sortKey_;
    /// Distance from camera.
    float distance_;
    /// Geometry.
    Geometry* geometry_;
    /// Material.
    Material* material_;
    /// World transform(s). For a skinned model, these are the bone transforms.
    const Matrix3x4* worldTransform_;
    /// Number of world transforms.
    unsigned numWorldTransforms_;
    /// Per-instance data. If not null, must contain enough data to fill instancing buffer.
    void* instancingData_;
    /// Zone.
    Zone* zone_;
    /// Light properties.
    LightBatchQueue* lightQueue_;
    /// Material pass.
    Pass* pass_;
    /// Vertex shader.
    ShaderVariation* vertexShader_;
    /// Pixel shader.
    ShaderVariation* pixelShader_;
    /// %Geometry type.
    GeometryType geometryType_;
    /// 8-bit render order modifier from material.
    uint8_t renderOrder_;
    /// 8-bit light mask for stencil marking in deferred rendering.
    uint8_t lightMask_;
    /// Base batch flag. This tells to draw the object fully without light optimizations.
    bool isBase_;
};

/// Data for one geometry instance.
struct InstanceData
{
    /// Construct undefined.
    InstanceData() = default;

    /// Construct with transform, instancing data and distance.
    constexpr InstanceData(const Matrix3x4* worldTransform, const void* instancingData, float distance) :
        worldTransform_(worldTransform),
        instancingData_(instancingData),
        distance_(distance)
    {
    }
    /// World transform.
    const Matrix3x4* worldTransform_;
    /// Instancing data buffer.
    const void* instancingData_;
    /// Distance from camera.
    float distance_;
};

/// Instanced 3D geometry draw call.
struct BatchGroup : public Batch
{
    /// Construct with defaults.
    BatchGroup() = default;

    /// Construct from a batch.
    BatchGroup(const Batch &batch) :
        Batch(batch),
        startIndex_(M_MAX_UNSIGNED)
    {
    }

    /// Destruct.
    ~BatchGroup() = default;

    /// Add world transform(s) from a batch.
    void AddTransforms(float distance,unsigned int numTransforms,const Matrix3x4 *transforms,void *instanceData)
    {
        for (unsigned i = 0; i < numTransforms; ++i)
        {
            instances_.emplace_back(transforms + i,instanceData,distance);
        }
    }

    /// Pre-set the instance data. Buffer must be big enough to hold all data.
    void SetInstancingData(void* lockedData, unsigned stride, unsigned& freeIndex);
    /// Prepare and draw.
    void Draw(View* view, Camera* camera, bool allowDepthWrite) const;

    /// Instance data.
    PODVectorN<InstanceData,32> instances_;
    /// Instance stream start index, or M_MAX_UNSIGNED if transforms not pre-set.
    unsigned startIndex_=M_MAX_UNSIGNED;
};

/// Instanced draw call grouping key.
struct BatchGroupKey
{
    /// Construct undefined.
    BatchGroupKey() = default;

    /// Construct from a batch.
    BatchGroupKey(const Batch &batch)
        : zone_(batch.zone_),
          lightQueue_(batch.lightQueue_),
          pass_(batch.pass_),
          material_(batch.material_),
          geometry_(batch.geometry_),
          renderOrder_(batch.renderOrder_)
    {
    }
    /// Test for equality with another batch group key.
    constexpr bool operator==(const BatchGroupKey &rhs) const
    {
        return zone_ == rhs.zone_ && lightQueue_ == rhs.lightQueue_ && pass_ == rhs.pass_ &&
               material_ == rhs.material_ && geometry_ == rhs.geometry_ && renderOrder_ == rhs.renderOrder_;
    }
    /// Test for inequality with another batch group key.
    constexpr bool operator!=(const BatchGroupKey &rhs) const { return !(*this == rhs); }

    /// Return hash value.
    unsigned ToHash() const
    {
        return (uintptr_t(pass_) >> 1) ^ (uintptr_t(material_) >> 3) ^ (uintptr_t(geometry_) >> 5) ^
               (uintptr_t(zone_) >> 7) ^ (uintptr_t(lightQueue_) >> 9) ^ renderOrder_;
    }

private:
    /// Zone.
    Zone *zone_;
    /// Light properties.
    LightBatchQueue *lightQueue_;
    /// Material pass.
    Pass *pass_;
    /// Material.
    Material *material_;
    /// Geometry.
    Geometry *geometry_;
    /// 8-bit render order modifier from material.
    uint8_t renderOrder_;
};

}
namespace std {
template<> struct hash<Urho3D::BatchGroupKey> {
    inline size_t operator()(const Urho3D::BatchGroupKey & key) const
    {
        return key.ToHash();
    }
};
}

namespace Urho3D {

/// Queue that contains both instanced and non-instanced draw calls.
struct LUTEFISK3D_EXPORT BatchQueue
{
public:
    typedef FasterHashMap<BatchGroupKey, uint32_t> BatchGroupMap; //FasterHashMap<BatchGroupKey, int>
    /// Clear for new frame by clearing all groups and batches.
    void Clear(int maxSortedInstances);
    /// Sort non-instanced draw calls back to front.
    void SortBackToFront();
    /// Sort instanced and non-instanced draw calls front to back.
    void SortFrontToBack();
    /// Sort batches front to back while also maintaining state sorting.
    void SortFrontToBack2Pass(std::vector<Batch*>& batches);
    /// Pre-set instance data of all groups. The vertex buffer must be big enough to hold all data.
    void SetInstancingData(void* lockedData, unsigned stride, unsigned& freeIndex);
    /// Draw.
    void Draw(View* view, Camera* camera, bool markToStencil, bool usingLightOptimization, bool allowDepthWrite) const;
    /// Return the combined amount of instances.
    unsigned GetNumInstances() const;
    /// Return whether the batch group is empty.
    bool IsEmpty() const { return batches_.empty() && batchGroupStorage_.empty(); }

    /// Instanced draw calls.
    std::vector<BatchGroup> batchGroupStorage_;
    BatchGroupMap batchGroups_;
    /// Shader remapping table for 2-pass state and distance sort.
    HashMap<unsigned, unsigned> shaderRemapping_;
    /// Material remapping table for 2-pass state and distance sort.
    HashMap<unsigned short, unsigned short> materialRemapping_;
    /// Geometry remapping table for 2-pass state and distance sort.
    HashMap<unsigned short, unsigned short> geometryRemapping_;

    /// Unsorted non-instanced draw calls.
    std::vector<Batch> batches_;
    /// Sorted non-instanced draw calls.
    std::vector<Batch*> sortedBatches_;
    /// Sorted instanced draw calls.
    std::vector<BatchGroup*> sortedBatchGroups_;
    /// Maximum sorted instances.
    unsigned maxSortedInstances_;
    /// Whether the pass command contains extra shader defines.
    bool hasExtraDefines_;
    /// Vertex shader extra defines.
    QString vsExtraDefines_;
    /// Pixel shader extra defines.
    QString psExtraDefines_;
    /// Hash for vertex shader extra defines.
    StringHash vsExtraDefinesHash_;
    /// Hash for pixel shader extra defines.
    StringHash psExtraDefinesHash_;
};

/// Queue for shadow map draw calls
struct ShadowBatchQueue
{
    /// Shadow map camera.
    Camera* shadowCamera_;
    /// Shadow map viewport.
    IntRect shadowViewport_;
    /// Shadow caster draw calls.
    BatchQueue shadowBatches_;
    /// Directional light cascade near split distance.
    float nearSplit_;
    /// Directional light cascade far split distance.
    float farSplit_;
};

/// Queue for light related draw calls.
struct LightBatchQueue
{
    /// Per-pixel light.
    Light* light_;
    /// Light negative flag.
    bool negative_;
    /// Shadow map depth texture.
    Texture2D* shadowMap_;
    /// Lit geometry draw calls, base (replace blend mode)
    BatchQueue litBaseBatches_;
    /// Lit geometry draw calls, non-base (additive)
    BatchQueue litBatches_;
    /// Shadow map split queues.
    std::vector<ShadowBatchQueue> shadowSplits_;
    /// Per-vertex lights.
    std::vector<Light*> vertexLights_;
    /// Light volume draw calls.
    std::vector<Batch> volumeBatches_;
};

inline unsigned int qHash(const Urho3D::BatchGroupKey & key)
{
    return key.ToHash();
}
}
