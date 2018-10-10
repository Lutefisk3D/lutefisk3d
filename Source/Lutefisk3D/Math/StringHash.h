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
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include "Lutefisk3D/Math/MathDefs.h"

#include <functional> // for std::hash specialization
class QString;
class QStringRef;
class QLatin1String;
namespace Urho3D
{
class StringHashRegister;

/// 32-bit hash value for a string.
class LUTEFISK3D_EXPORT StringHash
{
public:
    /// Construct with zero value.
#ifndef LUTEFISK3D_HASH_DEBUG
    constexpr
#endif
    StringHash() noexcept : value_(0)
    {
    }

    /// Copy-construct from another hash.
#ifndef LUTEFISK3D_HASH_DEBUG
    constexpr
#endif
    StringHash(const StringHash& rhs) = default;

    /// Construct with an initial value.
    explicit StringHash(unsigned value) : value_(value)
    {
    }

    /// Construct from a C string case-insensitively.
#ifndef LUTEFISK3D_HASH_DEBUG
    constexpr
    StringHash(const char* str) noexcept :      // NOLINT(google-explicit-constructor)
        value_(Calculate(str))
    {
    }
#else
    StringHash(const char* str) noexcept;
#endif


    /// Construct from a string case-insensitively.
    StringHash(const QString& str);
    /// Construct from a string case-insensitively.
    StringHash(const QLatin1String& str);
    /// Construct from a QString ref case-insensitively.
    StringHash(const QStringRef& str);

    /// Assign from another hash.
    StringHash& operator =(const StringHash& rhs) noexcept = default;

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
    explicit operator bool() const { return value_ != 0; }
    /// Return hash value.
    unsigned Value() const { return value_; }
    /// Return as string.
    QString ToString() const;
    /// Return string which has specific hash value. Return first string if many (in order of calculation). Use for debug purposes only. Return empty string if LUTEFISK3D_HASH_DEBUG is off.
    QString Reverse() const;
    /// Return hash value for HashSet & HashMap.
    unsigned ToHash() const { return value_; }

#ifndef LUTEFISK3D_HASH_DEBUG
    /// Calculate hash value case-insensitively from a C string.
    static constexpr unsigned Calculate(const char *str, unsigned hash = 0)
    {
        return str == nullptr || *str == 0
                   ? hash
                   : Calculate(
                         str + 1,
                         SDBMHash(hash, (unsigned char)(((*str) >= 'A' && (*str) <= 'Z') ? (*str) + ('a' - 'A') : (*str))));
    }
#else
    /// Calculate hash value case-insensitively from a C string.
    static unsigned Calculate(const char* str, unsigned hash = 0);
#endif
    /// Calculate hash value from binary data.
    static unsigned Calculate(void* data, unsigned length, unsigned hash = 0);

    /// Get global StringHashRegister. Use for debug purposes only. Return nullptr if LUTEFISK3D_HASH_DEBUG is off.
    static StringHashRegister* GetGlobalStringHashRegister();
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
template<class T>
inline unsigned int qHash(const Urho3D::StringHash & key, unsigned int seed)
{
    return key.ToHash()^seed;
}

}

namespace std {
template<> struct hash<Urho3D::StringHash> {
    size_t operator()(Urho3D::StringHash v) const {
        return v.ToHash();
    }
};
}
