//
// Copyright (c) 2008-2015 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:addmember
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
#include "JSONValue.h"

#include "JSONFile.h"
#include "../Core/Context.h"
#include "../Core/StringUtils.h"
#include "../IO/Log.h"

#include <rapidjson/document.h>

using namespace rapidjson;

namespace Urho3D
{

const JSONValue JSONValue::EMPTY;

static rapidjson::Type ToRapidJsonType(JSONValueType valueType)
{
    if (valueType == JSON_OBJECT)
        return kObjectType;
    else if (valueType == JSON_ARRAY)
        return kArrayType;
    return kNullType;
}

JSONValue::JSONValue() :
    file_(nullptr),
    value_(nullptr)
{
}

JSONValue::JSONValue(JSONFile* file, rapidjson::Value* value) :
    file_(file),
    value_(value)
{
}

JSONValue::JSONValue(const JSONValue& rhs) :
    file_(rhs.file_),
    value_(rhs.value_)
{
}

JSONValue::~JSONValue()
{
}

JSONValue& JSONValue::operator = (const JSONValue& rhs)
{
    file_ = rhs.file_;
    value_ = rhs.value_;
    return *this;
}

bool JSONValue::IsNull() const
{
    return value_ == nullptr;
}

bool JSONValue::NotNull() const
{
    return value_ != nullptr;
}

JSONValue::operator bool() const
{
    return NotNull();
}

JSONValue JSONValue::CreateChild(const QString& name, JSONValueType valueType)
{
    assert(IsObject());
    if (!IsObject())
        return JSONValue::EMPTY;

    Value jsonValue;
    if (valueType == JSON_OBJECT)
        jsonValue.SetObject();
    else if (valueType == JSON_ARRAY)
        jsonValue.SetArray();

    AddMember(name, jsonValue);

    return GetChild(name, valueType);
}

JSONValue JSONValue::GetChild(const QString& name, JSONValueType valueType) const
{
    assert(IsObject());

    if (!value_->HasMember(qPrintable(name)))
        return JSONValue::EMPTY;

    Value& value = GetMember(name);
    if (valueType != JSON_ANY && value.GetType() != ToRapidJsonType(valueType))
        return JSONValue::EMPTY;

    return JSONValue(file_, &value);
}

void JSONValue::SetInt(const QString& name, int value)
{
    Value jsonValue;
    jsonValue.SetInt(value);
    AddMember(name, jsonValue);
}

void JSONValue::SetBool(const QString& name, bool value)
{
    Value jsonValue;
    jsonValue.SetBool(value);
    AddMember(name, jsonValue);
}

void JSONValue::SetFloat(const QString& name, float value)
{
    Value jsonValue;
    jsonValue.SetDouble((double)value);
    AddMember(name, jsonValue);
}

void JSONValue::SetDouble(const QString& name, double value)
{
    Value jsonValue;
    jsonValue.SetDouble(value);
    AddMember(name, jsonValue);
}

void JSONValue::SetVector2(const QString& name, const Vector2& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetVector3(const QString& name, const Vector3& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetVector4(const QString& name, const Vector4& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetVectorVariant(const QString& name, const Variant& value)
{
    VariantType type = value.GetType();
    if (type == VAR_FLOAT || type == VAR_VECTOR2 || type == VAR_VECTOR3 || type == VAR_VECTOR4 || type == VAR_MATRIX3 ||
        type == VAR_MATRIX3X4 || type == VAR_MATRIX4)
        SetString(name, value.ToString());
}

void JSONValue::SetQuaternion(const QString& name, const Quaternion& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetColor(const QString& name, const Color& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetString(const QString& name, const QString& value)
{
    Value jsonValue;
    jsonValue.SetString(qPrintable(value), value.length(), file_->GetDocument()->GetAllocator());
    AddMember(name, jsonValue);
}

void JSONValue::SetBuffer(const QString& name, const void* data, unsigned size)
{
    QString dataStr;
    BufferToString(dataStr, data, size);
    SetString(name, dataStr);
}

void JSONValue::SetBuffer(const QString& name, const std::vector<unsigned char>& value)
{
    if (!value.size())
        SetString(name, QString::null);
    else
        SetBuffer(name, &value[0], value.size());
}

void JSONValue::SetResourceRef(const QString& name, const ResourceRef& value)
{
    Context* context = file_->GetContext();
    SetString(name, QString(context->GetTypeName(value.type_)) + ";" + value.name_);
}

void JSONValue::SetResourceRefList(const QString& name, const ResourceRefList& value)
{
    Context* context = file_->GetContext();
    QString str(context->GetTypeName(value.type_));
    for (unsigned i = 0; i < value.names_.size(); ++i)
    {
        str += ";";
        str += value.names_[i];
    }
    SetString(name, str);
}

void JSONValue::SetIntRect(const QString& name, const IntRect& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetIntVector2(const QString& name, const IntVector2& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetMatrix3(const QString& name, const Matrix3& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetMatrix3x4(const QString& name, const Matrix3x4& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetMatrix4(const QString& name, const Matrix4& value)
{
    SetString(name, value.ToString());
}

void JSONValue::SetVariant(const QString& name, const Variant& value)
{
    // Create child object for variant
    JSONValue child = CreateChild(name, JSON_OBJECT);
    // Set type
    child.SetString("type", value.GetTypeName());
    // Set value
    child.SetVariantValue("value", value);
}

void JSONValue::SetVariantValue(const QString& name, const Variant& value)
{
    switch (value.GetType())
    {
    case VAR_RESOURCEREF:
        SetResourceRef(name, value.GetResourceRef());
        break;

    case VAR_RESOURCEREFLIST:
        SetResourceRefList(name, value.GetResourceRefList());
        break;

    case VAR_VARIANTVECTOR:
    case VAR_VARIANTMAP:
        LOGERROR("Unsupported value type");
        break;

    default:
        SetString(name, value.ToString());
    }
}

bool JSONValue::IsObject() const
{
    return value_ && value_->IsObject();
}

QStringList JSONValue::GetChildNames() const
{
    QStringList ret;
    if (!IsObject())
        return ret;

    for (Value::ConstMemberIterator i = value_->MemberBegin(); i != value_->MemberEnd(); ++i)
    {
        // Only reutrn name for child object and array
        if (i->value.GetType() == kArrayType || i->value.GetType() == kObjectType)
            ret.push_back(i->name.GetString());
    }

    return ret;
}

QStringList JSONValue::GetValueNames() const
{
    QStringList ret;
    if (!IsObject())
        return ret;

    for (Value::ConstMemberIterator i = value_->MemberBegin(); i != value_->MemberEnd(); ++i)
    {
        if (i->value.GetType() != kArrayType && i->value.GetType() != kObjectType)
            ret.push_back(i->name.GetString());
    }

    return ret;
}

int JSONValue::GetInt(const QString& name) const
{
    return GetMember(name).GetInt();
}

bool JSONValue::GetBool(const QString& name) const
{
    return GetMember(name).GetBool();
}

float JSONValue::GetFloat(const QString& name) const
{
    return (float)GetMember(name).GetDouble();
}

double JSONValue::GetDouble(const QString& name) const
{
    return GetMember(name).GetDouble();
}

Vector2 JSONValue::GetVector2(const QString& name) const
{
    return ToVector2(GetCString(name));
}

Vector3 JSONValue::GetVector3(const QString& name) const
{
    return ToVector3(GetCString(name));
}

Vector4 JSONValue::GetVector4(const QString& name) const
{
    return ToVector4(GetCString(name));
}

Variant JSONValue::GetVectorVariant(const QString& name) const
{
    return ToVectorVariant(GetCString(name));
}

Quaternion JSONValue::GetQuaternion(const QString& name) const
{
    return ToQuaternion(GetCString(name));
}

Color JSONValue::GetColor(const QString& name) const
{
    return ToColor(GetCString(name));
}

QString JSONValue::GetString(const QString& name) const
{
    return GetMember(name).GetString();
}

const char* JSONValue::GetCString(const QString& name) const
{
    return GetMember(name).GetString();
}

std::vector<unsigned char> JSONValue::GetBuffer(const QString& name) const
{
    std::vector<unsigned char> buffer;
    StringToBuffer(buffer, GetCString(name));
    return buffer;
}

bool JSONValue::GetBuffer(const QString& name, void* dest, unsigned size) const
{
    QStringList bytes = GetString(name).split(' ');
    unsigned char* destBytes = (unsigned char*)dest;
    if (size < bytes.size())
        return false;

    for (unsigned i = 0; i < bytes.size(); ++i)
        destBytes[i] = bytes[i].toInt();
    return true;
}


ResourceRef JSONValue::GetResourceRef(const QString& name) const
{
    ResourceRef ret;

    QStringList values = GetString(name).split(';');
    if (values.size() == 2)
    {
        ret.type_ = values[0];
        ret.name_ = values[1];
    }

    return ret;
}

ResourceRefList JSONValue::GetResourceRefList(const QString& name) const
{
    ResourceRefList ret;

    QStringList values = GetString(name).split(';');
    if (values.size() >= 1)
    {
        ret.type_ = values[0];
        ret.names_.resize(values.size() - 1);
        for (unsigned i = 1; i < values.size(); ++i)
            ret.names_[i - 1] = values[i];
    }

    return ret;
}

IntRect JSONValue::GetIntRect(const QString& name) const
{
    return ToIntRect(GetCString(name));
}

IntVector2 JSONValue::GetIntVector2(const QString& name) const
{
    return ToIntVector2(GetCString(name));
}

Matrix3 JSONValue::GetMatrix3(const QString& name) const
{
    return ToMatrix3(GetCString(name));
}

Matrix3x4 JSONValue::GetMatrix3x4(const QString& name) const
{
    return ToMatrix3x4(GetCString(name));
}

Matrix4 JSONValue::GetMatrix4(const QString& name) const
{
    return ToMatrix4(GetCString(name));
}

Variant JSONValue::GetVariant(const QString& name) const
{
    // Get child for variant
    JSONValue child = GetChild(name, JSON_OBJECT);
    if (child.IsNull())
        return Variant::EMPTY;

    // Get type
    VariantType type = Variant::GetTypeFromName(child.GetString("type"));
    // Get value
    return child.GetVariantValue("value", type);
}

Variant JSONValue::GetVariantValue(const QString& name, VariantType type) const
{
    Variant ret;

    if (type == VAR_RESOURCEREF)
        ret = GetResourceRef(name);
    else if (type == VAR_RESOURCEREFLIST)
        ret = GetResourceRefList(name);
    else if (type == VAR_VARIANTVECTOR || type == VAR_VARIANTMAP)
        LOGERROR("Unsupported value type");
    else
        ret.FromString(type, GetCString(name));

    return ret;
}

JSONValue JSONValue::CreateChild(JSONValueType valueType)
{
    assert(IsArray());
    if (!IsArray())
        return JSONValue::EMPTY;

    Value value(ToRapidJsonType(valueType));
    value_->PushBack(value, file_->GetDocument()->GetAllocator());

    return GetChild(GetSize() - 1, valueType);
}

JSONValue JSONValue::GetChild(unsigned index, JSONValueType valueType) const
{
    if (index >= GetSize())
        return JSONValue::EMPTY;

    const Value& value = (*value_)[(SizeType)index];
    if (valueType != JSON_ANY && value.GetType() != ToRapidJsonType(valueType))
        return JSONValue::EMPTY;

    return JSONValue(file_, (Value*)&value);
}

void JSONValue::AddInt(int value)
{
    Value jsonValue;
    jsonValue.SetInt(value);
    AddMember(jsonValue);
}

void JSONValue::AddBool(bool value)
{
    Value jsonValue;
    jsonValue.SetBool(value);
    AddMember(jsonValue);
}

void JSONValue::AddFloat(float value)
{
    Value jsonValue;
    jsonValue.SetDouble((double)value);
    AddMember(jsonValue);
}

void JSONValue::AddDouble(double value)
{
    Value jsonValue;
    jsonValue.SetDouble(value);
    AddMember(jsonValue);
}

void JSONValue::AddVector2(const Vector2& value)
{
    AddString(value.ToString());
}

void JSONValue::AddVector3(const Vector3& value)
{
    AddString(value.ToString());
}

void JSONValue::AddVector4(const Vector4& value)
{
    AddString(value.ToString());
}

void JSONValue::AddVectorVariant(const Variant& value)
{
    VariantType type = value.GetType();
    if (type == VAR_FLOAT || type == VAR_VECTOR2 || type == VAR_VECTOR3 || type == VAR_VECTOR4 || type == VAR_MATRIX3 ||
        type == VAR_MATRIX3X4 || type == VAR_MATRIX4)
        AddString(value.ToString());
}

void JSONValue::AddQuaternion(const Quaternion& value)
{
    AddString(value.ToString());
}

void JSONValue::AddColor(const Color& value)
{
    AddString(value.ToString());
}

void JSONValue::AddString(const QString& value)
{
    Value jsonValue;
    jsonValue.SetString(qPrintable(value), value.length(), file_->GetDocument()->GetAllocator());
    AddMember(jsonValue);
}

void JSONValue::AddBuffer(const std::vector<unsigned char>& value)
{
    if (!value.size())
        AddString(QString::null);
    else
        AddBuffer(&value[0], value.size());
}

void JSONValue::AddBuffer(const void* data, unsigned size)
{
    QString dataStr;
    BufferToString(dataStr, data, size);
    AddString(dataStr);
}

void JSONValue::AddResourceRef(const ResourceRef& value)
{
    Context* context = file_->GetContext();
    AddString(QString(context->GetTypeName(value.type_)) + ";" + value.name_);
}

void JSONValue::AddResourceRefList(const ResourceRefList& value)
{
    Context* context = file_->GetContext();
    QString str(context->GetTypeName(value.type_));
    for (unsigned i = 0; i < value.names_.size(); ++i)
    {
        str += ";";
        str += value.names_[i];
    }
    AddString(str);
}

void JSONValue::AddIntRect(const IntRect& value)
{
    AddString(value.ToString());
}

void JSONValue::AddIntVector2(const IntVector2& value)
{
    AddString(value.ToString());
}

void JSONValue::AddMatrix3(const Matrix3& value)
{
    AddString(value.ToString());
}

void JSONValue::AddMatrix3x4(const Matrix3x4& value)
{
    AddString(value.ToString());
}

void JSONValue::AddMatrix4(const Matrix4& value)
{
    AddString(value.ToString());
}

void JSONValue::AddVariant(const Variant& value)
{
    // Create child object for variant
    JSONValue child = CreateChild(JSON_OBJECT);
    // Set type
    child.SetString("type", value.GetTypeName());
    // Set value
    child.SetVariantValue("value", value);
}

void JSONValue::AddVariantValue(const Variant& value)
{
    switch (value.GetType())
    {
    case VAR_RESOURCEREF:
        AddResourceRef(value.GetResourceRef());
        break;

    case VAR_RESOURCEREFLIST:
        AddResourceRefList(value.GetResourceRefList());
        break;

    case VAR_VARIANTVECTOR:
    case VAR_VARIANTMAP:
        LOGERROR("Unsupported value type");
        break;

    default:
        AddString(value.ToString());
    }
}

unsigned JSONValue::GetSize() const
{
    if (IsArray())
        return (unsigned)value_->Size();
    else
        return 0;
}

bool JSONValue::IsArray() const
{
    return value_ && value_->IsArray();
}

int JSONValue::GetInt(unsigned index) const
{
    return GetMember(index).GetInt();
}

bool JSONValue::GetBool(unsigned index) const
{
    return GetMember(index).GetBool();
}

float JSONValue::GetFloat(unsigned index) const
{
    return (float)GetMember(index).GetDouble();
}

double JSONValue::GetDouble(unsigned index) const
{
    return GetMember(index).GetDouble();
}

Vector2 JSONValue::GetVector2(unsigned index) const
{
    return ToVector2(GetCString(index));
}

Vector3 JSONValue::GetVector3(unsigned index) const
{
    return ToVector3(GetCString(index));
}

Vector4 JSONValue::GetVector4(unsigned index) const
{
    return ToVector4(GetCString(index));
}

Variant JSONValue::GetVectorVariant(unsigned index) const
{
    return ToVectorVariant(GetCString(index));
}

Quaternion JSONValue::GetQuaternion(unsigned index) const
{
    return ToQuaternion(GetCString(index));
}

Color JSONValue::GetColor(unsigned index) const
{
    return ToColor(GetCString(index));
}

QString JSONValue::GetString(unsigned index) const
{
    return GetMember(index).GetString();
}

const char* JSONValue::GetCString(unsigned index) const
{
    return GetMember(index).GetString();
}

std::vector<unsigned char> JSONValue::GetBuffer(unsigned index) const
{
    std::vector<unsigned char> buffer;
    StringToBuffer(buffer, GetCString(index));
    return buffer;
}

bool JSONValue::GetBuffer(unsigned index, void* dest, unsigned size) const
{
    QStringList bytes = GetString(index).split(' ');
    unsigned char* destBytes = (unsigned char*)dest;
    if (size < bytes.size())
        return false;

    for (unsigned i = 0; i < bytes.size(); ++i)
        destBytes[i] = bytes[i].toInt();
    return true;
}

ResourceRef JSONValue::GetResourceRef(unsigned index) const
{
    ResourceRef ret;

    QStringList values = GetString(index).split(';');
    if (values.size() == 2)
    {
        ret.type_ = values[0];
        ret.name_ = values[1];
    }

    return ret;
}

ResourceRefList JSONValue::GetResourceRefList(unsigned index) const
{
    ResourceRefList ret;

    QStringList values = GetString(index).split(';');
    if (values.size() >= 1)
    {
        ret.type_ = values[0];
        ret.names_.resize(values.size() - 1);
        for (unsigned i = 1; i < values.size(); ++i)
            ret.names_[i - 1] = values[i];
    }

    return ret;
}

IntRect JSONValue::GetIntRect(unsigned index) const
{
    return ToIntRect(GetCString(index));
}

IntVector2 JSONValue::GetIntVector2(unsigned index) const
{
    return ToIntVector2(GetCString(index));
}

Matrix3 JSONValue::GetMatrix3(unsigned index) const
{
    return ToMatrix3(GetCString(index));
}

Matrix3x4 JSONValue::GetMatrix3x4(unsigned index) const
{
    return ToMatrix3x4(GetCString(index));
}

Matrix4 JSONValue::GetMatrix4(unsigned index) const
{
    return ToMatrix4(GetCString(index));
}

Variant JSONValue::GetVariant(unsigned index) const
{
    // Get child for variant
    JSONValue child = GetChild(index, JSON_OBJECT);
    if (child.IsNull())
        return Variant::EMPTY;

    // Get type
    VariantType type = Variant::GetTypeFromName(child.GetString("type"));
    // Get value
    return child.GetVariantValue("value", type);
}

Variant JSONValue::GetVariantValue(unsigned index, VariantType type) const
{
    Variant ret;

    if (type == VAR_RESOURCEREF)
        ret = GetResourceRef(index);
    else if (type == VAR_RESOURCEREFLIST)
        ret = GetResourceRefList(index);
    else if (type == VAR_VARIANTVECTOR || type == VAR_VARIANTMAP)
        LOGERROR("Unsupported value type");
    else
        ret.FromString(type, GetCString(index));

    return ret;
}

void JSONValue::AddMember(const QString& name, rapidjson::Value& jsonValue)
{
    if (!IsObject())
        return;

    Value jsonName;
    jsonName.SetString(qPrintable(name), name.length(), file_->GetDocument()->GetAllocator());
    value_->AddMember(jsonName, jsonValue, file_->GetDocument()->GetAllocator());
}

rapidjson::Value& JSONValue::GetMember(const QString& name) const
{
    assert(IsObject());

    return (*value_)[qPrintable(name)];
}

void JSONValue::AddMember(rapidjson::Value& jsonValue)
{
    assert(IsArray());

    value_->PushBack(jsonValue, file_->GetDocument()->GetAllocator());
}

rapidjson::Value& JSONValue::GetMember(unsigned index) const
{
    assert(IsArray());

    return (*value_)[(SizeType)index];
}

}
