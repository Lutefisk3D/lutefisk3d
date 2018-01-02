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

#include "Lutefisk3D/Graphics/Drawable.h"
#include "Lutefisk3D/Math/Frustum.h"
#include "Lutefisk3D/Graphics/Skeleton.h"

#include <deque>

namespace Urho3D
{

class IndexBuffer;
class VertexBuffer;

/// %Decal vertex.
struct DecalVertex
{
    DecalVertex() = default;

    /// Construct with position and normal.
    DecalVertex(const Vector3& position, const Vector3& normal) :
        position_(position),
        normal_(normal)
    {
    }

    /// Construct with position, normal and skinning information.
    DecalVertex(const Vector3& position, const Vector3& normal, const float* blendWeights, const uint8_t* blendIndices) :
        position_(position),
        normal_(normal)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            blendWeights_[i] = blendWeights[i];
            blendIndices_[i] = blendIndices[i];
        }
    }

    Vector3 position_;        //!< Position.
    Vector3 normal_;          //!< Normal.
    Vector2 texCoord_;        //!< Texture coordinates.
    Vector4 tangent_;         //!< Tangent.
    float   blendWeights_[4]; //!< Blend weights.
    uint8_t blendIndices_[4]; //!< Blend indices.
};

/// One decal in a decal set.
struct Decal
{

    /// Add a vertex.
    void AddVertex(const DecalVertex& vertex);
    /// Calculate local-space bounding box.
    void CalculateBoundingBox();

    float                       timer_      = 0; //!< Decal age timer.
    float                       timeToLive_ = 0; //!< Maximum time to live in seconds (0 = infinite)
    BoundingBox                 boundingBox_;    //!< Local-space bounding box.
    std::vector<DecalVertex>    vertices_;       //!< Decal vertices.
    std::vector<unsigned short> indices_;        //!</ Decal indices.
};

/// %Decal renderer component.
class LUTEFISK3D_EXPORT DecalSet : public Drawable
{
    URHO3D_OBJECT(DecalSet,Drawable)

public:
    DecalSet(Context* context);
    virtual ~DecalSet();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Apply attribute changes that can not be applied immediately. Called after scene load or a network update.
    void ApplyAttributes() override;
    /// Handle enabled/disabled state change.
    void OnSetEnabled() override;
    /// Process octree raycast. May be called from a worker thread.
    void ProcessRayQuery(const RayOctreeQuery& query, std::vector<RayQueryResult>& results) override;
    /// Calculate distance and prepare batches for rendering. May be called from worker thread(s), possibly re-entrantly.
    void UpdateBatches(const FrameInfo& frame) override;
    /// Prepare geometry for rendering. Called from a worker thread if possible (no GPU update.)
    void UpdateGeometry(const FrameInfo& frame) override;
    /// Return whether a geometry update is necessary, and if it can happen in a worker thread.
    UpdateGeometryType GetUpdateGeometryType() override;

    /// Set material. The material should use a small negative depth bias to avoid Z-fighting.
    void SetMaterial(Material* material);
    /// Set maximum number of decal vertices.
    void SetMaxVertices(unsigned num);
    /// Set maximum number of decal vertex indices.
    void SetMaxIndices(unsigned num);
    /// Set whether to optimize GPU buffer sizes according to current amount of decals. Default false, which will size the buffers according to the maximum vertices/indices. When true, buffers will be reallocated whenever decals are added/removed, which can be worse for performance.
    void SetOptimizeBufferSize(bool enable);
    /// Add a decal at world coordinates, using a target drawable's geometry for reference. If the decal needs to move with the target, the decal component should be created to the target's node. Return true if successful.
    bool AddDecal(Drawable* target, const Vector3& worldPosition, const Quaternion& worldRotation, float size, float aspectRatio, float depth, const Vector2& topLeftUV, const Vector2& bottomRightUV, float timeToLive = 0.0f, float normalCutoff = 0.1f, unsigned subGeometry = M_MAX_UNSIGNED);
    /// Remove n oldest decals.
    void RemoveDecals(unsigned num);
    /// Remove all decals.
    void RemoveAllDecals();

    /// Return material.
    Material* GetMaterial() const;
    /// Return number of decals.
    size_t GetNumDecals() const { return decals_.size(); }
    /// Retur number of vertices in the decals.
    unsigned GetNumVertices() const { return numVertices_; }
    /// Retur number of vertex indices in the decals.
    unsigned GetNumIndices() const { return numIndices_; }
    /// Return maximum number of decal vertices.
    unsigned GetMaxVertices() const { return maxVertices_; }
    /// Return maximum number of decal vertex indices.
    unsigned GetMaxIndices() const { return maxIndices_; }

    /// Return whether is optimizing GPU buffer sizes according to current amount of decals.
    bool GetOptimizeBufferSize() const { return optimizeBufferSize_; }
    /// Set material attribute.
    void SetMaterialAttr(const ResourceRef& value);
    /// Set decals attribute.
    void SetDecalsAttr(const std::vector<unsigned char>& value);
    /// Return material attribute.
    ResourceRef GetMaterialAttr() const;
    /// Return decals attribute.
    std::vector<unsigned char> GetDecalsAttr() const;

protected:
    /// Recalculate the world-space bounding box.
    void OnWorldBoundingBoxUpdate() override;
    /// Handle node transform being dirtied.
    void OnMarkedDirty(Node* node) override;

private:
    /// Get triangle faces from the target geometry.
    void GetFaces(std::vector<std::vector<DecalVertex> >& faces, Drawable* target, unsigned batchIndex, const Frustum& frustum, const Vector3& decalNormal, float normalCutoff);
    /// Get triangle face from the target geometry.
    void GetFace(std::vector<std::vector<DecalVertex> >& faces, Drawable* target, unsigned batchIndex, unsigned i0, unsigned i1, unsigned i2, const unsigned char* positionData, const unsigned char* normalData, const unsigned char* skinningData, unsigned positionStride, unsigned normalStride, unsigned skinningStride, const Frustum& frustum, const Vector3& decalNormal, float normalCutoff);
    /// Get bones referenced by skinning data and remap the skinning indices. Return true if successful.
    bool GetBones(Drawable* target, unsigned batchIndex, const float* blendWeights, const unsigned char* blendIndices, unsigned char* newBlendIndices);
    /// Calculate UV coordinates for the decal.
    void CalculateUVs(Decal& decal, const Matrix3x4& view, const Matrix4& projection, const Vector2& topLeftUV, const Vector2& bottomRightUV);
    /// Transform decal's vertices from the target geometry to the decal set local space.
    void TransformVertices(Decal& decal, const Matrix3x4& transform);
    /// Remove a decal by iterator and return iterator to the next decal.
    std::deque<Decal>::iterator RemoveDecal(std::deque<Decal>::iterator i);
    /// Mark decals and the bounding box dirty.
    void MarkDecalsDirty();
    /// Recalculate the local-space bounding box.
    void CalculateBoundingBox();
    /// Rewrite decal vertex and index buffers.
    void UpdateBuffers();
    /// Recalculate skinning.
    void UpdateSkinning();
    /// Update the batch (geometry type, shader data.)
    void UpdateBatch();
    /// Find bones after loading.
    void AssignBoneNodes();
    /// Subscribe/unsubscribe from scene post-update as necessary.
    void UpdateEventSubscription(bool checkAllDecals);
    /// Handle scene post-update event.
    void HandleScenePostUpdate(Scene *, float ts);

    SharedPtr<Geometry> geometry_;
    SharedPtr<VertexBuffer> vertexBuffer_;
    SharedPtr<IndexBuffer> indexBuffer_;
    std::deque<Decal> decals_;
    std::vector<Bone>       bones_;              //!< Bones used for skinned decals.
    std::vector<Matrix3x4>  skinMatrices_;       //!< Skinning matrices.
    unsigned                numVertices_;        //!< Vertices in the current decals.
    unsigned                numIndices_;         //!< Indices in the current decals.
    unsigned                maxVertices_;        //!< Maximum vertices.
    unsigned                maxIndices_;         //!< Maximum indices.
    bool                    optimizeBufferSize_; //!< Optimize buffer sizes flag.
    bool                    skinned_;            //!< Skinned mode flag.
    bool                    bufferDirty_;        //!< Vertex buffer needs rewrite / resizing flag.
    bool                    boundingBoxDirty_;   //!<Bounding box needs update flag.
    bool                    skinningDirty_;      //!< Skinning dirty flag.
    bool                    assignBonesPending_; //!< Bone nodes assignment pending flag.
    bool                    subscribed_;         //!< Subscribed to scene post update event flag.
};

}
