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

#include "Lutefisk3D/Graphics/Geometry.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/IndexBuffer.h"
#include "Lutefisk3D/Graphics/VertexBuffer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Math/Ray.h"


namespace Urho3D
{

Geometry::Geometry(Context* context) :
    primitiveType_(TRIANGLE_LIST),
    indexStart_(0),
    indexCount_(0),
    vertexStart_(0),
    vertexCount_(0),
    lodDistance_(0.0f),
    rawVertexSize_(0),
    rawIndexSize_(0)
{
    SetNumVertexBuffers(1);
}

Geometry::~Geometry()
{
}

bool Geometry::SetNumVertexBuffers(unsigned num)
{
    if (num >= MAX_VERTEX_STREAMS)
    {
        URHO3D_LOGERROR("Too many vertex streams");
        return false;
    }

    vertexBuffers_.resize(num);

    return true;
}

bool Geometry::SetVertexBuffer(unsigned index, VertexBuffer* buffer)
{
    if (index >= vertexBuffers_.size())
    {
        URHO3D_LOGERROR("Stream index out of bounds");
        return false;
    }
    vertexBuffers_[index] = buffer;

    return true;
}

void Geometry::SetIndexBuffer(IndexBuffer* buffer)
{
    indexBuffer_ = buffer;
}

bool Geometry::SetDrawRange(PrimitiveType type, unsigned indexStart, unsigned indexCount, bool getUsedVertexRange)
{
    if (!indexBuffer_ && !rawIndexData_)
    {
        URHO3D_LOGERROR("Null index buffer and no raw index data, can not define indexed draw range");
        return false;
    }
    if (indexBuffer_ && indexStart + indexCount > indexBuffer_->GetIndexCount())
    {
        URHO3D_LOGERROR(QString("Illegal draw range %1 to %2, index buffer has %3 indices")
                 .arg(indexStart)
                 .arg(indexStart + indexCount - 1)
                 .arg(indexBuffer_->GetIndexCount()));
        return false;
    }

    primitiveType_ = type;
    indexStart_ = indexStart;
    indexCount_ = indexCount;

    // Get min.vertex index and num of vertices from index buffer. If it fails, use full range as fallback
    if (indexCount)
    {
        vertexStart_ = 0;
        vertexCount_ = vertexBuffers_[0] ? vertexBuffers_[0]->GetVertexCount() : 0;

        if (getUsedVertexRange && indexBuffer_)
            indexBuffer_->GetUsedVertexRange(indexStart_, indexCount_, vertexStart_, vertexCount_);
    }
    else
    {
        vertexStart_ = 0;
        vertexCount_ = 0;
    }

    return true;
}

bool Geometry::SetDrawRange(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount, bool checkIllegal)
{
    if (indexBuffer_)
    {
        // We can allow setting an illegal draw range now if the caller guarantees to resize / fill the buffer later
        if (checkIllegal && indexStart + indexCount > indexBuffer_->GetIndexCount())
        {
            URHO3D_LOGERROR(QString("Illegal draw range %1 to %2, index buffer has %3 indices")
                     .arg(indexStart)
                     .arg(indexStart + indexCount - 1)
                     .arg(indexBuffer_->GetIndexCount()));
            return false;
        }
    }
    else if (!rawIndexData_)
    {
        indexStart = 0;
        indexCount = 0;
    }

    primitiveType_ = type;
    indexStart_ = indexStart;
    indexCount_ = indexCount;
    vertexStart_ = minVertex;
    vertexCount_ = vertexCount;

    return true;
}

void Geometry::SetLodDistance(float distance)
{
    if (distance < 0.0f)
        distance = 0.0f;

    lodDistance_ = distance;
}

void Geometry::SetRawVertexData(SharedArrayPtr<uint8_t> data, const std::vector<VertexElement>& elements)
{
    rawVertexData_ = data;
    rawVertexSize_ = VertexBuffer::GetVertexSize(elements);
    rawElements_ = elements;
}

void Geometry::SetRawVertexData(SharedArrayPtr<uint8_t> data, unsigned elementMask)
{
    rawVertexData_ = data;
    rawVertexSize_ = VertexBuffer::GetVertexSize(elementMask);
    rawElements_ = VertexBuffer::GetElements(elementMask);
}

void Geometry::SetRawIndexData(SharedArrayPtr<uint8_t> data, unsigned indexSize)
{
    rawIndexData_ = data;
    rawIndexSize_ = indexSize;
}

void Geometry::Draw(Graphics* graphics)
{
    if (indexBuffer_ && indexCount_ > 0)
    {
        graphics->SetIndexBuffer(indexBuffer_);
        graphics->SetVertexBuffers(vertexBuffers_);
        graphics->Draw(primitiveType_, indexStart_, indexCount_, vertexStart_, vertexCount_);
    }
    else if (vertexCount_ > 0)
    {
        graphics->SetVertexBuffers(vertexBuffers_);
        graphics->Draw(primitiveType_, vertexStart_, vertexCount_);
    }
}

VertexBuffer* Geometry::GetVertexBuffer(unsigned index) const
{
    return index < vertexBuffers_.size() ? vertexBuffers_[index] : nullptr;
}
/// Return buffers' combined hash value for state sorting.
unsigned short Geometry::GetBufferHash() const
{
    unsigned short hash = 0;

    for (unsigned i = 0; i < vertexBuffers_.size(); ++i)
    {
        VertexBuffer* vBuf = vertexBuffers_[i];
        hash += *((unsigned short*)&vBuf);
    }

    IndexBuffer* iBuf = indexBuffer_;
    hash += *((unsigned short*)&iBuf);

    return hash;
}
/// Return raw vertex and index data for CPU operations, or null pointers if not available. Will return data of the first vertex buffer if override data not set.
void Geometry::GetRawData(const uint8_t*& vertexData, unsigned& vertexSize, const uint8_t*& indexData,
    unsigned& indexSize, const std::vector<VertexElement>*& elements) const
{
    if (rawVertexData_)
    {
        vertexData = rawVertexData_;
        vertexSize = rawVertexSize_;
        elements = &rawElements_;
    }
    else if (vertexBuffers_.size() && vertexBuffers_[0])
    {
        vertexData = vertexBuffers_[0]->GetShadowData();
        vertexSize = vertexBuffers_[0]->GetVertexSize();
        elements = &vertexBuffers_[0]->GetElements();
    }
    else
    {
        vertexData = nullptr;
        vertexSize = 0;
        elements = nullptr;
    }

    if (rawIndexData_)
    {
        indexData = rawIndexData_;
        indexSize = rawIndexSize_;
    }
    else
    {
        if (indexBuffer_)
        {
            indexData = indexBuffer_->GetShadowData();
            if (indexData)
                indexSize = indexBuffer_->GetIndexSize();
            else
                indexSize = 0;
        }
        else
        {
            indexData = nullptr;
            indexSize = 0;
        }
    }
}

void Geometry::GetRawDataShared(SharedArrayPtr<uint8_t>& vertexData, unsigned& vertexSize,
    SharedArrayPtr<uint8_t>& indexData, unsigned& indexSize,const std::vector<VertexElement>*& elements) const
{
    if (rawVertexData_)
    {
        vertexData = rawVertexData_;
        vertexSize = rawVertexSize_;
        elements = &rawElements_;
    }
    else if (vertexBuffers_.size() && vertexBuffers_[0])
    {
        vertexData = vertexBuffers_[0]->GetShadowDataShared();
        vertexSize = vertexBuffers_[0]->GetVertexSize();
        elements = &vertexBuffers_[0]->GetElements();
    }
    else
    {
        vertexData = nullptr;
        vertexSize = 0;
        elements = nullptr;
    }

    if (rawIndexData_)
    {
        indexData = rawIndexData_;
        indexSize = rawIndexSize_;
    }
    else
    {
        if (indexBuffer_)
        {
            indexData = indexBuffer_->GetShadowDataShared();
            if (indexData)
                indexSize = indexBuffer_->GetIndexSize();
            else
                indexSize = 0;
        }
        else
        {
            indexData = nullptr;
            indexSize = 0;
        }
    }
}

float Geometry::GetHitDistance(const Ray& ray, Vector3* outNormal, Vector2 * outUV) const
{
    const uint8_t* vertexData;
    const uint8_t* indexData;
    unsigned vertexSize;
    unsigned indexSize;
    const std::vector<VertexElement>* elements;

    GetRawData(vertexData, vertexSize, indexData, indexSize, elements);
    if (!vertexData || !elements || VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR3, SEM_POSITION) != 0)
        return M_INFINITY;

    unsigned uvOffset = VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR2, SEM_TEXCOORD);

    if (outUV && uvOffset == M_MAX_UNSIGNED)
    {
        // requested UV output, but no texture data in vertex buffer
        URHO3D_LOGWARNING("Illegal GetHitDistance call: UV return requested on vertex buffer without UV coords");
        *outUV = Vector2::ZERO;
        outUV = nullptr;
    }

    return indexData ? ray.HitDistance(vertexData, vertexSize, indexData, indexSize, indexStart_, indexCount_, outNormal, outUV,
        uvOffset) : ray.HitDistance(vertexData, vertexSize, vertexStart_, vertexCount_, outNormal, outUV, uvOffset);
}

bool Geometry::IsInside(const Ray& ray) const
{
    const uint8_t* vertexData;
    const uint8_t* indexData;
    unsigned vertexSize;
    unsigned indexSize;
    const std::vector<VertexElement>* elements;

    GetRawData(vertexData, vertexSize, indexData, indexSize, elements);

    return vertexData ? (indexData ? ray.InsideGeometry(vertexData, vertexSize, indexData, indexSize, indexStart_, indexCount_) :
                         ray.InsideGeometry(vertexData, vertexSize, vertexStart_, vertexCount_)) : false;
}


}
