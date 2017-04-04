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

#include "Variant.h"
#include "StringUtils.h"

#include <cstring>

namespace Urho3D
{

const Variant Variant::EMPTY;
const std::vector<unsigned char> Variant::emptyBuffer(0);
const ResourceRef Variant::emptyResourceRef;
const ResourceRefList Variant::emptyResourceRefList;
const VariantMap Variant::emptyVariantMap;
const VariantVector Variant::emptyVariantVector(0);
const QStringList Variant::emptyStringVector;

static const char* typeNames[] =
{
    "None",
    "Int",
    "Bool",
    "Float",
    "Vector2",
    "Vector3",
    "Vector4",
    "Quaternion",
    "Color",
    "String",
    "Buffer",
    "VoidPtr",
    "ResourceRef",
    "ResourceRefList",
    "VariantVector",
    "VariantMap",
    "IntRect",
    "IntVector2",
    "Ptr",
    "Matrix3",
    "Matrix3x4",
    "Matrix4",
    "Double",
    "StringVector",
    nullptr
};
Variant& Variant::operator = (const Variant& rhs)
{
    SetType(rhs.GetType());

    switch (type_)
    {
    case VAR_STRING:
        *(reinterpret_cast<QString*>(&value_)) = *(reinterpret_cast<const QString*>(&rhs.value_));
        break;

    case VAR_BUFFER:
        *(reinterpret_cast<std::vector<unsigned char>*>(&value_)) = *(reinterpret_cast<const std::vector<unsigned char>*>(&rhs.value_));
        break;

    case VAR_RESOURCEREF:
        *(reinterpret_cast<ResourceRef*>(&value_)) = *(reinterpret_cast<const ResourceRef*>(&rhs.value_));
        break;

    case VAR_RESOURCEREFLIST:
        *(reinterpret_cast<ResourceRefList*>(&value_)) = *(reinterpret_cast<const ResourceRefList*>(&rhs.value_));
        break;

    case VAR_VARIANTVECTOR:
        *(reinterpret_cast<VariantVector*>(&value_)) = *(reinterpret_cast<const VariantVector*>(&rhs.value_));
        break;

    case VAR_STRINGVECTOR:
        *(reinterpret_cast<QStringList*>(&value_)) = *(reinterpret_cast<const QStringList*>(&rhs.value_));
        break;
    case VAR_VARIANTMAP:
        *(reinterpret_cast<VariantMap*>(&value_)) = *(reinterpret_cast<const VariantMap*>(&rhs.value_));
        break;

    case VAR_PTR:
        *(reinterpret_cast<WeakPtr<RefCounted>*>(&value_)) = *(reinterpret_cast<const WeakPtr<RefCounted>*>(&rhs.value_));
        break;

    case VAR_MATRIX3:
        *(reinterpret_cast<Matrix3*>(value_.ptr_)) = *(reinterpret_cast<const Matrix3*>(rhs.value_.ptr_));
        break;

    case VAR_MATRIX3X4:
        *(reinterpret_cast<Matrix3x4*>(value_.ptr_)) = *(reinterpret_cast<const Matrix3x4*>(rhs.value_.ptr_));
        break;

    case VAR_MATRIX4:
        *(reinterpret_cast<Matrix4*>(value_.ptr_)) = *(reinterpret_cast<const Matrix4*>(rhs.value_.ptr_));
        break;

    default:
        value_ = rhs.value_;
        break;
    }

    return *this;
}

bool Variant::operator == (const Variant& rhs) const
{
    if (type_ == VAR_VOIDPTR || type_ == VAR_PTR)
        return GetVoidPtr() == rhs.GetVoidPtr();
    else if (type_ != rhs.type_)
        return false;

    switch (type_)
    {
    case VAR_INT:
        return value_.int_ == rhs.value_.int_;

    case VAR_BOOL:
        return value_.bool_ == rhs.value_.bool_;

    case VAR_FLOAT:
        return value_.float_ == rhs.value_.float_;


    case VAR_VECTOR2:
        return *(reinterpret_cast<const Vector2*>(&value_)) == *(reinterpret_cast<const Vector2*>(&rhs.value_));

    case VAR_VECTOR3:
        return *(reinterpret_cast<const Vector3*>(&value_)) == *(reinterpret_cast<const Vector3*>(&rhs.value_));

    case VAR_VECTOR4:
    case VAR_QUATERNION:
    case VAR_COLOR:
        // Hack: use the Vector4 compare for all these classes, as they have the same memory structure
        return *(reinterpret_cast<const Vector4*>(&value_)) == *(reinterpret_cast<const Vector4*>(&rhs.value_));

    case VAR_STRING:
        return *(reinterpret_cast<const QString*>(&value_)) == *(reinterpret_cast<const QString*>(&rhs.value_));

    case VAR_BUFFER:
        return *(reinterpret_cast<const std::vector<unsigned char>*>(&value_)) == *(reinterpret_cast<const std::vector<unsigned char>*>(&rhs.value_));

    case VAR_RESOURCEREF:
        return *(reinterpret_cast<const ResourceRef*>(&value_)) == *(reinterpret_cast<const ResourceRef*>(&rhs.value_));

    case VAR_RESOURCEREFLIST:
        return *(reinterpret_cast<const ResourceRefList*>(&value_)) == *(reinterpret_cast<const ResourceRefList*>(&rhs.value_));

    case VAR_VARIANTVECTOR:
        return *(reinterpret_cast<const VariantVector*>(&value_)) == *(reinterpret_cast<const VariantVector*>(&rhs.value_));

    case VAR_STRINGVECTOR:
        return *(reinterpret_cast<const QStringList*>(&value_)) == *(reinterpret_cast<const QStringList*>(&rhs.value_));
    case VAR_VARIANTMAP:
        return *(reinterpret_cast<const VariantMap*>(&value_)) == *(reinterpret_cast<const VariantMap*>(&rhs.value_));

    case VAR_INTRECT:
        return *(reinterpret_cast<const IntRect*>(&value_)) == *(reinterpret_cast<const IntRect*>(&rhs.value_));

    case VAR_INTVECTOR2:
        return *(reinterpret_cast<const IntVector2*>(&value_)) == *(reinterpret_cast<const IntVector2*>(&rhs.value_));

    case VAR_MATRIX3:
        return *(reinterpret_cast<const Matrix3*>(value_.ptr_)) == *(reinterpret_cast<const Matrix3*>(rhs.value_.ptr_));

    case VAR_MATRIX3X4:
        return *(reinterpret_cast<const Matrix3x4*>(value_.ptr_)) == *(reinterpret_cast<const Matrix3x4*>(rhs.value_.ptr_));

    case VAR_MATRIX4:
        return *(reinterpret_cast<const Matrix4*>(value_.ptr_)) == *(reinterpret_cast<const Matrix4*>(rhs.value_.ptr_));
    case VAR_DOUBLE:
        return *(reinterpret_cast<const double*>(&value_)) == *(reinterpret_cast<const double*>(&rhs.value_));

    default:
        return true;
    }
}

bool Variant::operator ==(const std::vector<unsigned char>& rhs) const
{
    // Use strncmp() instead of std::vector<unsigned char>::operator ==
    const std::vector<unsigned char>& buffer = *(reinterpret_cast<const std::vector<unsigned char>*>(&value_));
    return type_ == VAR_BUFFER && buffer.size() == rhs.size() ? strncmp(reinterpret_cast<const char*>(&buffer[0]), reinterpret_cast<const char*>(&rhs[0]), buffer.size()) == 0 : false;
}
void Variant::FromString(const QString& type, const QString& value)
{
    return FromString(GetTypeFromName(type), qPrintable(value));
}

void Variant::FromString(const char* type, const char* value)
{
    return FromString(GetTypeFromName(type), value);
}

void Variant::FromString(VariantType type, const QString& value)
{
    return FromString(type, qPrintable(value));
}

void Variant::FromString(VariantType type, const char* value)
{
    QString src(value);
    switch (type)
    {
    case VAR_INT:
        *this = src.toInt();
        break;

    case VAR_BOOL:
        *this = ToBool(value);
        break;

    case VAR_FLOAT:
        *this = src.toFloat();
        break;

    case VAR_VECTOR2:
        *this = ToVector2(value);
        break;

    case VAR_VECTOR3:
        *this = ToVector3(value);
        break;

    case VAR_VECTOR4:
        *this = ToVector4(value);
        break;

    case VAR_QUATERNION:
        *this = ToQuaternion(value);
        break;

    case VAR_COLOR:
        *this = ToColor(value);
        break;

    case VAR_STRING:
        *this = value;
        break;

    case VAR_BUFFER:
        {
            SetType(VAR_BUFFER);
            std::vector<unsigned char>& buffer = *(reinterpret_cast<std::vector<unsigned char>*>(&value_));
            StringToBuffer(buffer, value);
        }
        break;

    case VAR_VOIDPTR:
        // From string to void pointer not supported, set to null
        *this = (void*)nullptr;
        break;

    case VAR_RESOURCEREF:
        {
            QStringList values = QString(value).split(';');
            if (values.size() == 2)
            {
                SetType(VAR_RESOURCEREF);
                ResourceRef& ref = *(reinterpret_cast<ResourceRef*>(&value_));
                ref.type_ = values[0];
                ref.name_ = values[1];
            }
        }
        break;

    case VAR_RESOURCEREFLIST:
        {
            QStringList values = QString(value).split(';');
            if (values.size() >= 1)
            {
                SetType(VAR_RESOURCEREFLIST);
                ResourceRefList& refList = *(reinterpret_cast<ResourceRefList*>(&value_));
                refList.type_ = values[0];
                refList.names_.resize(values.size() - 1);
                for (unsigned i = 1; i < values.size(); ++i)
                    refList.names_[i - 1] = values[i];
            }
        }
        break;

    case VAR_INTRECT:
        *this = ToIntRect(value);
        break;

    case VAR_INTVECTOR2:
        *this = ToIntVector2(value);
        break;

    case VAR_PTR:
        // From string to RefCounted pointer not supported, set to null
        *this = (RefCounted*)nullptr;
        break;

    case VAR_MATRIX3:
        *this = ToMatrix3(value);
        break;

    case VAR_MATRIX3X4:
        *this = ToMatrix3x4(value);
        break;

    case VAR_MATRIX4:
        *this = ToMatrix4(value);
        break;
    case VAR_DOUBLE:
        *this = src.toDouble();
        break;
    default:
        SetType(VAR_NONE);
    }
}

void Variant::SetBuffer(const void* data, unsigned size)
{
    if (size && !data)
        size = 0;

    SetType(VAR_BUFFER);
    std::vector<unsigned char>& buffer = *(reinterpret_cast<std::vector<unsigned char>*>(&value_));
    buffer.resize(size);
    if (size)
        memcpy(&buffer[0], data, size);
}

QString Variant::GetTypeName() const
{
    return typeNames[type_];
}

QString Variant::ToString() const
{
    switch (type_)
    {
    case VAR_INT:
        return QString::number(value_.int_);

    case VAR_BOOL:
        return QString::number(value_.bool_);

    case VAR_FLOAT:
        return QString::number(value_.float_);

    case VAR_VECTOR2:
        return (reinterpret_cast<const Vector2*>(&value_))->ToString();

    case VAR_VECTOR3:
        return (reinterpret_cast<const Vector3*>(&value_))->ToString();

    case VAR_VECTOR4:
        return (reinterpret_cast<const Vector4*>(&value_))->ToString();

    case VAR_QUATERNION:
        return (reinterpret_cast<const Quaternion*>(&value_))->ToString();

    case VAR_COLOR:
        return (reinterpret_cast<const Color*>(&value_))->ToString();

    case VAR_STRING:
        return *(reinterpret_cast<const QString*>(&value_));

    case VAR_BUFFER:
        {
            const std::vector<unsigned char>& buffer = *(reinterpret_cast<const std::vector<unsigned char>*>(&value_));
            QString ret;
            BufferToString(ret, buffer.data(), buffer.size());
            return ret;
        }

    case VAR_VOIDPTR:
    case VAR_PTR:
        // Pointer serialization not supported (convert to null)
        return QString("null"); //TODO: use String::null ?

    case VAR_INTRECT:
        return (reinterpret_cast<const IntRect*>(&value_))->ToString();

    case VAR_INTVECTOR2:
        return (reinterpret_cast<const IntVector2*>(&value_))->ToString();

    case VAR_MATRIX3:
        return (reinterpret_cast<const Matrix3*>(value_.ptr_))->ToString();

    case VAR_MATRIX3X4:
        return (reinterpret_cast<const Matrix3x4*>(value_.ptr_))->ToString();

    case VAR_MATRIX4:
        return (reinterpret_cast<const Matrix4*>(value_.ptr_))->ToString();

    case VAR_DOUBLE:
        return QString::number(value_.double_);
    default:
        // VAR_RESOURCEREF, VAR_RESOURCEREFLIST, VAR_VARIANTVECTOR, VAR_STRINGVECTOR, VAR_VARIANTMAP
        // Reference string serialization requires typehash-to-name mapping from the context. Can not support here
        // Also variant map or vector string serialization is not supported. XML or binary save should be used instead
        return QString::null;
    }
}

bool Variant::IsZero() const
{
    switch (type_)
    {
    case VAR_INT:
        return value_.int_ == 0;

    case VAR_BOOL:
        return value_.bool_ == false;

    case VAR_FLOAT:
        return value_.float_ == 0.0f;


    case VAR_VECTOR2:
        return *reinterpret_cast<const Vector2*>(&value_) == Vector2::ZERO;

    case VAR_VECTOR3:
        return *reinterpret_cast<const Vector3*>(&value_) == Vector3::ZERO;

    case VAR_VECTOR4:
        return *reinterpret_cast<const Vector4*>(&value_) == Vector4::ZERO;

    case VAR_QUATERNION:
        return *reinterpret_cast<const Quaternion*>(&value_) == Quaternion::IDENTITY;

    case VAR_COLOR:
        // WHITE is considered empty (i.e. default) color in the Color class definition
        return *reinterpret_cast<const Color*>(&value_) == Color::WHITE;

    case VAR_STRING:
        return reinterpret_cast<const QString*>(&value_)->isEmpty();

    case VAR_BUFFER:
        return reinterpret_cast<const std::vector<unsigned char>*>(&value_)->empty();

    case VAR_VOIDPTR:
        return value_.ptr_ == nullptr;

    case VAR_RESOURCEREF:
        return reinterpret_cast<const ResourceRef*>(&value_)->name_.isEmpty();

    case VAR_RESOURCEREFLIST:
    {
        const std::vector<QString>& names = reinterpret_cast<const ResourceRefList*>(&value_)->names_;
        for (const QString &name : names)
        {
            if (!name.isEmpty())
                return false;
        }
        return true;
    }

    case VAR_VARIANTVECTOR:
        return reinterpret_cast<const VariantVector*>(&value_)->empty();

    case VAR_STRINGVECTOR:
        return reinterpret_cast<const QStringList*>(&value_)->isEmpty();

    case VAR_VARIANTMAP:
        return reinterpret_cast<const VariantMap*>(&value_)->empty();

    case VAR_INTRECT:
        return *reinterpret_cast<const IntRect*>(&value_) == IntRect::ZERO;

    case VAR_INTVECTOR2:
        return *reinterpret_cast<const IntVector2*>(&value_) == IntVector2::ZERO;

    case VAR_PTR:
        return *reinterpret_cast<const WeakPtr<RefCounted>*>(&value_) == (RefCounted*)nullptr;

    case VAR_MATRIX3:
        return *reinterpret_cast<const Matrix3*>(value_.ptr_) == Matrix3::IDENTITY;

    case VAR_MATRIX3X4:
        return *reinterpret_cast<const Matrix3x4*>(value_.ptr_) == Matrix3x4::IDENTITY;

    case VAR_MATRIX4:
        return *reinterpret_cast<const Matrix4*>(value_.ptr_) == Matrix4::IDENTITY;

    case VAR_DOUBLE:
        return *reinterpret_cast<const double*>(&value_) == 0.0;
    default:
        return true;
    }
}

void Variant::SetType(VariantType newType)
{
    if (type_ == newType)
        return;

    switch (type_)
    {
    case VAR_STRING:
        (reinterpret_cast<QString*>(&value_))->~QString();
        break;

    case VAR_BUFFER:
        (reinterpret_cast<std::vector<unsigned char>*>(&value_))->~vector();
        break;

    case VAR_RESOURCEREF:
        (reinterpret_cast<ResourceRef*>(&value_))->~ResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        (reinterpret_cast<ResourceRefList*>(&value_))->~ResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        (reinterpret_cast<VariantVector*>(&value_))->~VariantVector();
        break;

    case VAR_STRINGVECTOR:
        (reinterpret_cast<QStringList*>(&value_))->~QStringList();
        break;
    case VAR_VARIANTMAP:
        (reinterpret_cast<VariantMap*>(&value_))->~VariantMap();
        break;

    case VAR_PTR:
        (reinterpret_cast<WeakPtr<RefCounted>*>(&value_))->~WeakPtr<RefCounted>();
        break;

    case VAR_MATRIX3:
        delete reinterpret_cast<Matrix3*>(value_.ptr_);
        break;

    case VAR_MATRIX3X4:
        delete reinterpret_cast<Matrix3x4*>(value_.ptr_);
        break;

    case VAR_MATRIX4:
        delete reinterpret_cast<Matrix4*>(value_.ptr_);
        break;

    default:
        break;
    }

    type_ = newType;

    switch (type_)
    {
    case VAR_STRING:
        new(reinterpret_cast<QString*>(&value_)) QString();
        break;

    case VAR_BUFFER:
        static_assert(sizeof(std::vector<unsigned char>)<=sizeof(VariantValue),"Cannot construct PODVector<unsigned char> in-place");
        new(reinterpret_cast<std::vector<unsigned char>*>(&value_)) std::vector<unsigned char>();
        break;

    case VAR_RESOURCEREF:
        new(reinterpret_cast<ResourceRef*>(&value_)) ResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        new(reinterpret_cast<ResourceRefList*>(&value_)) ResourceRefList();
        static_assert(sizeof(ResourceRefList)<=sizeof(VariantValue),"Cannot construct ResourceRefList in-place");
        break;

    case VAR_VARIANTVECTOR:
        static_assert(sizeof(VariantVector)<=sizeof(VariantValue),"Cannot construct VariantVector in-place");
        new(reinterpret_cast<VariantVector*>(&value_)) VariantVector();
        break;

    case VAR_STRINGVECTOR:
        new(reinterpret_cast<QStringList*>(&value_)) QStringList();
        break;
    case VAR_VARIANTMAP:
        new(reinterpret_cast<VariantMap*>(&value_)) VariantMap();
        break;

    case VAR_PTR:
        static_assert(sizeof(WeakPtr<RefCounted>)<=sizeof(VariantValue),"Cannot construct WeakPtr<RefCounted> in-place");
        new(reinterpret_cast<WeakPtr<RefCounted>*>(&value_)) WeakPtr<RefCounted>();
        break;

    case VAR_MATRIX3:
        value_.ptr_ = new Matrix3();
        break;

    case VAR_MATRIX3X4:
        value_.ptr_ = new Matrix3x4();
        break;

    case VAR_MATRIX4:
        value_.ptr_ = new Matrix4();
        break;

    default:
        break;
    }
}

template<> int Variant::Get<int>() const
{
    return GetInt();
}

template<> unsigned Variant::Get<unsigned>() const
{
    return GetUInt();
}

template<> StringHash Variant::Get<StringHash>() const
{
    return GetStringHash();
}

template<> bool Variant::Get<bool>() const
{
    return GetBool();
}

template<> float Variant::Get<float>() const
{
    return GetFloat();
}
template<> double Variant::Get<double>() const
{
    return GetDouble();
}
template<> const Vector2& Variant::Get<const Vector2&>() const
{
    return GetVector2();
}

template<> const Vector3& Variant::Get<const Vector3&>() const
{
    return GetVector3();
}

template<> const Vector4& Variant::Get<const Vector4&>() const
{
    return GetVector4();
}

template<> const Quaternion& Variant::Get<const Quaternion&>() const
{
    return GetQuaternion();
}

template<> const Color& Variant::Get<const Color&>() const
{
    return GetColor();
}

template<> const QString& Variant::Get<const QString&>() const
{
    return GetString();
}

template<> const IntRect& Variant::Get<const IntRect&>() const
{
    return GetIntRect();
}

template<> const IntVector2& Variant::Get<const IntVector2&>() const
{
    return GetIntVector2();
}

template<> const std::vector<unsigned char>& Variant::Get<const std::vector<unsigned char>& >() const
{
    return GetBuffer();
}

template<> void* Variant::Get<void*>() const
{
    return GetVoidPtr();
}

template<> RefCounted* Variant::Get<RefCounted*>() const
{
    return GetPtr();
}

template<> const Matrix3& Variant::Get<const Matrix3&>() const
{
    return GetMatrix3();
}

template<> const Matrix3x4& Variant::Get<const Matrix3x4&>() const
{
    return GetMatrix3x4();
}

template<> const Matrix4& Variant::Get<const Matrix4&>() const
{
    return GetMatrix4();
}

template<> ResourceRef Variant::Get<ResourceRef>() const
{
    return GetResourceRef();
}

template<> ResourceRefList Variant::Get<ResourceRefList>() const
{
    return GetResourceRefList();
}

template<> VariantVector Variant::Get<VariantVector>() const
{
    return GetVariantVector();
}

template <> QStringList Variant::Get<QStringList >() const
{
    return GetStringVector();
}

template<> VariantMap Variant::Get<VariantMap>() const
{
    return GetVariantMap();
}

template<> Vector2 Variant::Get<Vector2>() const
{
    return GetVector2();
}

template<> Vector3 Variant::Get<Vector3>() const
{
    return GetVector3();
}

template<> Vector4 Variant::Get<Vector4>() const
{
    return GetVector4();
}

template<> Quaternion Variant::Get<Quaternion>() const
{
    return GetQuaternion();
}

template<> Color Variant::Get<Color>() const
{
    return GetColor();
}

template<> QString Variant::Get<QString>() const
{
    return GetString();
}

template <> Rect Variant::Get<Rect>() const
{
    return GetRect();
}
template<> IntRect Variant::Get<IntRect>() const
{
    return GetIntRect();
}

template<> IntVector2 Variant::Get<IntVector2>() const
{
    return GetIntVector2();
}

template<> std::vector<unsigned char> Variant::Get<std::vector<unsigned char> >() const
{
    return GetBuffer();
}

template<> Matrix3 Variant::Get<Matrix3>() const
{
    return GetMatrix3();
}

template<> Matrix3x4 Variant::Get<Matrix3x4>() const
{
    return GetMatrix3x4();
}

template<> Matrix4 Variant::Get<Matrix4>() const
{
    return GetMatrix4();
}

QString Variant::GetTypeName(VariantType type)
{
    return typeNames[type];
}

VariantType Variant::GetTypeFromName(const QString& typeName)
{
    return GetTypeFromName(qPrintable(typeName));
}

VariantType Variant::GetTypeFromName(const char* typeName)
{
    return (VariantType)GetStringListIndex(typeName, typeNames, VAR_NONE);
}

}
