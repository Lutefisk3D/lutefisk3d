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

#include <functional>
#include "Lutefisk3D/Core/Lutefisk3D.h"
class QString;
class QStringRef;
namespace Urho3D
{

/// 32-bit hash value for a string.
class URHO3D_API StringHash
{
public:
    /// Construct with zero value.
    constexpr StringHash() : value_(0)
    {
    }

    /// Copy-construct from another hash.
    constexpr StringHash(const StringHash& rhs) : value_(rhs.value_)
    {
    }

    /// Copy-construct from another hash.
    StringHash(StringHash&& rhs) : value_(rhs.value_)
    {
        rhs.value_=0;
    }
    /// Construct with an initial value.
    explicit StringHash(unsigned value) : value_(value)
    {
    }

    /// Construct from a C string case-insensitively.
    StringHash(const char* str);
    /// Construct from a string case-insensitively.
    StringHash(const QString& str);
    /// Construct from a QString ref case-insensitively.
    StringHash(const QStringRef& str);

    /// Assign from another hash.
    StringHash& operator = (const StringHash& rhs)
    {
        value_ = rhs.value_;
        return *this;
    }

    /// Add a hash.
    StringHash operator + (const StringHash& rhs) const
    {
        StringHash ret;
        ret.value_ = value_ + rhs.value_;
        return ret;
    }

    /// Add-assign a hash.
    StringHash& operator += (const StringHash& rhs)
    {
        value_ += rhs.value_;
        return *this;
    }

    /// Test for equality with another hash.
    bool operator == (const StringHash& rhs) const { return value_ == rhs.value_; }
    /// Test for inequality with another hash.
    bool operator != (const StringHash& rhs) const { return value_ != rhs.value_; }
    /// Test if less than another hash.
    bool operator < (const StringHash& rhs) const { return value_ < rhs.value_; }
    /// Test if greater than another hash.
    bool operator > (const StringHash& rhs) const { return value_ > rhs.value_; }
    /// Return true if nonzero hash value.
    operator bool () const { return value_ != 0; }
    /// Return hash value.
    unsigned Value() const { return value_; }
    /// Return as string.
    QString ToString() const;
    /// Return hash value for HashSet & HashMap.
    unsigned ToHash() const { return value_; }

    /// Calculate hash value case-insensitively from a C string.
    static unsigned Calculate(const char* str);

    /// Zero hash.
    static const StringHash ZERO;

private:
    /// Hash value.
    unsigned value_;
};
inline unsigned int qHash(const StringHash &key, unsigned int seed)
{
    return key.ToHash();
}

}

namespace std {
template<> struct hash<Urho3D::StringHash> {
    size_t operator()(Urho3D::StringHash v) const {
        return v.ToHash();
    }
};
}
