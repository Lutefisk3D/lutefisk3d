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

#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Container/DataHandle.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Graphics/Skeleton.h"
#include "Lutefisk3D/Math/BoundingBox.h"
#include "Lutefisk3D/Resource/Resource.h"

namespace Urho3D
{

class Geometry;
class IndexBuffer;
class Graphics;
class VertexBuffer;
using VertexBufferHandle = DataHandle<VertexBuffer,20,20>;
using IndexBufferHandle = DataHandle<IndexBuffer,20,20>;
/// Vertex buffer morph data.
struct VertexBufferMorph
{
    /// Vertex elements.
    unsigned elementMask_;
    /// Number of vertices.
    unsigned vertexCount_;
    /// Morphed vertices data size as bytes.
    unsigned dataSize_;
    /// Morphed vertices. Stored packed as <index, data> pairs.
    SharedArrayPtr<unsigned char> morphData_;
};

/// Definition of a model's vertex morph.
struct ModelMorph
{
    /// Morph name.
    QString name_;
    /// Morph name hash.
    StringHash nameHash_;
    /// Current morph weight.
    float weight_;
    /// Morph data per vertex buffer.
    HashMap<unsigned, VertexBufferMorph> buffers_;
};

/// Description of vertex buffer data for asynchronous loading.
struct VertexBufferDesc
{
    /// Vertex count.
    unsigned vertexCount_;
    /// Vertex declaration.
    std::vector<VertexElement> vertexElements_;
    /// Vertex data size.
    unsigned dataSize_;
    /// Vertex data.
    SharedArrayPtr<unsigned char> data_;
};

/// Description of index buffer data for asynchronous loading.
struct IndexBufferDesc
{
    /// Index count.
    unsigned indexCount_;
    /// Index size.
    unsigned indexSize_;
    /// Index data size.
    unsigned dataSize_;
    /// Index data.
    SharedArrayPtr<unsigned char> data_;
};

/// Description of a geometry for asynchronous loading.
struct GeometryDesc
{
    /// Primitive type.
    PrimitiveType type_;
    /// Vertex buffer ref.
    unsigned vbRef_;
    /// Index buffer ref.
    unsigned ibRef_;
    /// Index start.
    unsigned indexStart_;
    /// Index count.
    unsigned indexCount_;
};

/// 3D model resource.
class URHO3D_API Model : public Resource
{
    URHO3D_OBJECT(Model,Resource)

public:
    /// Construct.
    Model(Context* context);
    /// Destruct.
    virtual ~Model();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source) override;
    /// Finish resource loading. Always called from the main thread. Return true if successful.
    virtual bool EndLoad() override;
    /// Save resource. Return true if successful.
    virtual bool Save(Serializer& dest) const override;

    /// Set local-space bounding box.
    void SetBoundingBox(const BoundingBox& box);
    /// Set vertex buffers and their morph ranges.
    bool SetVertexBuffers(const std::vector<SharedPtr<VertexBuffer> >& buffers, const std::vector<unsigned>& morphRangeStarts, const std::vector<unsigned>& morphRangeCounts);
    /// Set index buffers.
    bool SetIndexBuffers(const std::vector<SharedPtr<IndexBuffer> >& buffers);
    /// Set number of geometries.
    void SetNumGeometries(unsigned num);
    /// Set number of LOD levels in a geometry.
    bool SetNumGeometryLodLevels(unsigned index, unsigned num);
    /// Set geometry.
    bool SetGeometry(unsigned index, unsigned lodLevel, Geometry* geometry);
    /// Set geometry center.
    bool SetGeometryCenter(unsigned index, const Vector3& center);
    /// Set skeleton.
    void SetSkeleton(const Skeleton& skeleton);
    /// Set bone mappings when model has more bones than the skinning shader can handle.
    void SetGeometryBoneMappings(const std::vector<std::vector<unsigned> >& mappings);
    /// Set vertex morphs.
    void SetMorphs(const std::vector<ModelMorph>& morphs);
    /// Clone the model. The geometry data is deep-copied and can be modified in the clone without affecting the original.
    SharedPtr<Model> Clone(const QString& cloneName = QString()) const;

    /// Return bounding box.
    const BoundingBox& GetBoundingBox() const { return boundingBox_; }
    /// Return skeleton.
    Skeleton& GetSkeleton() { return skeleton_; }
    /// Return vertex buffers.
    const std::vector<SharedPtr<VertexBuffer> >& GetVertexBuffers() const { return vertexBuffers_; }
    /// Return index buffers.
    const std::vector<SharedPtr<IndexBuffer> >& GetIndexBuffers() const { return indexBuffers_; }
    /// Return number of geometries.
    unsigned GetNumGeometries() const { return geometries_.size(); }
    /// Return number of LOD levels in geometry.
    unsigned GetNumGeometryLodLevels(unsigned index) const;
    /// Return geometry pointers.
    const std::vector<std::vector<SharedPtr<Geometry> > >& GetGeometries() const { return geometries_; }
    /// Return geometry center points.
    const std::vector<Vector3>& GetGeometryCenters() const { return geometryCenters_; }
    /// Return geometry by index and LOD level. The LOD level is clamped if out of range.
    Geometry* GetGeometry(unsigned index, unsigned lodLevel) const;
    /// Return geometry center by index.
    const Vector3& GetGeometryCenter(unsigned index) const { return index < geometryCenters_.size() ? geometryCenters_[index] : Vector3::ZERO; }
    /// Return geometery bone mappings.
    const std::vector<std::vector<unsigned> >& GetGeometryBoneMappings() const { return geometryBoneMappings_; }
    /// Return vertex morphs.
    const std::vector<ModelMorph>& GetMorphs() const { return morphs_; }
    /// Return number of vertex morphs.
    unsigned GetNumMorphs() const { return morphs_.size(); }
    /// Return vertex morph by index.
    const ModelMorph* GetMorph(unsigned index) const;
    /// Return vertex morph by name.
    const ModelMorph* GetMorph(const QString& name) const;
    /// Return vertex morph by name hash.
    const ModelMorph* GetMorph(StringHash nameHash) const;
    /// Return vertex buffer morph range start.
    unsigned GetMorphRangeStart(unsigned bufferIndex) const;
    /// Return vertex buffer morph range vertex count.
    unsigned GetMorphRangeCount(unsigned bufferIndex) const;

private:
    /// Bounding box.
    BoundingBox boundingBox_;
    /// Skeleton.
    Skeleton skeleton_;
    /// Vertex buffers.
    std::vector<SharedPtr<VertexBuffer> > vertexBuffers_;
    /// Index buffers.
    std::vector<SharedPtr<IndexBuffer> > indexBuffers_;
    /// Geometries.
    std::vector<std::vector<SharedPtr<Geometry> > > geometries_;
    /// Geometry bone mappings.
    std::vector<std::vector<unsigned> > geometryBoneMappings_;
    /// Geometry centers.
    std::vector<Vector3> geometryCenters_;
    /// Vertex morphs.
    std::vector<ModelMorph> morphs_;
    /// Vertex buffer morph range start.
    std::vector<unsigned> morphRangeStarts_;
    /// Vertex buffer morph range vertex count.
    std::vector<unsigned> morphRangeCounts_;
    /// Vertex buffer data for asynchronous loading.
    std::vector<VertexBufferDesc> loadVBData_;
    /// Index buffer data for asynchronous loading.
    std::vector<IndexBufferDesc> loadIBData_;
    /// Geometry definitions for asynchronous loading.
    std::vector<std::vector<GeometryDesc> > loadGeometries_;
};

}
