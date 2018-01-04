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

#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Graphics/GPUObject.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Container/HandleManager.h"
#include <vector>
#include <limits>
namespace Urho3D
{
class Context;
/// Hardware vertex buffer.
class LUTEFISK3D_EXPORT VertexBuffer : public RefCounted, public GPUObject
{
    /// Prevent copy construction.
    VertexBuffer(const VertexBuffer& rhs)=delete;
    /// Prevent assignment.
    VertexBuffer& operator = (const VertexBuffer& rhs)=delete;

public:
    VertexBuffer(VertexBuffer&& rhs) :
        GPUObject(rhs),
        shadowData_(std::move(rhs.shadowData_)),
        vertexCount_(std::move(rhs.vertexCount_)),
        vertexSize_(std::move(rhs.vertexSize_)),
        elements_(std::move(rhs.elements_)),
        elementHash_(std::move(rhs.elementHash_)),
        elementMask_(std::move(rhs.elementMask_)),
        lockState_(std::move(rhs.lockState_)),
        lockStart_(std::move(rhs.lockStart_)),
        lockCount_(std::move(rhs.lockCount_)),
        lockScratchData_(std::move(rhs.lockScratchData_)),
        dynamic_(std::move(rhs.dynamic_)),
        shadowed_(std::move(rhs.shadowed_)),
        discardLock_(std::move(rhs.discardLock_))
    {
        rhs.vertexCount_ = 0;
        rhs.vertexSize_ = 0;
        rhs.elementHash_ = 0;
        rhs.elementMask_ = 0;
        rhs.lockState_ = LOCK_NONE;
        rhs.lockStart_ = 0;
        rhs.lockCount_ = 0;
        rhs.lockScratchData_ = nullptr;
        rhs.dynamic_ = 0;
        rhs.shadowed_ = 0;
        rhs.discardLock_ = 0;
    }
    VertexBuffer& operator = (VertexBuffer&& rhs) {
        if (this != &rhs) {
            shadowData_ = std::move(rhs.shadowData_);
            vertexCount_ = std::move(rhs.vertexCount_);
            vertexSize_ = std::move(rhs.vertexSize_);
            elements_ = std::move(rhs.elements_);
            elementHash_ = std::move(rhs.elementHash_);
            elementMask_ = std::move(rhs.elementMask_);
            lockState_ = std::move(rhs.lockState_);
            lockStart_ = std::move(rhs.lockStart_);
            lockCount_ = std::move(rhs.lockCount_);
            lockScratchData_ = std::move(rhs.lockScratchData_);
            dynamic_ = std::move(rhs.dynamic_);
            shadowed_ = std::move(rhs.shadowed_);
            discardLock_ = std::move(rhs.discardLock_);

            rhs.vertexCount_ = 0;
            rhs.vertexSize_ = 0;
            rhs.elementHash_ = 0;
            rhs.elementMask_ = 0;
            rhs.lockState_ = LOCK_NONE;
            rhs.lockStart_ = 0;
            rhs.lockCount_ = 0;
            rhs.lockScratchData_ = nullptr;
            rhs.dynamic_ = 0;
            rhs.shadowed_ = 0;
            rhs.discardLock_ = 0;
        }
        return  *this;
    }

    /// Construct. Optionally force headless (no GPU-side buffer) operation.
    explicit VertexBuffer(Context* context, bool forceHeadless = false);
    /// Destruct.
    ~VertexBuffer() override;

    /// Mark the buffer destroyed on graphics context destruction. May be a no-op depending on the API.
    void OnDeviceLost() override;
    /// Recreate the buffer and restore data if applicable. May be a no-op depending on the API.
    void OnDeviceReset() override;
    /// Release buffer.
    void Release() override;

    /// Enable shadowing in CPU memory. Shadowing is forced on if the graphics subsystem does not exist.
    void SetShadowed(bool enable);
    /// Set size, vertex elements and dynamic mode. Previous data will be lost.
    bool SetSize(unsigned vertexCount, const std::vector<VertexElement>& elements, bool dynamic = false);
    /// Set size and vertex elements and dynamic mode using legacy element bitmask. Previous data will be lost.
    bool SetSize(unsigned vertexCount, unsigned elementMask, bool dynamic = false);
    /// Set all data in the buffer.
    bool SetData(const void* data);
    /// Set a data range in the buffer. Optionally discard data outside the range.
    bool SetDataRange(const void* data, unsigned start, unsigned count, bool discard = false);
    /// Lock the buffer for write-only editing. Return data pointer if successful. Optionally discard data outside the range.
    void* Lock(unsigned start, unsigned count, bool discard = false);
    /// Unlock the buffer and apply changes to the GPU buffer.
    void Unlock();

    /// Return whether CPU memory shadowing is enabled.
    bool IsShadowed() const { return shadowed_; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return dynamic_; }
    /// Return whether is currently locked.
    bool IsLocked() const { return lockState_ != LOCK_NONE; }
    /// Return number of vertices.
    unsigned GetVertexCount() const {return vertexCount_; }
    /// Return vertex size in bytes.
    unsigned GetVertexSize() const { return vertexSize_; }
    /// Return vertex elements.
    const std::vector<VertexElement>& GetElements() const { return elements_; }

    /// Return vertex element, or null if does not exist.
    const VertexElement* GetElement(VertexElementSemantic semantic, unsigned char index = 0) const;

    /// Return vertex element with specific type, or null if does not exist.
    const VertexElement* GetElement(VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0) const;

    /// Return whether has a specified element semantic.
    bool HasElement(VertexElementSemantic semantic, unsigned char index = 0) const { return GetElement(semantic, index) != 0; }

    /// Return whether has an element semantic with specific type.
    bool HasElement(VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0) const { return GetElement(type, semantic, index) != 0; }

    /// Return offset of a element within vertex, or M_MAX_UNSIGNED if does not exist.
    unsigned GetElementOffset(VertexElementSemantic semantic, unsigned char index = 0) const
    {
        const VertexElement *element = GetElement(semantic, index);
        return element ? element->offset_ : std::numeric_limits<unsigned>::max();
    }

    /// Return offset of a element with specific type within vertex, or M_MAX_UNSIGNED if element does not exist.
    unsigned GetElementOffset(VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0) const
    {
        const VertexElement *element = GetElement(type, semantic, index);
        return element ? element->offset_ : std::numeric_limits<unsigned>::max();
    }

    /// Return legacy vertex element mask. Note that both semantic and type must match the legacy element for a mask bit to be set.
    unsigned GetElementMask() const { return elementMask_; }
    /// Return CPU memory shadow data.
    unsigned char* GetShadowData() const { return shadowData_.Get(); }
    /// Return shared array pointer to the CPU memory shadow data.
    SharedArrayPtr<unsigned char> GetShadowDataShared() const { return shadowData_; }

    /// Return buffer hash for building vertex declarations. Used internally.
    uint64_t GetBufferHash(unsigned streamIndex) { return elementHash_ << (streamIndex * 16); }

    /// Return element with specified type and semantic from a vertex element list, or null if does not exist.
    static const VertexElement* GetElement(const std::vector<VertexElement>& elements, VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0);

    /// Return whether element list has a specified element type and semantic.
    static bool HasElement(const std::vector<VertexElement>& elements, VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0);

    /// Return element offset for specified type and semantic from a vertex element list, or M_MAX_UNSIGNED if does not exist.
    static unsigned GetElementOffset(const std::vector<VertexElement>& elements, VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0);

    /// Return a vertex element list from a legacy element bitmask.
    static std::vector<VertexElement> GetElements(unsigned elementMask);

    /// Return vertex size from an element list.
    static unsigned GetVertexSize(const std::vector<VertexElement>& elements);

    /// Return vertex size for a legacy vertex element bitmask.
    static unsigned GetVertexSize(unsigned elementMask);
    /// Update offsets of vertex elements.
    static void UpdateOffsets(std::vector<VertexElement>& elements);
private:
    /// Update offsets of vertex elements.
    void UpdateOffsets();
    /// Create buffer.
    bool Create();
    /// Update the shadow data to the GPU buffer.
    bool UpdateToGPU();
    /// Map the GPU buffer into CPU memory. Not used on OpenGL.
    void* MapBuffer(unsigned start, unsigned count, bool discard);
    /// Unmap the GPU buffer. Not used on OpenGL.
    void UnmapBuffer();

    /// Shadow data.
    SharedArrayPtr<unsigned char> shadowData_;
    /// Number of vertices.
    unsigned vertexCount_;
    /// Vertex size.
    unsigned vertexSize_;
    /// Vertex elements.
    std::vector<VertexElement> elements_;
    /// Vertex element hash.
    uint64_t elementHash_;
    /// Vertex element legacy bitmask.
    unsigned elementMask_;
    /// Buffer locking state.
    LockState lockState_;
    /// Lock start vertex.
    unsigned lockStart_;
    /// Lock number of vertices.
    unsigned lockCount_;
    /// Scratch buffer for fallback locking.
    void* lockScratchData_;
    /// Dynamic flag.
    bool dynamic_;
    /// Shadowed flag.
    bool shadowed_;
    /// Discard lock flag. Used by OpenGL only.
    bool discardLock_;
};
//struct VertexBufferManager : public HandleManager<VertexBuffer> {
//    Handle build(Context *ctx,bool shadowed,unsigned count,unsigned elementMask,const float *data) {
//        Handle ibh = add(ctx,"");
//        VertexBuffer &ibuf(get(ibh));
//        ibuf.SetShadowed(shadowed);
//        ibuf.SetSize(count, elementMask);
//        ibuf.SetData(data);
//        return ibh;
//    }
//    void uploadData(Handle h, unsigned count, const std::vector<VertexElement> &elem_decls,
//                    const unsigned char *data)
//    {
//        VertexBuffer &ibuf(get(h));
//        ibuf.SetShadowed(true);
//        ibuf.SetSize(count, elem_decls);
//        ibuf.SetData(data);
//    }
//    unsigned getVertexCount(Handle h) { return valid(h) ? get(h).GetVertexCount() : 0; }
//};
//using VertexBufferHandle = VertexBufferManager::Handle;
//extern VertexBufferManager g_vertexBufferManager;

}
