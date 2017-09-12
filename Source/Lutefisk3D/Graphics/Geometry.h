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

#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Container/DataHandle.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include <vector>
namespace Urho3D
{
class LUTEFISK3D_EXPORT Vector2;
class LUTEFISK3D_EXPORT IndexBuffer;
class LUTEFISK3D_EXPORT Ray;
class LUTEFISK3D_EXPORT Graphics;
class LUTEFISK3D_EXPORT VertexBuffer;
class LUTEFISK3D_EXPORT Context;
using IndexBufferHandle = DataHandle<IndexBuffer,20,20>;
using VertexBufferHandle = DataHandle<VertexBuffer,20,20>;
/// Defines one or more vertex buffers, an index buffer and a draw range.
class LUTEFISK3D_EXPORT Geometry : public RefCounted
{
public:
    /// Construct with one empty vertex buffer.
    Geometry(Context* context);
    /// Destruct.
    virtual ~Geometry();

    /// Set number of vertex buffers.
    bool SetNumVertexBuffers(unsigned num);
    /// Set a vertex buffer by index.
    bool SetVertexBuffer(unsigned index, VertexBuffer* buffer);
    /// Set the index buffer.
    void SetIndexBuffer(IndexBuffer* buffer);
    /// Set the draw range.
    bool SetDrawRange(PrimitiveType type, unsigned indexStart, unsigned indexCount, bool getUsedVertexRange = true);
    /// Set the draw range.
    bool SetDrawRange(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned vertexStart, unsigned vertexCount, bool checkIllegal = true);
    /// Set the LOD distance.
    void SetLodDistance(float distance);
    /// Override raw vertex data to be returned for CPU-side operations.
    void SetRawVertexData(SharedArrayPtr<uint8_t> data, const std::vector<VertexElement>& elements);
    /// Override raw vertex data to be returned for CPU-side operations using a legacy vertex bitmask.
    void SetRawVertexData(SharedArrayPtr<uint8_t> data, unsigned elementMask);
    /// Override raw index data to be returned for CPU-side operations.
    void SetRawIndexData(SharedArrayPtr<uint8_t> data, unsigned indexSize);
    /// Draw.
    void Draw(Graphics* graphics);

    /// Return all vertex buffers.
    const std::vector<SharedPtr<VertexBuffer> >& GetVertexBuffers() const { return vertexBuffers_; }
    /// Return number of vertex buffers.
    unsigned GetNumVertexBuffers() const { return vertexBuffers_.size(); }
    /// Return vertex buffer by index.
    VertexBuffer* GetVertexBuffer(unsigned index) const;
    /// Return the index buffer.
    IndexBuffer* GetIndexBuffer() const { return indexBuffer_; }
    /// Return primitive type.
    PrimitiveType GetPrimitiveType() const { return primitiveType_; }
    /// Return start index.
    unsigned GetIndexStart() const { return indexStart_; }
    /// Return number of indices.
    unsigned GetIndexCount() const { return indexCount_; }
    /// Return first used vertex.
    unsigned GetVertexStart() const { return vertexStart_; }
    /// Return number of used vertices.
    unsigned GetVertexCount() const { return vertexCount_; }
    /// Return LOD distance.
    float GetLodDistance() const { return lodDistance_; }
    unsigned short GetBufferHash() const;
    void GetRawData(const uint8_t*& vertexData, unsigned& vertexSize, const uint8_t*& indexData, unsigned& indexSize, const std::vector<VertexElement>*& elements) const;
    void GetRawDataShared(SharedArrayPtr<uint8_t>& vertexData, unsigned& vertexSize, SharedArrayPtr<uint8_t>& indexData, unsigned& indexSize, const std::vector<VertexElement>*& elements) const;
    /// Return ray hit distance or infinity if no hit. Requires raw data to be set. Optionally return hit normal and hit uv coordinates at intersect point.
    float GetHitDistance(const Ray& ray, Vector3* outNormal = nullptr,Vector2* outUV = nullptr) const;
    /// Return whether or not the ray is inside geometry.
    bool IsInside(const Ray& ray) const;
    /// Return whether has empty draw range.
    bool IsEmpty() const { return indexCount_ == 0 && vertexCount_ == 0; }

private:
    std::vector<SharedPtr<VertexBuffer>> vertexBuffers_; //!< Vertex buffers.
    SharedPtr<IndexBuffer>               indexBuffer_;   //!< Index buffer.
    PrimitiveType                        primitiveType_; //!< Primitive type.
    unsigned                             indexStart_;    //!< Start index.
    unsigned                             indexCount_;    //!< Number of indices.
    unsigned                             vertexStart_;   //!< First used vertex.
    unsigned                             vertexCount_;   //!< Number of used vertices.
    float                                lodDistance_;   //!< LOD distance.
    std::vector<VertexElement>           rawElements_;   //!< Raw vertex data elements.
    SharedArrayPtr<uint8_t>              rawVertexData_; //!< Raw vertex data override.
    SharedArrayPtr<uint8_t>              rawIndexData_;  //!< Raw index data override.
    unsigned                             rawVertexSize_; //!< Raw vertex data override size.
    unsigned                             rawIndexSize_;  //!< Raw index data override size.
};

}
