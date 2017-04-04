//
// Copyright (c) 2008-2016 the Urho3D project.
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
#include "Lutefisk3D/Core/StringUtils.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Log.h"



namespace Urho3D
{

static const char* valueTypeNames[] =
{
    "Null",
    "Bool",
    "Number",
    "String",
    "Array",
    "Object",
    0
};

static const char* numberTypeNames[] =
{
    "NaN",
    "Int",
    "Unsigned",
    "Real",
    0
};
const JSONValue JSONValue::EMPTY;
const JSONArray JSONValue::emptyArray(0);
const JSONObject JSONValue::emptyObject;

JSONValue& JSONValue::operator =(bool rhs)
{
    SetType(JSON_BOOL);
    boolValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(int rhs)
{
    SetType(JSON_NUMBER, JSONNT_INT);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(unsigned rhs)
{
    SetType(JSON_NUMBER, JSONNT_UINT);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(float rhs)
{
    SetType(JSON_NUMBER, JSONNT_FLOAT_DOUBLE);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(double rhs)
{
    SetType(JSON_NUMBER, JSONNT_FLOAT_DOUBLE);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const QString& rhs)
{
    SetType(JSON_STRING);
    *stringValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const char* rhs)
{
    SetType(JSON_STRING);
    *stringValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const JSONArray& rhs)
{
    SetType(JSON_ARRAY);
    *arrayValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const JSONObject& rhs)
{
    SetType(JSON_OBJECT);
    *objectValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const JSONValue& rhs)
{
    if (this == &rhs)
        return *this;

    SetType(rhs.GetValueType(), rhs.GetNumberType());

    switch (GetValueType())
    {
    case JSON_BOOL:
        boolValue_ = rhs.boolValue_;
        break;

    case JSON_NUMBER:
        numberValue_ = rhs.numberValue_;
        break;

    case JSON_STRING:
        *stringValue_ = *rhs.stringValue_;
        break;

    case JSON_ARRAY:
        *arrayValue_ = *rhs.arrayValue_;
        break;

    case JSON_OBJECT:
        *objectValue_ = *rhs.objectValue_;

    default:
        break;
    }

    return *this;
}

JSONValueType JSONValue::GetValueType() const
{
    return (JSONValueType)(type_ >> 16);
}

JSONNumberType JSONValue::GetNumberType() const
{
    return (JSONNumberType)(type_ & 0xffff);
}

QString JSONValue::GetValueTypeName() const
{
    return GetValueTypeName(GetValueType());
}

QString JSONValue::GetNumberTypeName() const
{
    return GetNumberTypeName(GetNumberType());
}
JSONValue& JSONValue::operator [](unsigned index)
{
    // Convert to array type
    SetType(JSON_ARRAY);

    return (*arrayValue_)[index];
}

const JSONValue& JSONValue::operator [](unsigned index) const
{
    if (GetValueType() != JSON_ARRAY)
        return EMPTY;

    return (*arrayValue_)[index];
}

void JSONValue::Push(const JSONValue& value)
{
    // Convert to array type
    SetType(JSON_ARRAY);

    arrayValue_->push_back(value);
}

void JSONValue::Pop()
{
    if (GetValueType() != JSON_ARRAY)
        return;

    arrayValue_->pop_back();
}

void JSONValue::Insert(unsigned pos, const JSONValue& value)
{
    if (GetValueType() != JSON_ARRAY)
        return;

    arrayValue_->insert(arrayValue_->begin()+pos, value);
}

void JSONValue::Erase(unsigned pos, unsigned length)
{
    if (GetValueType() != JSON_ARRAY)
        return;

    arrayValue_->erase(arrayValue_->begin()+pos, arrayValue_->begin()+pos+length);
}

void JSONValue::Resize(unsigned newSize)
{
    // Convert to array type
    SetType(JSON_ARRAY);

    arrayValue_->resize(newSize);
}

unsigned JSONValue::Size() const
{
    if (GetValueType() == JSON_ARRAY)
        return arrayValue_->size();
    else if (GetValueType() == JSON_OBJECT)
        return objectValue_->size();

    return 0;
}

JSONValue& JSONValue::operator [](const QString& key)
{
    // Convert to object type
    SetType(JSON_OBJECT);

    return (*objectValue_)[key];
}

const JSONValue& JSONValue::operator [](const QString& key) const
{
    if (GetValueType() != JSON_OBJECT)
        return EMPTY;

    return (*objectValue_)[key];
}

void JSONValue::Set(const QString& key, const JSONValue& value)
{
    // Convert to object type
    SetType(JSON_OBJECT);

    (*objectValue_)[key] = value;
}

const JSONValue& JSONValue::Get(const QString& key) const
{
    if (GetValueType() != JSON_OBJECT)
        return EMPTY;

    auto i = objectValue_->find(key);
    if (i == objectValue_->end())
        return EMPTY;

    return MAP_VALUE(i);
}

bool JSONValue::Erase(const QString& key)
{
    if (GetValueType() != JSON_OBJECT)
        return false;

    return objectValue_->erase(key);
}

bool JSONValue::Contains(const QString& key) const
{
    if  (GetValueType() != JSON_OBJECT)
        return false;

    return objectValue_->contains(key);
}

void JSONValue::Clear()
{
    if (GetValueType() == JSON_ARRAY)
        arrayValue_->clear();
    else if (GetValueType() == JSON_OBJECT)
        objectValue_->clear();
}

void JSONValue::SetType(JSONValueType valueType, JSONNumberType numberType)
{
    int type = (valueType << 16) | numberType;
    if (type == type_)
        return;

    switch (GetValueType())
    {
    case JSON_STRING:
        delete stringValue_;
        break;

    case JSON_ARRAY:
        delete arrayValue_;
        break;

    case JSON_OBJECT:
        delete objectValue_;
        break;

    default:
        break;
    }

    type_ = type;

    switch (GetValueType())
    {
    case JSON_STRING:
        stringValue_ = new QString();
        break;

    case JSON_ARRAY:
        arrayValue_ = new JSONArray();
        break;

    case JSON_OBJECT:
        objectValue_ = new JSONObject();
        break;

    default:
        break;
    }
}

void JSONValue::SetVariant(const Variant& variant, Context* context)
{
    if (!IsNull())
    {
        URHO3D_LOGWARNING("JsonValue is not null");
    }

    (*this)["type"] = variant.GetTypeName();
    (*this)["value"].SetVariantValue(variant, context);
}

Variant JSONValue::GetVariant() const
{
    VariantType type = Variant::GetTypeFromName((*this)["type"].GetString());
    return (*this)["value"].GetVariantValue(type);
}

void JSONValue::SetVariantValue(const Variant& variant, Context* context)
{
    if (!IsNull())
    {
        URHO3D_LOGWARNING("JsonValue is not null");
    }

    switch (variant.GetType())
    {
    case VAR_BOOL:
        *this = variant.GetBool();
        return;

    case VAR_INT:
        *this = variant.GetInt();
        return;

    case VAR_FLOAT:
        *this = variant.GetFloat();
        return;

    case VAR_DOUBLE:
        *this = variant.GetDouble();
        return;

    case VAR_STRING:
        *this = variant.GetString();
        return;

    case VAR_VARIANTVECTOR:
        SetVariantVector(variant.GetVariantVector(), context);
        return;

    case VAR_VARIANTMAP:
        SetVariantMap(variant.GetVariantMap(), context);
        return;

    case VAR_RESOURCEREF:
        {
            if (!context)
            {
                URHO3D_LOGERROR("Context must not be null for ResourceRef");
                return;
            }

            const ResourceRef& ref = variant.GetResourceRef();
            *this = QString(context->GetTypeName(ref.type_)) + ";" + ref.name_;
        }
        return;

    case VAR_RESOURCEREFLIST:
        {
            if (!context)
            {
                URHO3D_LOGERROR("Context must not be null for ResourceRefList");
                return;
            }

            const ResourceRefList& refList = variant.GetResourceRefList();
            QString str(context->GetTypeName(refList.type_));
            for (unsigned i = 0; i < refList.names_.size(); ++i)
            {
                str += ";";
                str += refList.names_[i];
            }
            *this = str;
        }
        return;

    case VAR_STRINGVECTOR:
        {
            const QStringList& vector = variant.GetStringVector();
            Resize(vector.size());
            for (unsigned i = 0; i < vector.size(); ++i)
                (*this)[i] = vector[i];
        }
        return;

    default:
        *this = variant.ToString();
    }
}

Variant JSONValue::GetVariantValue(VariantType type) const
{
    Variant variant;
    switch (type)
    {
    case VAR_BOOL:
        variant = GetBool();
        break;

    case VAR_INT:
        variant = GetInt();
        break;

    case VAR_FLOAT:
        variant = GetFloat();
        break;

    case VAR_DOUBLE:
        variant = GetDouble();
        break;

    case VAR_STRING:
        variant = GetString();
        break;

    case VAR_VARIANTVECTOR:
        variant = GetVariantVector();
        break;

    case VAR_VARIANTMAP:
        variant = GetVariantMap();
        break;

    case VAR_RESOURCEREF:
        {
            ResourceRef ref;
            QStringList values = GetString().split(';');
            if (values.size() == 2)
            {
                ref.type_ = values[0];
                ref.name_ = values[1];
            }
            variant = ref;
        }
        break;

    case VAR_RESOURCEREFLIST:
        {
            ResourceRefList refList;
            QStringList values = GetString().split(';');
            if (values.size() >= 1)
            {
                refList.type_ = values[0];
                refList.names_.resize(values.size() - 1);
                for (unsigned i = 1; i < values.size(); ++i)
                    refList.names_[i - 1] = values[i];
            }
            variant = refList;
        }
        break;

    case VAR_STRINGVECTOR:
        {
            QStringList vector;
            for (unsigned i = 0; i < Size(); ++i)
                vector.push_back((*this)[i].GetString());
            variant = vector;
        }
        break;

    default:
        variant.FromString(type, GetString());
    }

    return variant;
}

void JSONValue::SetVariantMap(const VariantMap& variantMap, Context* context)
{
    SetType(JSON_OBJECT);
    for (VariantMap::const_iterator i = variantMap.begin(); i != variantMap.end(); ++i)
        (*this)[MAP_KEY(i).ToString()].SetVariant(MAP_VALUE(i));
}

VariantMap JSONValue::GetVariantMap() const
{
    VariantMap variantMap;
    if (!IsObject())
    {
        URHO3D_LOGERROR("JSONValue is not a object");
        return variantMap;
    }
    for (const auto & i : *objectValue_)
    {
        StringHash key(ELEMENT_KEY(i).toUInt());
        Variant variant = ELEMENT_VALUE(i).GetVariant();
        variantMap[key] = variant;
    }

    return variantMap;
}

void JSONValue::SetVariantVector(const VariantVector& variantVector, Context* context)
{
    SetType(JSON_ARRAY);
    arrayValue_->reserve(variantVector.size());
    for (unsigned i = 0; i < variantVector.size(); ++i)
    {
        JSONValue val;
        val.SetVariant(variantVector[i], context);
        arrayValue_->push_back(val);
    }
}

VariantVector JSONValue::GetVariantVector() const
{
    VariantVector variantVector;
    if (!IsArray())
    {
        URHO3D_LOGERROR("JSONValue is not a array");
        return variantVector;
    }

    for (unsigned i = 0; i < Size(); ++i)
    {
        Variant variant = (*this)[i].GetVariant();
        variantVector.push_back(variant);
    }

    return variantVector;
}

QString JSONValue::GetValueTypeName(JSONValueType type)
{
    return valueTypeNames[type];
}

QString JSONValue::GetNumberTypeName(JSONNumberType type)
{
    return numberTypeNames[type];
}

JSONValueType JSONValue::GetValueTypeFromName(const QString& typeName)
{
    return GetValueTypeFromName(qPrintable(typeName));
}

JSONValueType JSONValue::GetValueTypeFromName(const char* typeName)
{
    return (JSONValueType)GetStringListIndex(typeName, valueTypeNames, JSON_NULL);
}

JSONNumberType JSONValue::GetNumberTypeFromName(const QString& typeName)
{
    return GetNumberTypeFromName(qPrintable(typeName));
}

JSONNumberType JSONValue::GetNumberTypeFromName(const char* typeName)
{
    return (JSONNumberType)GetStringListIndex(typeName, numberTypeNames, JSONNT_NAN);
}

}
