//
// Copyright (c) 2008-2018 the Urho3D project.
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
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include <functional>

namespace Urho3D
{

/// Reference count structure.
struct RefCount
{
    ~RefCount()
    {
        // Set reference counts below zero to fire asserts if this object is still accessed
        refs_ = -1;
        weakRefs_ = -1;
    }
    int refs_     = 0; //!< Reference count. If below zero, the object has been destroyed.
    int weakRefs_ = 0; //!< Weak reference count.
};

/// Base class for intrusively reference-counted objects. These are noncopyable and non-assignable.
class LUTEFISK3D_EXPORT RefCounted
{
public:
    /// Construct. Allocate the reference count structure and set an initial self weak reference.
     RefCounted();
    /// Destruct. Mark as expired and also delete the reference count structure if no outside weak references exist.
    virtual ~RefCounted();
    /// Prevent copy construction.
    RefCounted(const RefCounted& rhs) = delete;
    /// Prevent assignment.
    RefCounted& operator = (const RefCounted& rhs) = delete;

    /// Increment reference count. Can also be called outside of a SharedPtr for traditional reference counting.
    void AddRef();
    /// Decrement reference count and delete self if no more references. Can also be called outside of a SharedPtr for traditional reference counting.
    void ReleaseRef();
    /// Return reference count.
    int Refs() const { return refCount_->refs_; }
    /// Return weak reference count.
    /// \note does not count the internally held reference
    int WeakRefs() const
    {
        return refCount_->weakRefs_ - 1;
    }
    /// Return pointer to the reference count structure.
    RefCount* RefCountPtr() { return refCount_; }

    /// Set a custom deleter function which will be in charge of deallocating object.
    void SetDeleter(std::function<void(RefCounted*)> deleter);
    /// Returns custom deleter of this object.
    std::function<void(RefCounted*)> GetDeleter() const { return deleter_; }
private:
    /// Pointer to the reference count structure.
    RefCount* refCount_;
    /// Custom deleter which will be deallocating native object.
    std::function<void (RefCounted*)> deleter_;
};
} // end of namespace
#include <vector>
#include <utility>
#include <algorithm>
template<class T>
void RemovePopBack(std::vector<T> &l,const T &v)
{
    auto iter=std::find(l.begin(),l.end(),v);
    if(iter!=l.end())
    {
        std::swap(*iter,l.back());
        l.pop_back();
    }
}
