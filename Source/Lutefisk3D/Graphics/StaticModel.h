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
#include "Lutefisk3D/Resource/Resource.h"

namespace Urho3D
{

class Model;

/// Static model per-geometry extra data.
struct StaticModelGeometryData
{
    /// Geometry center.
    Vector3 center_;
    /// Current LOD level.
    unsigned lodLevel_;
};

/// Static model component.
class LUTEFISK3D_EXPORT StaticModel : public Drawable
{
    URHO3D_OBJECT(StaticModel,Drawable)

public:
    explicit StaticModel(Context* context);
    ~StaticModel();
    /// Register object factory. Drawable must be registered first.
    static void RegisterObject(Context* context);

    void ProcessRayQuery(const RayOctreeQuery& query, std::vector<RayQueryResult>& results) override;
    void UpdateBatches(const FrameInfo& frame) override;
    Geometry* GetLodGeometry(unsigned batchIndex, unsigned level) override;
    unsigned GetNumOccluderTriangles() override;
    bool DrawOcclusion(OcclusionBuffer* buffer) override;

    /// Set model.
    virtual void SetModel(Model* model);
    /// Set material on all geometries.
    virtual void SetMaterial(Material* material);
    /// Set material on one geometry. Return true if successful.
    virtual bool SetMaterial(unsigned index, Material* material);
    /// Set occlusion LOD level. By default (M_MAX_UNSIGNED) same as visible.
    void SetOcclusionLodLevel(unsigned level);
    /// Apply default materials from a material list file. If filename is empty (default), the model's resource name with extension .txt will be used.
    void ApplyMaterialList(const QString& fileName = QString());

    /// Return model.
    Model* GetModel() const { return model_; }
    /// Return number of geometries.
    size_t GetNumGeometries() const { return geometries_.size(); }
    /// Return material by geometry index.
    virtual Material* GetMaterial(unsigned index = 0) const;
    /// Return occlusion LOD level.
    unsigned GetOcclusionLodLevel() const { return occlusionLodLevel_; }
    /// Determines if the given world space point is within the model geometry.
    bool IsInside(const Vector3& point) const;
    /// Determines if the given local space point is within the model geometry.
    bool IsInsideLocal(const Vector3& point) const;

    /// Set model attribute.
    void SetModelAttr(const ResourceRef& value);
    /// Set materials attribute.
    void SetMaterialsAttr(const ResourceRefList& value);
    /// Return model attribute.
    ResourceRef GetModelAttr() const;
    /// Return materials attribute.
    const ResourceRefList& GetMaterialsAttr() const;

protected:
    /// Recalculate the world-space bounding box.
    void OnWorldBoundingBoxUpdate() override;
    void SetBoundingBox(const BoundingBox& box);
    void SetNumGeometries(unsigned num);
    void ResetLodLevels();
    void CalculateLodLevels();

    /// Extra per-geometry data.
    std::vector<StaticModelGeometryData> geometryData_;
    /// All geometries.
    std::vector<std::vector<SharedPtr<Geometry> > > geometries_;
    /// Model.
    SharedPtr<Model> model_;
    /// Occlusion LOD level.
    unsigned occlusionLodLevel_;
    /// Material list attribute.
    mutable ResourceRefList materialsAttr_;

private:
    /// Handle model reload finished.
    void HandleModelReloadFinished();
};

}
