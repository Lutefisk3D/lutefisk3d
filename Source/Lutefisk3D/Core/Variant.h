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

#include "Lutefisk3D/Math/Color.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Container/Str.h"

namespace Urho3D
{
/// Variant's supported types.
enum LUTEFISK3D_EXPORT VariantType : uint8_t
{
    VAR_NONE = 0,
    VAR_INT,
    VAR_BOOL,
    VAR_FLOAT,
    VAR_VECTOR2,
    VAR_VECTOR3,
    VAR_VECTOR4,
    VAR_QUATERNION,
    VAR_COLOR,
    VAR_STRING,
    VAR_BUFFER,
    VAR_VOIDPTR,
    VAR_RESOURCEREF,
    VAR_RESOURCEREFLIST,
    VAR_VARIANTVECTOR,
    VAR_VARIANTMAP,
    VAR_INTRECT,
    VAR_INTVECTOR2,
    VAR_PTR,
    VAR_MATRIX3,
    VAR_MATRIX3X4,
    VAR_MATRIX4,
    VAR_DOUBLE,
    VAR_STRINGVECTOR,
    VAR_RECT,
    VAR_INTVECTOR3,
    VAR_INT64,
    MAX_VAR_TYPES
};

/// Union for the possible variant values. Also stores non-POD objects such as String and math objects
/// (excluding Matrix) which must not exceed 40 bytes in size (or 80 bytes in a 64-bit build.)
/// Objects exceeding the limit are allocated on the heap and pointed to by _ptr.
struct VariantValue
{
    union
    {
        int int_;
        bool bool_;
        float float_;
        double double_;
        void* ptr_;
    };

    union
    {
        int int2_;
        float float2_;
        void* ptr2_;
    };

    union
    {
        int int3_;
        float float3_;
        void* ptr3_;
    };

    union
    {
        int int4_;
        float float4_;
        void* ptr4_;
    };
    union
    {
        void* ptr5_[6];
    };
#ifdef _GLIBCXX_DEBUG
    union
    {
        void* ptr7_[8];
    };
#endif
};
static_assert(sizeof(VariantValue)>=sizeof(QStringList),"Variant value to small");
struct ResourceRef;
struct ResourceRefList;

class Variant;

/// Vector of variants.
using VariantVector = std::vector<Variant>;
static_assert(sizeof(VariantValue)>=sizeof(VariantVector),"Variant value must be large enough to hold VariantVector");


/// Map of variants.
using VariantMap = HashMap<StringHash, Variant> ;

/// Variable that supports a fixed set of types.
class LUTEFISK3D_EXPORT Variant
{
public:
    /// Construct empty.
    Variant()
    {
    }

    /// Construct from integer.
    Variant(int value)
    {
        *this = value;
    }

    /// Construct from unsigned integer.
    Variant(uint32_t value)
    {
        *this = (int)value;
    }
    /// Construct from size_t type //ERROR: truncates on 64 bit on OSes
    Variant(long long value)
    {
        *this = (long long)value;
    }
    /// Construct from a string hash (convert to integer).
    Variant(const StringHash& value)
    {
        *this = (int)value.Value();
    }

    /// Construct from a bool.
    Variant(bool value)
    {
        *this = value;
    }

    /// Construct from a float.
    Variant(float value)
    {
        *this = value;
    }

    /// Construct from a double.
    Variant(double value)
    {
        *this = value;
    }

    /// Construct from a Vector2.
    Variant(const Vector2& value)
    {
        *this = value;
    }

    /// Construct from a Vector3.
    Variant(const Vector3& value)
    {
        *this = value;
    }

    /// Construct from a Vector4.
    Variant(const Vector4& value)
    {
        *this = value;
    }

    /// Construct from a quaternion.
    Variant(const Quaternion& value)
    {
        *this = value;
    }

    /// Construct from a color.
    Variant(const Color& value)
    {
        *this = value;
    }

    /// Construct from a string.
    Variant(const QString& value)
    {
        *this = value;
    }

    /// Construct from a C string.
    Variant(const char* value)
    {
        *this = value;
    }

    /// Construct from a buffer.
    Variant(const std::vector<unsigned char>& value)
    {
        *this = value;
    }

    /// Construct from a pointer.
    Variant(void* value)
    {
        *this = value;
    }

    /// Construct from a resource reference.
    Variant(const ResourceRef& value)
    {
        *this = value;
    }

    /// Construct from a resource reference list.
    Variant(const ResourceRefList& value)
    {
        *this = value;
    }

    /// Construct from a variant vector.
    Variant(const VariantVector& value)
    {
        *this = value;
    }

    /// Construct from a variant map.
    Variant(const VariantMap& value)
    {
        *this = value;
    }

    /// Construct from a string vector.
    Variant(const QStringList& value)
    {
        *this = value;
    }
    /// Construct from a rect.
    Variant(const Rect& value)
    {
        *this = value;
    }
    /// Construct from an integer rect.
    Variant(const IntRect& value)
    {
        *this = value;
    }

    /// Construct from an IntVector2.
    Variant(const IntVector2& value)
    {
        *this = value;
    }
    /// Construct from an IntVector3.
    Variant(const IntVector3& value)
    {
        *this = value;
    }
    /// Construct from a RefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant(RefCounted* value)
    {
        *this = value;
    }

    /// Construct from a Matrix3.
    Variant(const Matrix3& value)
    {
        *this = value;
    }

    /// Construct from a Matrix3x4.
    Variant(const Matrix3x4& value)
    {
        *this = value;
    }

    /// Construct from a Matrix4.
    Variant(const Matrix4& value)
    {
        *this = value;
    }

    /// Construct from type and value.
    Variant(const QString& type, const QString& value)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(VariantType type, const QString& value)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(const char* type, const char* value)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(VariantType type, const char* value)
    {
        FromString(type, value);
    }

    /// Copy-construct from another variant.
    Variant(const Variant& value)
    {
        *this = value;
    }

    /// Destruct.
    ~Variant()
    {
        SetType(VAR_NONE);
    }

    /// Reset to empty.
    void Clear()
    {
        SetType(VAR_NONE);
    }

    /// Assign from another variant.
    Variant& operator = (const Variant& rhs);

    /// Assign from an integer.
    Variant& operator = (int rhs)
    {
        SetType(VAR_INT);
        value_.int_ = rhs;
        return *this;
    }
    /// Assign from 64 bit integer.
    Variant& operator =(long long rhs)
    {
        SetType(VAR_INT64);
        *reinterpret_cast<long long*>(&value_) = rhs;
        return *this;
    }

    /// Assign from unsigned 64 bit integer.
    Variant& operator =(uint64_t rhs)
    {
        SetType(VAR_INT64);
        *reinterpret_cast<long long*>(&value_) = (long long)rhs;
        return *this;
    }
    /// Assign from an unsigned integer.
    Variant& operator = (unsigned rhs)
    {
        SetType(VAR_INT);
        value_.int_ = (int)rhs;
        return *this;
    }

    /// Assign from a StringHash (convert to integer.)
    Variant& operator = (const StringHash& rhs)
    {
        SetType(VAR_INT);
        value_.int_ = (int)rhs.Value();
        return *this;
    }

    /// Assign from a bool.
    Variant& operator = (bool rhs)
    {
        SetType(VAR_BOOL);
        value_.bool_ = rhs;
        return *this;
    }

    /// Assign from a float.
    Variant& operator = (float rhs)
    {
        SetType(VAR_FLOAT);
        value_.float_ = rhs;
        return *this;
    }

    /// Assign from a double.
    Variant& operator = (double rhs)
    {
        SetType(VAR_DOUBLE);
        *(reinterpret_cast<double*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a Vector2.
    Variant& operator = (const Vector2& rhs)
    {
        SetType(VAR_VECTOR2);
        *(reinterpret_cast<Vector2*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a Vector3.
    Variant& operator = (const Vector3& rhs)
    {
        SetType(VAR_VECTOR3);
        *(reinterpret_cast<Vector3*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a Vector4.
    Variant& operator = (const Vector4& rhs)
    {
        SetType(VAR_VECTOR4);
        *(reinterpret_cast<Vector4*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a quaternion.
    Variant& operator = (const Quaternion& rhs)
    {
        SetType(VAR_QUATERNION);
        *(reinterpret_cast<Quaternion*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a color.
    Variant& operator = (const Color& rhs)
    {
        SetType(VAR_COLOR);
        *(reinterpret_cast<Color*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a string.
    Variant& operator = (const QString& rhs)
    {
        SetType(VAR_STRING);
        *(reinterpret_cast<QString*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a C string.
    Variant& operator = (const char* rhs)
    {
        SetType(VAR_STRING);
        *(reinterpret_cast<QString*>(&value_)) = QString(rhs);
        return *this;
    }

    /// Assign from a buffer.
    Variant& operator = (const std::vector<unsigned char>& rhs)
    {
        SetType(VAR_BUFFER);
        *(reinterpret_cast<std::vector<unsigned char>*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a void pointer.
    Variant& operator = (void* rhs)
    {
        SetType(VAR_VOIDPTR);
        value_.ptr_ = rhs;
        return *this;
    }

    /// Assign from a resource reference.
    Variant& operator= (const ResourceRef& rhs);

    /// Assign from a resource reference list.
    Variant& operator = (const ResourceRefList& rhs);

    /// Assign from a variant vector.
    Variant& operator = (const VariantVector& rhs)
    {
        SetType(VAR_VARIANTVECTOR);
        *(reinterpret_cast<VariantVector*>(&value_)) = rhs;
        return *this;
    }

    // Assign from a string vector.
    Variant& operator =(const QStringList & rhs)
    {
        SetType(VAR_STRINGVECTOR);
        *(reinterpret_cast<QStringList*>(&value_)) = rhs;
        return *this;
    }
    /// Assign from a variant map.
    Variant& operator = (const VariantMap& rhs)
    {
        SetType(VAR_VARIANTMAP);
        *(reinterpret_cast<VariantMap*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a rect.
    Variant& operator =(const Rect& rhs)
    {
        SetType(VAR_RECT);
        *(reinterpret_cast<Rect*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from an integer rect.
    Variant& operator = (const IntRect& rhs)
    {
        SetType(VAR_INTRECT);
        *(reinterpret_cast<IntRect*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from an IntVector2.
    Variant& operator = (const IntVector2& rhs)
    {
        SetType(VAR_INTVECTOR2);
        *(reinterpret_cast<IntVector2*>(&value_)) = rhs;
        return *this;
    }
    /// Assign from an IntVector3.
    Variant& operator =(const IntVector3& rhs)
    {
        SetType(VAR_INTVECTOR3);
        *(reinterpret_cast<IntVector3*>(&value_)) = rhs;
        return *this;
    }
    /// Assign from a RefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant& operator = (RefCounted* rhs)
    {
        SetType(VAR_PTR);
        *(reinterpret_cast<WeakPtr<RefCounted>*>(&value_)) = rhs;
        return *this;
    }

    /// Assign from a Matrix3.
    Variant& operator = (const Matrix3& rhs)
    {
        SetType(VAR_MATRIX3);
        *(reinterpret_cast<Matrix3*>(value_.ptr_)) = rhs;
        return *this;
    }

    /// Assign from a Matrix3x4.
    Variant& operator = (const Matrix3x4& rhs)
    {
        SetType(VAR_MATRIX3X4);
        *(reinterpret_cast<Matrix3x4*>(value_.ptr_)) = rhs;
        return *this;
    }

    /// Assign from a Matrix4.
    Variant& operator = (const Matrix4& rhs)
    {
        SetType(VAR_MATRIX4);
        *(reinterpret_cast<Matrix4*>(value_.ptr_)) = rhs;
        return *this;
    }

    /// Test for equality with another variant.
    bool operator == (const Variant& rhs) const;
    /// Test for equality with an integer. To return true, both the type and value must match.
    bool operator == (int rhs) const { return type_ == VAR_INT ? value_.int_ == rhs : false; }
    /// Test for equality with an unsigned integer. To return true, both the type and value must match.
    bool operator == (unsigned rhs) const { return type_ == VAR_INT ? value_.int_ == (int)rhs : false; }
    /// Test for equality with a bool. To return true, both the type and value must match.
    bool operator ==(long long rhs) const { return type_ == VAR_INT64 ? *reinterpret_cast<const long long*>(&value_.int_) == rhs : false; }

    /// Test for equality with an unsigned integer. To return true, both the type and value must match.
    bool operator ==(uint64_t rhs) const { return type_ == VAR_INT64 ? *reinterpret_cast<const uint64_t*>(&value_.int_) == (int)rhs : false; }
    bool operator == (bool rhs) const { return type_ == VAR_BOOL ? value_.bool_ == rhs : false; }
    /// Test for equality with a float. To return true, both the type and value must match.
    bool operator == (float rhs) const { return type_ == VAR_FLOAT ? value_.float_ == rhs : false; }
    /// Test for equality with a double. To return true, both the type and value must match.
    bool operator ==(double rhs) const { return type_ == VAR_DOUBLE ? *(reinterpret_cast<const double*>(&value_)) == rhs : false; }
    /// Test for equality with a Vector2. To return true, both the type and value must match.
    bool operator == (const Vector2& rhs) const { return type_ == VAR_VECTOR2 ? *(reinterpret_cast<const Vector2*>(&value_)) == rhs : false; }
    /// Test for equality with a Vector3. To return true, both the type and value must match.
    bool operator == (const Vector3& rhs) const { return type_ == VAR_VECTOR3 ? *(reinterpret_cast<const Vector3*>(&value_)) == rhs : false; }
    /// Test for equality with a Vector4. To return true, both the type and value must match.
    bool operator == (const Vector4& rhs) const { return type_ == VAR_VECTOR4 ? *(reinterpret_cast<const Vector4*>(&value_)) == rhs : false; }
    /// Test for equality with a quaternion. To return true, both the type and value must match.
    bool operator == (const Quaternion& rhs) const { return type_ == VAR_QUATERNION ? *(reinterpret_cast<const Quaternion*>(&value_)) == rhs : false; }
    /// Test for equality with a color. To return true, both the type and value must match.
    bool operator == (const Color& rhs) const { return type_ == VAR_COLOR ? *(reinterpret_cast<const Color*>(&value_)) == rhs : false; }
    /// Test for equality with a string. To return true, both the type and value must match.
    bool operator == (const QString& rhs) const { return type_ == VAR_STRING ? *(reinterpret_cast<const QString*>(&value_)) == rhs : false; }
    /// Test for equality with a buffer. To return true, both the type and value must match.
    bool operator == (const std::vector<unsigned char>& rhs) const;

    /// Test for equality with a void pointer. To return true, both the type and value must match, with the exception that a RefCounted pointer is also allowed.
    bool operator == (void* rhs) const
    {
        if (type_ == VAR_VOIDPTR)
            return value_.ptr_ == rhs;
        else if (type_ == VAR_PTR)
            return *(reinterpret_cast<const WeakPtr<RefCounted>*>(&value_)) == rhs;
        else
            return false;
    }

    /// Test for equality with a resource reference. To return true, both the type and value must match.
    bool operator == (const ResourceRef& rhs) const;
    /// Test for equality with a resource reference list. To return true, both the type and value must match.
    bool operator == (const ResourceRefList& rhs) const;
    /// Test for equality with a variant vector. To return true, both the type and value must match.
    bool operator == (const VariantVector& rhs) const { return type_ == VAR_VARIANTVECTOR ? *(reinterpret_cast<const VariantVector*>(&value_)) == rhs : false; }
   /// Test for equality with a string vector. To return true, both the type and value must match.
    bool operator ==(const QStringList &rhs) const
    {
        return type_ == VAR_STRINGVECTOR ? *(reinterpret_cast<const QStringList*>(&value_)) == rhs : false;
    }
    /// Test for equality with a variant map. To return true, both the type and value must match.
    bool operator == (const VariantMap& rhs) const { return type_ == VAR_VARIANTMAP ? *(reinterpret_cast<const VariantMap*>(&value_)) == rhs : false; }
    /// Test for equality with a rect. To return true, both the type and value must match.
    bool operator ==(const Rect& rhs) const    {        return type_ == VAR_RECT ? *(reinterpret_cast<const Rect*>(&value_)) == rhs : false;    }
    /// Test for equality with an integer rect. To return true, both the type and value must match.
    bool operator == (const IntRect& rhs) const { return type_ == VAR_INTRECT ? *(reinterpret_cast<const IntRect*>(&value_)) == rhs : false; }
    /// Test for equality with an IntVector2. To return true, both the type and value must match.
    bool operator == (const IntVector2& rhs) const { return type_ == VAR_INTVECTOR2 ? *(reinterpret_cast<const IntVector2*>(&value_)) == rhs : false; }
    /// Test for equality with an IntVector3. To return true, both the type and value must match.
    bool operator ==(const IntVector3& rhs) const    {        return type_ == VAR_INTVECTOR3 ? *(reinterpret_cast<const IntVector3*>(&value_)) == rhs : false;    }
    /// Test for equality with a StringHash. To return true, both the type and value must match.
    bool operator == (const StringHash& rhs) const { return type_ == VAR_INT ? (unsigned)value_.int_ == rhs.Value() : false; }

    /// Test for equality with a RefCounted pointer. To return true, both the type and value must match, with the exception that void pointer is also allowed.
    bool operator == (RefCounted* rhs) const
    {
        if (type_ == VAR_PTR)
            return *(reinterpret_cast<const WeakPtr<RefCounted>*>(&value_)) == rhs;
        else if (type_ == VAR_VOIDPTR)
            return value_.ptr_ == rhs;
        else
            return false;
    }

    /// Test for equality with a Matrix3. To return true, both the type and value must match.
    bool operator == (const Matrix3& rhs) const { return type_ == VAR_MATRIX3 ? *(reinterpret_cast<const Matrix3*>(value_.ptr_)) == rhs : false; }
    /// Test for equality with a Matrix3x4. To return true, both the type and value must match.
    bool operator == (const Matrix3x4& rhs) const { return type_ == VAR_MATRIX3X4 ? *(reinterpret_cast<const Matrix3x4*>(value_.ptr_)) == rhs : false; }
    /// Test for equality with a Matrix4. To return true, both the type and value must match.
    bool operator == (const Matrix4& rhs) const { return type_ == VAR_MATRIX4 ? *(reinterpret_cast<const Matrix4*>(value_.ptr_)) == rhs : false; }

    /// Test for inequality with another variant.
    bool operator != (const Variant& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an integer.
    bool operator != (int rhs) const { return !(*this == rhs); }
    /// Test for inequality with an unsigned integer.
    bool operator != (unsigned rhs) const { return !(*this == rhs); }
    /// Test for inequality with an 64 bit integer.
    bool operator !=(long long rhs) const { return !(*this == rhs); }
    /// Test for inequality with an unsigned 64 bit integer.
    bool operator !=(uint64_t rhs) const { return !(*this == rhs); }
    /// Test for inequality with a bool.
    bool operator != (bool rhs) const { return !(*this == rhs); }
    /// Test for inequality with a float.
    bool operator != (float rhs) const { return !(*this == rhs); }
    /// Test for inequality with a double.
    bool operator !=(double rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Vector2.
    bool operator != (const Vector2& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Vector3.
    bool operator != (const Vector3& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an Vector4.
    bool operator != (const Vector4& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Quaternion.
    bool operator != (const Quaternion& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a string.
    bool operator != (const QString& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a buffer.
    bool operator != (const std::vector<unsigned char>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a pointer.
    bool operator != (void* rhs) const { return !(*this == rhs); }
    /// Test for inequality with a resource reference.
    bool operator != (const ResourceRef& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a resource reference list.
    bool operator != (const ResourceRefList& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a variant vector.
    bool operator != (const VariantVector& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a string vector.
    bool operator !=(const QStringList& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a variant map.
    bool operator != (const VariantMap& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a rect.
    bool operator !=(const Rect& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an integer rect.
    bool operator != (const IntRect& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an IntVector2.
    bool operator != (const IntVector2& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an IntVector3.
    bool operator !=(const IntVector3& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a StringHash.
    bool operator != (const StringHash& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a RefCounted pointer.
    bool operator != (RefCounted* rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Matrix3.
    bool operator != (const Matrix3& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Matrix3x4.
    bool operator != (const Matrix3x4& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Matrix4.
    bool operator != (const Matrix4& rhs) const { return !(*this == rhs); }

    /// Set from typename and value strings. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(const QString& type, const QString& value);
    /// Set from typename and value strings. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(const char* type, const char* value);
    /// Set from type and value string. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(VariantType type, const QString& value);
    /// Set from type and value string. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(VariantType type, const char* value);
    /// Set buffer type from a memory area.
    void SetBuffer(const void* data, unsigned size);

    /// Return int or zero on type mismatch. Floats and doubles are converted.
    int GetInt() const
    {
        if (type_ == VAR_INT)
            return value_.int_;
        else if (type_ == VAR_FLOAT)
            return (int)value_.float_;
        else if (type_ == VAR_DOUBLE)
            return (int)*reinterpret_cast<const double*>(&value_);
        else
            return 0;
    }
    /// Return 64 bit int or zero on type mismatch. Floats and doubles are converted.
    long long GetInt64() const
    {
        if (type_ == VAR_INT64)
            return *(reinterpret_cast<const long long*>(&value_));
        else if (type_ == VAR_INT)
            return value_.int_;
        else if (type_ == VAR_FLOAT)
            return (long long)value_.float_;
        else if (type_ == VAR_DOUBLE)
            return (long long)*reinterpret_cast<const double*>(&value_);
        else
            return 0;
    }

    /// Return unsigned 64 bit int or zero on type mismatch. Floats and doubles are converted.
    uint64_t GetUInt64() const
    {
        if (type_ == VAR_INT64)
            return *(reinterpret_cast<const uint64_t*>(&value_));
        else if (type_ == VAR_INT)
            return static_cast<uint64_t>(value_.int_);
        else if (type_ == VAR_FLOAT)
            return (uint64_t)value_.float_;
        else if (type_ == VAR_DOUBLE)
            return (uint64_t)*reinterpret_cast<const double*>(&value_);
        else
            return 0;
    }

    /// Return unsigned int or zero on type mismatch. Floats and doubles are converted.
    unsigned GetUInt() const
    {
        if (type_ == VAR_INT)
            return value_.int_;
        else if (type_ == VAR_FLOAT)
            return (unsigned)value_.float_;
        else if (type_ == VAR_DOUBLE)
            return (unsigned)*reinterpret_cast<const double*>(&value_);
        else
            return 0;
    }
    /// Return StringHash or zero on type mismatch.
    StringHash GetStringHash() const { return StringHash(GetUInt()); }
    /// Return bool or false on type mismatch.
    bool GetBool() const { return type_ == VAR_BOOL ? value_.bool_ : false; }
    /// Return float or zero on type mismatch. Ints and doubles are converted.
    float GetFloat() const
    {
        if (type_ == VAR_FLOAT)
            return value_.float_;
        else if (type_ == VAR_DOUBLE)
            return (float)*reinterpret_cast<const double*>(&value_);
        else if (type_ == VAR_INT)
            return (float)value_.int_;
        else
            return 0.0f;
    }
    /// Return double or zero on type mismatch. Ints and floats are converted.
    double GetDouble() const
    {
        if (type_ == VAR_DOUBLE)
            return *reinterpret_cast<const double*>(&value_);
        else if (type_ == VAR_FLOAT)
            return (double)value_.float_;
        else if (type_ == VAR_INT)
            return (double)value_.int_;
        else
            return 0.0;
    }
    /// Return Vector2 or zero on type mismatch.
    const Vector2& GetVector2() const { return type_ == VAR_VECTOR2 ? *reinterpret_cast<const Vector2*>(&value_) : Vector2::ZERO; }
    /// Return Vector3 or zero on type mismatch.
    const Vector3& GetVector3() const { return type_ == VAR_VECTOR3 ? *reinterpret_cast<const Vector3*>(&value_) : Vector3::ZERO; }
    /// Return Vector4 or zero on type mismatch.
    const Vector4& GetVector4() const { return type_ == VAR_VECTOR4 ? *reinterpret_cast<const Vector4*>(&value_) : Vector4::ZERO; }
    /// Return quaternion or identity on type mismatch.
    const Quaternion& GetQuaternion() const { return type_ == VAR_QUATERNION ? *reinterpret_cast<const Quaternion*>(&value_) : Quaternion::IDENTITY; }
    /// Return color or default on type mismatch. Vector4 is aliased to Color if necessary.
    const Color& GetColor() const { return (type_ == VAR_COLOR || type_ == VAR_VECTOR4) ? *reinterpret_cast<const Color*>(&value_) : Color::WHITE; }
    /// Return string or empty on type mismatch.
    const QString &GetString() const {
        return type_ == VAR_STRING ? *reinterpret_cast<const QString*>(&value_) : s_dummy; }
    /// Return buffer or empty on type mismatch.
    const std::vector<unsigned char>& GetBuffer() const { return type_ == VAR_BUFFER ? *reinterpret_cast<const std::vector<unsigned char>*>(&value_) : emptyBuffer; }

    /// Return void pointer or null on type mismatch. RefCounted pointer will be converted.
    void* GetVoidPtr() const
    {
        if (type_ == VAR_VOIDPTR)
            return value_.ptr_;
        else if (type_ == VAR_PTR)
            return *reinterpret_cast<const WeakPtr<RefCounted>*>(&value_);
        else
            return nullptr;
    }

    /// Return a resource reference or empty on type mismatch.
    const ResourceRef& GetResourceRef() const { return type_ == VAR_RESOURCEREF ? *reinterpret_cast<const ResourceRef*>(&value_) : emptyResourceRef; }
    /// Return a resource reference list or empty on type mismatch.
    const ResourceRefList& GetResourceRefList() const { return type_ == VAR_RESOURCEREFLIST ? *reinterpret_cast<const ResourceRefList*>(&value_) : emptyResourceRefList; }
    /// Return a variant vector or empty on type mismatch.
    const VariantVector& GetVariantVector() const { return type_ == VAR_VARIANTVECTOR ? *reinterpret_cast<const VariantVector*>(&value_) : emptyVariantVector; }
    /// Return a vector  buffer or empty on type mismatch.
    const std::vector<unsigned char> &GetVectorBuffer() const { return type_ == VAR_BUFFER ? *reinterpret_cast<const std::vector<unsigned char>*>(&value_) : emptyBuffer; }

    /// Return a string vector or empty on type mismatch.
    const QStringList& GetStringVector() const
    {
        return type_ == VAR_STRINGVECTOR ? *reinterpret_cast<const QStringList*>(&value_) : emptyStringVector;
    }
    /// Return a variant map or empty on type mismatch.
    const VariantMap& GetVariantMap() const { return type_ == VAR_VARIANTMAP ? *reinterpret_cast<const VariantMap*>(&value_) : emptyVariantMap; }
    /// Return a rect or empty on type mismatch.
    const Rect& GetRect() const { return type_ == VAR_RECT ? *reinterpret_cast<const Rect*>(&value_) : Rect::ZERO; }
    /// Return an integer rect or empty on type mismatch.
    const IntRect& GetIntRect() const { return type_ == VAR_INTRECT ? *reinterpret_cast<const IntRect*>(&value_) : IntRect::ZERO; }
    /// Return an IntVector2 or empty on type mismatch.
    const IntVector2& GetIntVector2() const { return type_ == VAR_INTVECTOR2 ? *reinterpret_cast<const IntVector2*>(&value_) : IntVector2::ZERO; }
    /// Return an IntVector3 or empty on type mismatch.
    const IntVector3& GetIntVector3() const    {        return type_ == VAR_INTVECTOR3 ? *reinterpret_cast<const IntVector3*>(&value_) : IntVector3::ZERO;    }
    /// Return a RefCounted pointer or null on type mismatch. Will return null if holding a void pointer, as it can not be safely verified that the object is a RefCounted.
    RefCounted* GetPtr() const { return type_ == VAR_PTR ? *reinterpret_cast<const WeakPtr<RefCounted>*>(&value_) : nullptr; }
    /// Return a Matrix3 or identity on type mismatch.
    const Matrix3& GetMatrix3() const { return type_ == VAR_MATRIX3 ? *(reinterpret_cast<const Matrix3*>(value_.ptr_)) : Matrix3::IDENTITY; }
    /// Return a Matrix3x4 or identity on type mismatch.
    const Matrix3x4& GetMatrix3x4() const { return type_ == VAR_MATRIX3X4 ? *(reinterpret_cast<const Matrix3x4*>(value_.ptr_)) : Matrix3x4::IDENTITY; }
    /// Return a Matrix4 or identity on type mismatch.
    const Matrix4& GetMatrix4() const { return type_ == VAR_MATRIX4 ? *(reinterpret_cast<const Matrix4*>(value_.ptr_)) : Matrix4::IDENTITY; }
    /// Return value's type.
    VariantType GetType() const { return type_; }
    /// Return value's type name.
    QString GetTypeName() const;
    /// Convert value to string. Pointers are returned as null, and VariantBuffer or VariantMap are not supported and return empty.
    QString ToString() const;
    /// Return true when the variant value is considered zero according to its actual type.
    bool IsZero() const;
    /// Return true when the variant is empty (i.e. not initialized yet).
    bool IsEmpty() const { return type_ == VAR_NONE; }
    /// Return the value, template version.
    template <class T> T Get() const;

    /// Return a pointer to a modifiable buffer or null on type mismatch.
    std::vector<unsigned char>* GetBufferPtr() { return type_ == VAR_BUFFER ? reinterpret_cast<std::vector<unsigned char>*>(&value_) : nullptr; }
    /// Return a pointer to a modifiable variant vector or null on type mismatch.
    VariantVector* GetVariantVectorPtr() { return type_ == VAR_VARIANTVECTOR ? reinterpret_cast<VariantVector*>(&value_) : nullptr; }
    /// Return a pointer to a modifiable string vector or null on type mismatch.
    QStringList* GetStringVectorPtr() { return type_ == VAR_STRINGVECTOR ? reinterpret_cast<QStringList*>(&value_) : nullptr; }
    /// Return a pointer to a modifiable variant map or null on type mismatch.
    VariantMap* GetVariantMapPtr() { return type_ == VAR_VARIANTMAP ? reinterpret_cast<VariantMap*>(&value_) : nullptr; }

    /// Return name for variant type.
    static QString GetTypeName(VariantType type);
    /// Return variant type from type name.
    static VariantType GetTypeFromName(const QString& typeName);
    /// Return variant type from type name.
    static VariantType GetTypeFromName(const char* typeName);

    /// Empty variant.
    static const Variant EMPTY;
    /// Empty buffer.
    static const std::vector<unsigned char> emptyBuffer;
    /// Empty resource reference.
    static const ResourceRef emptyResourceRef;
    /// Empty resource reference list.
    static const ResourceRefList emptyResourceRefList;
    /// Empty variant map.
    static const VariantMap emptyVariantMap;
    /// Empty variant vector.
    static const VariantVector emptyVariantVector;
    /// Empty string vector.
    static const QStringList emptyStringVector;

private:
    /// Set new type and allocate/deallocate memory as necessary.
    void SetType(VariantType newType);

    /// Variant type.
    VariantType type_ = VAR_NONE;
    /// Variant value.
    VariantValue value_;
};
static_assert(sizeof(VariantValue)>=sizeof(VariantMap),"Variant value must be large enough to hold VariantVector");
static_assert(sizeof(VariantValue)>=sizeof(VariantVector),"Variant value must be large enough to hold VariantVector");

/// Return variant type from type.
template<typename T> constexpr VariantType GetVariantType();

// Return variant type from concrete types
template<> constexpr VariantType GetVariantType<int>() { return VAR_INT; }
template<> constexpr VariantType GetVariantType<unsigned>() { return VAR_INT; }
template<> constexpr VariantType GetVariantType<long long>() { return VAR_INT64; }
template<> constexpr VariantType GetVariantType<uint64_t>() { return VAR_INT64; }
template<> constexpr VariantType GetVariantType<bool>() { return VAR_BOOL; }
template<> constexpr VariantType GetVariantType<float>() { return VAR_FLOAT; }
template<> constexpr VariantType GetVariantType<double>() { return VAR_DOUBLE; }
template<> constexpr VariantType GetVariantType<Vector2>() { return VAR_VECTOR2; }
template<> constexpr VariantType GetVariantType<Vector3>() { return VAR_VECTOR3; }
template<> constexpr VariantType GetVariantType<Vector4>() { return VAR_VECTOR4; }
template<> constexpr VariantType GetVariantType<Quaternion>() { return VAR_QUATERNION; }
template<> constexpr VariantType GetVariantType<Color>() { return VAR_COLOR; }
template<> constexpr VariantType GetVariantType<QString>() { return VAR_STRING; }
template<> constexpr VariantType GetVariantType<StringHash>() { return VAR_INT; }
template<> constexpr VariantType GetVariantType<std::vector<unsigned char> >() { return VAR_BUFFER; }
template<> constexpr VariantType GetVariantType<ResourceRef>() { return VAR_RESOURCEREF; }
template<> constexpr VariantType GetVariantType<ResourceRefList>() { return VAR_RESOURCEREFLIST; }
template<> constexpr VariantType GetVariantType<VariantVector>() { return VAR_VARIANTVECTOR; }
template<> constexpr VariantType GetVariantType<QStringList >() { return VAR_STRINGVECTOR; }
template<> constexpr VariantType GetVariantType<VariantMap>() { return VAR_VARIANTMAP; }
template<> constexpr VariantType GetVariantType<Rect>() { return VAR_RECT; }
template<> constexpr VariantType GetVariantType<IntRect>() { return VAR_INTRECT; }
template<> constexpr VariantType GetVariantType<IntVector2>() { return VAR_INTVECTOR2; }
template<> constexpr VariantType GetVariantType<IntVector3>() { return VAR_INTVECTOR3; }
template<> constexpr VariantType GetVariantType<Matrix3>() { return VAR_MATRIX3; }
template<> constexpr VariantType GetVariantType<Matrix3x4>() { return VAR_MATRIX3X4; }
template<> constexpr VariantType GetVariantType<Matrix4>() { return VAR_MATRIX4; }
//template<> LUTEFISK3D_EXPORT Rect Variant::Get<Rect>() const;
//template<> LUTEFISK3D_EXPORT IntRect Variant::Get<IntRect>() const;

static_assert(sizeof(VariantValue)>=sizeof(QString),"Variant value must be large enough to hold VariantMap");
}
