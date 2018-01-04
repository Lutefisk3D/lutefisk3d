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

#include "Lutefisk3D/Math/Color.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Container/Str.h"

#include <QString>
#include <QStringList>
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
    // Add new types here
    VAR_CUSTOM_HEAP,
    VAR_CUSTOM_STACK,
    MAX_VAR_TYPES
};
/// Typed resource reference.
struct LUTEFISK3D_EXPORT ResourceRef
{
    StringHash type_; //!< Object type.
    QString    name_; //!< Object name.

    /// Test for equality with another reference.
    bool operator == (const ResourceRef& rhs) const { return type_ == rhs.type_ && name_ == rhs.name_; }
    /// Test for inequality with another reference.
    bool operator != (const ResourceRef& rhs) const { return type_ != rhs.type_ || name_ != rhs.name_; }
};
/// %List of typed resource references.
struct LUTEFISK3D_EXPORT ResourceRefList
{
    StringHash type_; //!< Object type.
    std::vector<QString> names_; //!< List of object names.
    /// Test for equality with another reference list.
    bool operator == (const ResourceRefList& rhs) const { return type_ == rhs.type_ && names_ == rhs.names_; }
    /// Test for inequality with another reference list.
    bool operator != (const ResourceRefList& rhs) const { return type_ != rhs.type_ || names_ != rhs.names_; }
};
class Variant;
/// Vector of variants.
using VariantVector = std::vector<Variant>;
/// Map of variants.
using VariantMap = HashMap<StringHash, Variant> ;

/// Custom variant value. This type is not abstract to store it in the VariantValue by value.
class CustomVariantValue
{
    // GetValuePtr expects that CustomVariantValue is always convertible to CustomVariantValueImpl<T>.
    template <class T> friend class CustomVariantValueImpl;

private:
    /// Construct from type info.
    explicit CustomVariantValue(const std::type_info& typeInfo) : typeInfo_(typeInfo) {}

public:
    /// Construct empty.
    CustomVariantValue() : typeInfo_(typeid(void)) { }      // NOLINT
    /// Destruct.
    virtual ~CustomVariantValue() = default;

    /// Get the type info.
    const std::type_info& GetTypeInfo() const { return typeInfo_; }
    /// Return whether the specified type is stored.
    template <class T> bool IsType() const { return GetTypeInfo() == typeid(T); }
    /// Return pointer to value of the specified type. Return null pointer if type does not match.
    template <class T> T* GetValuePtr();
    /// Return const pointer to value of the specified type. Return null pointer if type does not match.
    template <class T> const T* GetValuePtr() const;

    /// Assign value.
    virtual bool Assign(const CustomVariantValue& rhs) { return false; }
    /// Clone.
    virtual CustomVariantValue* Clone() const { return nullptr; }
    /// Placement clone.
    virtual void Clone(void* dest) const { }
    /// Get size.
    virtual unsigned GetSize() const { return sizeof(CustomVariantValue); }

    /// Compare to another custom value.
    virtual bool Compare(const CustomVariantValue& rhs) const { (void)rhs; return false; }
    /// Compare to zero.
    virtual bool IsZero() const { return false; }
    /// Convert custom value to string.
    virtual QString ToString() const { return QString(); }

private:
    /// Type info.
    const std::type_info& typeInfo_;
};
/// Custom variant value type traits. Specialize the template to implement custom type behavior.
template <class T> struct CustomVariantValueTraits
{
    /// Compare types.
    static bool Compare(const T& lhs, const T& rhs) { (void)lhs, rhs; return false; }
    /// Check whether the value is zero.
    static bool IsZero(const T& value) { (void)value; return false; }
    /// Convert type to string.
    static QString ToString(const T& value) { (void)value; return QString(); }
};
/// Custom variant value implementation.
template <class T> class CustomVariantValueImpl final : public CustomVariantValue
{
public:
    /// This class name.
    using ClassName = CustomVariantValueImpl<T>;
    /// Type traits.
    using Traits = CustomVariantValueTraits<T>;
    /// Construct from value.
    explicit CustomVariantValueImpl(const T& value) : CustomVariantValue(typeid(T)), value_(value) {}
    /// Set value.
    void SetValue(const T& value) { value_ = value; }
    /// Get value.
    T& GetValue() { return value_; }
    /// Get const value.
    const T& GetValue() const { return value_; }

    /// Assign value.
    bool Assign(const CustomVariantValue& rhs) override
    {
        if (const T* rhsValue = rhs.GetValuePtr<T>())
        {
            SetValue(*rhsValue);
            return true;
        }
        return false;
    }
    /// Clone.
    CustomVariantValue* Clone() const override { return new ClassName(value_); }
    /// Placement clone.
    void Clone(void* dest) const override { new (dest) ClassName(value_); }
    /// Get size.
    unsigned GetSize() const override { return sizeof(ClassName); }

    /// Compare to another custom value.
    bool Compare(const CustomVariantValue& rhs) const override
    {
        const T* rhsValue = rhs.GetValuePtr<T>();
        return rhsValue && Traits::Compare(value_, *rhsValue);
    }
    /// Compare to zero.
    bool IsZero() const override { return Traits::IsZero(value_);}
    /// Convert custom value to string.
    QString ToString() const override { return Traits::ToString(value_); }

private:
    /// Value.
    T value_;
};
/// Make custom variant value.
template <typename T> CustomVariantValueImpl<T> MakeCustomValue(const T& value) { return CustomVariantValueImpl<T>(value); }

/// Size of variant value. 16 bytes on 32-bit platform, 32 bytes on 64-bit platform.
static const unsigned VARIANT_VALUE_SIZE = sizeof(void*) * 4;

/// Union for the possible variant values. Objects exceeding the VARIANT_VALUE_SIZE are allocated on the heap.
union VariantValue
{
    unsigned char storage_[VARIANT_VALUE_SIZE];
    int int_;
    bool bool_;
    float float_;
    double double_;
    long long int64_;
    void* voidPtr_;
    WeakPtr<RefCounted> weakPtr_;
    Vector2 vector2_;
    Vector3 vector3_;
    Vector4 vector4_;
    Rect rect_;
    IntVector2 intVector2_;
    IntVector3 intVector3_;
    IntRect intRect_;
    Matrix3* matrix3_;
    Matrix3x4* matrix3x4_;
    Matrix4* matrix4_;
    Quaternion quaternion_;
    Color color_;
    QString string_;
    QStringList stringVector_;
    VariantVector *variantVector_;
    VariantMap *variantMap_;
    std::vector<uint8_t> buffer_;
    ResourceRef resourceRef_;
    ResourceRefList resourceRefList_;
    CustomVariantValue* customValueHeap_;
    CustomVariantValue customValueStack_;
    /// Construct uninitialized.
    VariantValue() { }      // NOLINT
    /// Non-copyable.
    VariantValue(const VariantValue& value) = delete;
    /// Destruct.
    ~VariantValue() { }     // NOLINT
};

static_assert(sizeof(VariantValue)>=sizeof(QStringList),"Variant value to small");
struct ResourceRef;
struct ResourceRefList;

class Variant;

/// Variable that supports a fixed set of types.
class LUTEFISK3D_EXPORT Variant
{
public:
    /// Construct empty.
    Variant() = default;

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
    Variant(const std::vector<uint8_t>& value)
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

    /// Construct from custom value.
    template <class T>
    Variant(const CustomVariantValueImpl<T>& value)     // NOLINT
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
        value_.int64_ = rhs;
        return *this;
    }

    /// Assign from unsigned 64 bit integer.
    Variant& operator =(uint64_t rhs)
    {
        SetType(VAR_INT64);
        value_.int64_ = static_cast<long long>(rhs);
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
        value_.double_ = rhs;
        return *this;
    }

    /// Assign from a Vector2.
    Variant& operator = (const Vector2& rhs)
    {
        SetType(VAR_VECTOR2);
        value_.vector2_ = rhs;
        return *this;
    }

    /// Assign from a Vector3.
    Variant& operator = (const Vector3& rhs)
    {
        SetType(VAR_VECTOR3);
        value_.vector3_ = rhs;
        return *this;
    }

    /// Assign from a Vector4.
    Variant& operator = (const Vector4& rhs)
    {
        SetType(VAR_VECTOR4);
        value_.vector4_ = rhs;
        return *this;
    }

    /// Assign from a quaternion.
    Variant& operator = (const Quaternion& rhs)
    {
        SetType(VAR_QUATERNION);
        value_.quaternion_ = rhs;
        return *this;
    }

    /// Assign from a color.
    Variant& operator = (const Color& rhs)
    {
        SetType(VAR_COLOR);
        value_.color_ = rhs;
        return *this;
    }

    /// Assign from a string.
    Variant& operator = (const QString& rhs)
    {
        SetType(VAR_STRING);
        value_.string_ = rhs;
        return *this;
    }

    /// Assign from a C string.
    Variant& operator = (const char* rhs)
    {
        SetType(VAR_STRING);
        value_.string_ = rhs;
        return *this;
    }

    /// Assign from a buffer.
    Variant& operator = (const std::vector<unsigned char>& rhs)
    {
        SetType(VAR_BUFFER);
        value_.buffer_ = rhs;
        return *this;
    }

    /// Assign from a void pointer.
    Variant& operator = (void* rhs)
    {
        SetType(VAR_VOIDPTR);
        value_.voidPtr_ = rhs;
        return *this;
    }

    /// Assign from a resource reference.
    Variant& operator =(const ResourceRef& rhs)
    {
        SetType(VAR_RESOURCEREF);
        value_.resourceRef_ = rhs;
        return *this;
    }

    /// Assign from a resource reference list.
    Variant& operator =(const ResourceRefList& rhs)
    {
        SetType(VAR_RESOURCEREFLIST);
        value_.resourceRefList_ = rhs;
        return *this;
    }

    /// Assign from a variant vector.
    Variant& operator = (const VariantVector& rhs)
    {
        SetType(VAR_VARIANTVECTOR);
        value_.variantVector_ = new VariantVector(rhs);
        return *this;
    }

    // Assign from a string vector.
    Variant& operator =(const QStringList & rhs)
    {
        SetType(VAR_STRINGVECTOR);
        value_.stringVector_ = rhs;
        return *this;
    }
    /// Assign from a variant map.
    Variant& operator = (const VariantMap& rhs)
    {
        SetType(VAR_VARIANTMAP);
        value_.variantMap_ = new VariantMap(rhs);
        return *this;
    }

    /// Assign from a rect.
    Variant& operator =(const Rect& rhs)
    {
        SetType(VAR_RECT);
        value_.rect_ = rhs;
        return *this;
    }

    /// Assign from an integer rect.
    Variant& operator = (const IntRect& rhs)
    {
        SetType(VAR_INTRECT);
        value_.intRect_ = rhs;
        return *this;
    }

    /// Assign from an IntVector2.
    Variant& operator = (const IntVector2& rhs)
    {
        SetType(VAR_INTVECTOR2);
        value_.intVector2_ = rhs;
        return *this;
    }
    /// Assign from an IntVector3.
    Variant& operator =(const IntVector3& rhs)
    {
        SetType(VAR_INTVECTOR3);
        value_.intVector3_ = rhs;
        return *this;
    }
    /// Assign from a RefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant& operator = (RefCounted* rhs)
    {
        SetType(VAR_PTR);
        value_.weakPtr_ = rhs;
        return *this;
    }

    /// Assign from a Matrix3.
    Variant& operator = (const Matrix3& rhs)
    {
        SetType(VAR_MATRIX3);
        *value_.matrix3_ = rhs;
        return *this;
    }

    /// Assign from a Matrix3x4.
    Variant& operator = (const Matrix3x4& rhs)
    {
        SetType(VAR_MATRIX3X4);
        *value_.matrix3x4_ = rhs;
        return *this;
    }

    /// Assign from a Matrix4.
    Variant& operator = (const Matrix4& rhs)
    {
        SetType(VAR_MATRIX4);
        *value_.matrix4_ = rhs;
        return *this;
    }

    /// Assign from custom value.
    template <class T>
    Variant& operator =(const CustomVariantValueImpl<T>& value)
    {
        SetCustomVariantValue(value);
        return *this;
    }

    /// Test for equality with another variant.
    bool operator == (const Variant& rhs) const;
    /// Test for equality with an integer. To return true, both the type and value must match.
    bool operator == (int rhs) const { return type_ == VAR_INT ? value_.int_ == rhs : false; }
    /// Test for equality with an unsigned 64 bit integer. To return true, both the type and value must match.
    bool operator ==(unsigned rhs) const { return type_ == VAR_INT ? value_.int_ == static_cast<int>(rhs) : false; }

    /// Test for equality with an 64 bit integer. To return true, both the type and value must match.
    bool operator ==(long long rhs) const { return type_ == VAR_INT64 ? value_.int64_ == rhs : false; }

    /// Test for equality with an unsigned integer. To return true, both the type and value must match.
    bool operator ==(uint64_t rhs) const { return type_ == VAR_INT64 ? value_.int64_ == static_cast<long long>(rhs) : false; }

    /// Test for equality with a bool. To return true, both the type and value must match.
    bool operator == (bool rhs) const { return type_ == VAR_BOOL ? value_.bool_ == rhs : false; }
    /// Test for equality with a float. To return true, both the type and value must match.
    bool operator == (float rhs) const { return type_ == VAR_FLOAT ? value_.float_ == rhs : false; }
    /// Test for equality with a double. To return true, both the type and value must match.
    bool operator ==(double rhs) const { return type_ == VAR_DOUBLE ? value_.double_ == rhs : false; }
    /// Test for equality with a Vector2. To return true, both the type and value must match.
    bool operator ==(const Vector2& rhs) const
    {
        return type_ == VAR_VECTOR2 ? value_.vector2_ == rhs : false;
    }
    /// Test for equality with a Vector3. To return true, both the type and value must match.
    bool operator ==(const Vector3& rhs) const
    {
        return type_ == VAR_VECTOR3 ? value_.vector3_ == rhs : false;
    }
    /// Test for equality with a Vector4. To return true, both the type and value must match.
    bool operator ==(const Vector4& rhs) const
    {
        return type_ == VAR_VECTOR4 ? value_.vector4_ == rhs : false;
    }
    /// Test for equality with a quaternion. To return true, both the type and value must match.
    bool operator ==(const Quaternion& rhs) const
    {
        return type_ == VAR_QUATERNION ? value_.quaternion_ == rhs : false;
    }
    /// Test for equality with a color. To return true, both the type and value must match.
    bool operator ==(const Color& rhs) const
    {
        return type_ == VAR_COLOR ? value_.color_ == rhs : false;
    }
    /// Test for equality with a string. To return true, both the type and value must match.
    bool operator ==(const QString& rhs) const
    {
        return type_ == VAR_STRING ? value_.string_ == rhs : false;
    }
    /// Test for equality with a buffer. To return true, both the type and value must match.
    bool operator == (const std::vector<unsigned char>& rhs) const;

    /// Test for equality with a void pointer. To return true, both the type and value must match, with the exception that a RefCounted pointer is also allowed.
    bool operator == (void* rhs) const
    {
        if (type_ == VAR_VOIDPTR)
            return value_.voidPtr_ == rhs;
        else if (type_ == VAR_PTR)
            return value_.weakPtr_ == rhs;
        else
            return false;
    }

    /// Test for equality with a resource reference. To return true, both the type and value must match.
    bool operator ==(const ResourceRef& rhs) const
    {
        return type_ == VAR_RESOURCEREF ? value_.resourceRef_ == rhs : false;
    }
    /// Test for equality with a resource reference list. To return true, both the type and value must match.
    bool operator ==(const ResourceRefList& rhs) const
    {
        return type_ == VAR_RESOURCEREFLIST ? value_.resourceRefList_ == rhs : false;
    }
    /// Test for equality with a variant vector. To return true, both the type and value must match.
    bool operator ==(const VariantVector& rhs) const
    {
        return type_ == VAR_VARIANTVECTOR ? *value_.variantVector_ == rhs : false;
    }
   /// Test for equality with a string vector. To return true, both the type and value must match.
    bool operator ==(const QStringList &rhs) const
    {
        return type_ == VAR_STRINGVECTOR ? value_.stringVector_ == rhs : false;
    }
    /// Test for equality with a variant map. To return true, both the type and value must match.
    bool operator ==(const VariantMap& rhs) const
    {
        return type_ == VAR_VARIANTMAP ? *value_.variantMap_ == rhs : false;
    }
    /// Test for equality with a rect. To return true, both the type and value must match.
    bool operator ==(const Rect& rhs) const
    {
        return type_ == VAR_RECT ? value_.rect_ == rhs : false;
    }
    /// Test for equality with an integer rect. To return true, both the type and value must match.
    bool operator ==(const IntRect& rhs) const
    {
        return type_ == VAR_INTRECT ? value_.intRect_ == rhs : false;
    }
    /// Test for equality with an IntVector2. To return true, both the type and value must match.
    bool operator ==(const IntVector2& rhs) const
    {
        return type_ == VAR_INTVECTOR2 ? value_.intVector2_ == rhs : false;
    }
    /// Test for equality with an IntVector3. To return true, both the type and value must match.
    bool operator ==(const IntVector3& rhs) const
    {
        return type_ == VAR_INTVECTOR3 ? value_.intVector3_ == rhs : false;
    }
    /// Test for equality with a StringHash. To return true, both the type and value must match.
    bool operator == (const StringHash& rhs) const { return type_ == VAR_INT ? (unsigned)value_.int_ == rhs.Value() : false; }

    /// Test for equality with a RefCounted pointer. To return true, both the type and value must match, with the exception that void pointer is also allowed.
    bool operator == (RefCounted* rhs) const
    {
        if (type_ == VAR_PTR)
            return value_.weakPtr_ == rhs;
        else if (type_ == VAR_VOIDPTR)
            return value_.voidPtr_ == rhs;
        else
            return false;
    }

    /// Test for equality with a Matrix3. To return true, both the type and value must match.
    bool operator ==(const Matrix3& rhs) const
    {
        return type_ == VAR_MATRIX3 ? *value_.matrix3_ == rhs : false;
    }
    /// Test for equality with a Matrix3x4. To return true, both the type and value must match.
    bool operator ==(const Matrix3x4& rhs) const
    {
        return type_ == VAR_MATRIX3X4 ? *value_.matrix3x4_ == rhs : false;
    }
    /// Test for equality with a Matrix4. To return true, both the type and value must match.
    bool operator ==(const Matrix4& rhs) const
    {
        return type_ == VAR_MATRIX4 ? *value_.matrix4_ == rhs : false;
    }

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
    /// Set custom value.
    void SetCustomVariantValue(const CustomVariantValue& value);
    /// Set custom value.
    template <class T> void SetCustom(const T& value) { SetCustomVariantValue(MakeCustomValue<T>(value)); }

    /// Return int or zero on type mismatch. Floats and doubles are converted.
    int GetInt() const
    {
        if (type_ == VAR_INT)
            return value_.int_;
        else if (type_ == VAR_FLOAT)
            return static_cast<int>(value_.float_);
        else if (type_ == VAR_DOUBLE)
            return static_cast<int>(value_.double_);
        else
            return 0;
    }
    /// Return 64 bit int or zero on type mismatch. Floats and doubles are converted.
    long long GetInt64() const
    {
        if (type_ == VAR_INT64)
            return value_.int64_;
        else if (type_ == VAR_INT)
            return value_.int_;
        else if (type_ == VAR_FLOAT)
            return static_cast<long long>(value_.float_);
        else if (type_ == VAR_DOUBLE)
            return static_cast<long long>(value_.double_);
        else
            return 0;
    }

    /// Return unsigned 64 bit int or zero on type mismatch. Floats and doubles are converted.
    uint64_t GetUInt64() const
    {
        if (type_ == VAR_INT64)
            return static_cast<uint64_t>(value_.int64_);
        else if (type_ == VAR_INT)
            return static_cast<uint64_t>(value_.int_);
        else if (type_ == VAR_FLOAT)
            return static_cast<uint64_t>(value_.float_);
        else if (type_ == VAR_DOUBLE)
            return static_cast<uint64_t>(value_.double_);
        else
            return 0;
    }

    /// Return unsigned int or zero on type mismatch. Floats and doubles are converted.
    unsigned GetUInt() const
    {
        if (type_ == VAR_INT)
            return static_cast<unsigned>(value_.int_);
        else if (type_ == VAR_FLOAT)
            return static_cast<unsigned>(value_.float_);
        else if (type_ == VAR_DOUBLE)
            return static_cast<unsigned>(value_.double_);
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
        if (type_ == VAR_DOUBLE)
            return static_cast<float>(value_.double_);
        if (type_ == VAR_INT)
            return static_cast<float>(value_.int_);
        if (type_ == VAR_INT64)
            return static_cast<float>(value_.int64_);
        return 0.0f;
    }
    /// Return double or zero on type mismatch. Ints and floats are converted.
    double GetDouble() const
    {
        if (type_ == VAR_DOUBLE)
            return value_.double_;
        else if (type_ == VAR_FLOAT)
            return value_.float_;
        else if (type_ == VAR_INT)
            return static_cast<double>(value_.int_);
        else if (type_ == VAR_INT64)
            return static_cast<double>(value_.int64_);
        else
            return 0.0;
    }
    /// Return Vector2 or zero on type mismatch.
    const Vector2& GetVector2() const { return type_ == VAR_VECTOR2 ? value_.vector2_ : Vector2::ZERO; }
    /// Return Vector3 or zero on type mismatch.
    const Vector3& GetVector3() const { return type_ == VAR_VECTOR3 ? value_.vector3_ : Vector3::ZERO; }
    /// Return Vector4 or zero on type mismatch.
    const Vector4& GetVector4() const { return type_ == VAR_VECTOR4 ? value_.vector4_ : Vector4::ZERO; }
    /// Return quaternion or identity on type mismatch.
    const Quaternion& GetQuaternion() const
    {
        return type_ == VAR_QUATERNION ? value_.quaternion_ : Quaternion::IDENTITY;
    }
    /// Return color or default on type mismatch. Vector4 is aliased to Color if necessary.
    const Color& GetColor() const { return (type_ == VAR_COLOR || type_ == VAR_VECTOR4) ? value_.color_ : Color::WHITE; }
    /// Return string or empty on type mismatch.
    QString GetString() const { return type_ == VAR_STRING ? value_.string_ : QString(); }
    /// Return buffer or empty on type mismatch.
    const std::vector<uint8_t>& GetBuffer() const
    {
        return type_ == VAR_BUFFER ? value_.buffer_ : emptyBuffer;
    }
    /// Return a vector  buffer or empty on type mismatch.
    const std::vector<unsigned char> &GetVectorBuffer() const { return type_ == VAR_BUFFER ? *reinterpret_cast<const std::vector<unsigned char>*>(&value_) : emptyBuffer; }
    /// Return void pointer or null on type mismatch. RefCounted pointer will be converted.
    void* GetVoidPtr() const
    {
        if (type_ == VAR_VOIDPTR)
            return value_.voidPtr_;
        else if (type_ == VAR_PTR)
            return value_.weakPtr_;
        else
            return nullptr;
    }

    /// Return a resource reference or empty on type mismatch.
    const ResourceRef& GetResourceRef() const
    {
        return type_ == VAR_RESOURCEREF ? value_.resourceRef_ : emptyResourceRef;
    }
    /// Return a resource reference list or empty on type mismatch.
    const ResourceRefList& GetResourceRefList() const
    {
        return type_ == VAR_RESOURCEREFLIST ? value_.resourceRefList_ : emptyResourceRefList;
    }
    /// Return a variant vector or empty on type mismatch.
    const VariantVector& GetVariantVector() const
    {
        return type_ == VAR_VARIANTVECTOR ? *value_.variantVector_ : emptyVariantVector;
    }
    /// Return a string vector or empty on type mismatch.
    const QStringList& GetStringVector() const
    {
        return type_ == VAR_STRINGVECTOR ? value_.stringVector_ : emptyStringVector;
    }
    /// Return a variant map or empty on type mismatch.
    const VariantMap& GetVariantMap() const
    {
        return type_ == VAR_VARIANTMAP ? *value_.variantMap_ : emptyVariantMap;
    }
    /// Return a rect or empty on type mismatch.
    const Rect& GetRect() const { return type_ == VAR_RECT ? value_.rect_ : Rect::ZERO; }
    /// Return an integer rect or empty on type mismatch.
    const IntRect& GetIntRect() const { return type_ == VAR_INTRECT ? value_.intRect_ : IntRect::ZERO; }
    /// Return an IntVector2 or empty on type mismatch.
    const IntVector2& GetIntVector2() const
    {
        return type_ == VAR_INTVECTOR2 ? value_.intVector2_ : IntVector2::ZERO;
    }
    /// Return an IntVector3 or empty on type mismatch.
    const IntVector3& GetIntVector3() const
    {
        return type_ == VAR_INTVECTOR3 ? value_.intVector3_ : IntVector3::ZERO;
    }
    /// Return a RefCounted pointer or null on type mismatch. Will return null if holding a void pointer, as it can not be safely verified that the object is a RefCounted.
    RefCounted* GetPtr() const
    {
        return type_ == VAR_PTR ? value_.weakPtr_ : nullptr;
    }
    /// Return a Matrix3 or identity on type mismatch.
    const Matrix3& GetMatrix3() const
    {
        return type_ == VAR_MATRIX3 ? *value_.matrix3_ : Matrix3::IDENTITY;
    }
    /// Return a Matrix3x4 or identity on type mismatch.
    const Matrix3x4& GetMatrix3x4() const
    {
        return type_ == VAR_MATRIX3X4 ? *value_.matrix3x4_ : Matrix3x4::IDENTITY;
    }
    /// Return a Matrix4 or identity on type mismatch.
    const Matrix4& GetMatrix4() const
    {
        return type_ == VAR_MATRIX4 ? *value_.matrix4_ : Matrix4::IDENTITY;
    }

    /// Return pointer to custom variant value.
    CustomVariantValue* GetCustomVariantValuePtr()
    {
        return const_cast<CustomVariantValue*>(const_cast<const Variant*>(this)->GetCustomVariantValuePtr());
    }

    /// Return const pointer to custom variant value.
    const CustomVariantValue* GetCustomVariantValuePtr() const
    {
        if (type_ == VAR_CUSTOM_HEAP)
            return value_.customValueHeap_;
        else if (type_ == VAR_CUSTOM_STACK)
            return &value_.customValueStack_;
        else
            return nullptr;
    }

    /// Return custom variant value or default-constructed on type mismatch.
    template <class T> T GetCustom() const
    {
        if (const CustomVariantValue* value = GetCustomVariantValuePtr())
        {
            if (value->IsType<T>())
                return *value->GetValuePtr<T>();
        }
        return T();
    }

    /// Return true if specified custom type is stored in the variant.
    template <class T> bool IsCustomType() const
    {
        if (const CustomVariantValue* custom = GetCustomVariantValuePtr())
            return custom->IsType<T>();
        else
            return false;
    }
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
    /// Return true when the variant stores custom type.
    bool IsCustom() const { return type_ == VAR_CUSTOM_HEAP || type_ == VAR_CUSTOM_STACK; }
    /// Return the value, template version.
    template <class T> T Get() const;

    /// Return a pointer to a modifiable buffer or null on type mismatch.
    std::vector<unsigned char>* GetBufferPtr()
    {
        return type_ == VAR_BUFFER ? &value_.buffer_ : nullptr;
    }
    /// Return a pointer to a modifiable variant vector or null on type mismatch.
    VariantVector* GetVariantVectorPtr() { return type_ == VAR_VARIANTVECTOR ? value_.variantVector_ : nullptr; }
    /// Return a pointer to a modifiable string vector or null on type mismatch.
    QStringList* GetStringVectorPtr() { return type_ == VAR_STRINGVECTOR ? &value_.stringVector_ : nullptr; }
    /// Return a pointer to a modifiable variant map or null on type mismatch.
    VariantMap* GetVariantMapPtr() { return type_ == VAR_VARIANTMAP ? value_.variantMap_ : nullptr; }

    /// Return a pointer to a modifiable custom variant value or null on type mismatch.
    template <class T> T* GetCustomPtr()
    {
        if (const CustomVariantValue* value = GetCustomVariantValuePtr())
        {
            if (value->IsType<T>())
                return value->GetValuePtr<T>();
        }
        return nullptr;
    }

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

// Specializations of Variant::Get<T>
template <> LUTEFISK3D_EXPORT int Variant::Get<int>() const;

template <> LUTEFISK3D_EXPORT unsigned Variant::Get<unsigned>() const;

template <> LUTEFISK3D_EXPORT long long Variant::Get<long long>() const;

template <> LUTEFISK3D_EXPORT uint64_t Variant::Get<uint64_t>() const;

template <> LUTEFISK3D_EXPORT StringHash Variant::Get<StringHash>() const;

template <> LUTEFISK3D_EXPORT bool Variant::Get<bool>() const;

template <> LUTEFISK3D_EXPORT float Variant::Get<float>() const;

template <> LUTEFISK3D_EXPORT double Variant::Get<double>() const;

template <> LUTEFISK3D_EXPORT const Vector2& Variant::Get<const Vector2&>() const;

template <> LUTEFISK3D_EXPORT const Vector3& Variant::Get<const Vector3&>() const;

template <> LUTEFISK3D_EXPORT const Vector4& Variant::Get<const Vector4&>() const;

template <> LUTEFISK3D_EXPORT const Quaternion& Variant::Get<const Quaternion&>() const;

template <> LUTEFISK3D_EXPORT const Color& Variant::Get<const Color&>() const;

template <> LUTEFISK3D_EXPORT const QString& Variant::Get<const QString&>() const;

template <> LUTEFISK3D_EXPORT const Rect& Variant::Get<const Rect&>() const;

template <> LUTEFISK3D_EXPORT const IntRect& Variant::Get<const IntRect&>() const;

template <> LUTEFISK3D_EXPORT const IntVector2& Variant::Get<const IntVector2&>() const;

template <> LUTEFISK3D_EXPORT const IntVector3& Variant::Get<const IntVector3&>() const;

template <> LUTEFISK3D_EXPORT const std::vector<unsigned char>& Variant::Get<const std::vector<unsigned char>&>() const;

template <> LUTEFISK3D_EXPORT void* Variant::Get<void*>() const;

template <> LUTEFISK3D_EXPORT RefCounted* Variant::Get<RefCounted*>() const;

template <> LUTEFISK3D_EXPORT const Matrix3& Variant::Get<const Matrix3&>() const;

template <> LUTEFISK3D_EXPORT const Matrix3x4& Variant::Get<const Matrix3x4&>() const;

template <> LUTEFISK3D_EXPORT const Matrix4& Variant::Get<const Matrix4&>() const;

template <> LUTEFISK3D_EXPORT ResourceRef Variant::Get<ResourceRef>() const;

template <> LUTEFISK3D_EXPORT ResourceRefList Variant::Get<ResourceRefList>() const;

template <> LUTEFISK3D_EXPORT VariantVector Variant::Get<VariantVector>() const;

template <> LUTEFISK3D_EXPORT QStringList Variant::Get<QStringList>() const;

template <> LUTEFISK3D_EXPORT VariantMap Variant::Get<VariantMap>() const;

template <> LUTEFISK3D_EXPORT Vector2 Variant::Get<Vector2>() const;

template <> LUTEFISK3D_EXPORT Vector3 Variant::Get<Vector3>() const;

template <> LUTEFISK3D_EXPORT Vector4 Variant::Get<Vector4>() const;

template <> LUTEFISK3D_EXPORT Quaternion Variant::Get<Quaternion>() const;

template <> LUTEFISK3D_EXPORT Color Variant::Get<Color>() const;

template <> LUTEFISK3D_EXPORT QString Variant::Get<QString>() const;

template <> LUTEFISK3D_EXPORT Rect Variant::Get<Rect>() const;

template <> LUTEFISK3D_EXPORT IntRect Variant::Get<IntRect>() const;

template <> LUTEFISK3D_EXPORT IntVector2 Variant::Get<IntVector2>() const;

template <> LUTEFISK3D_EXPORT IntVector3 Variant::Get<IntVector3>() const;

template <> LUTEFISK3D_EXPORT std::vector<unsigned char> Variant::Get<std::vector<unsigned char> >() const;

template <> LUTEFISK3D_EXPORT Matrix3 Variant::Get<Matrix3>() const;

template <> LUTEFISK3D_EXPORT Matrix3x4 Variant::Get<Matrix3x4>() const;

template <> LUTEFISK3D_EXPORT Matrix4 Variant::Get<Matrix4>() const;

// Implementations
template <class T> T* CustomVariantValue::GetValuePtr()
{
    if (IsType<T>())
    {
        auto impl = static_cast<CustomVariantValueImpl<T>*>(this);
        return &impl->GetValue();
    }
    return nullptr;
}

template <class T> const T* CustomVariantValue::GetValuePtr() const
{
    if (IsType<T>())
    {
        auto impl = static_cast<const CustomVariantValueImpl<T>*>(this);
        return &impl->GetValue();
    }
    return nullptr;
}
}
