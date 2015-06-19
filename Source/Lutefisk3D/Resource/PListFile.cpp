//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "PListFile.h"

#include "XMLFile.h"
#include "../Core/Context.h"
#include "../IO/Deserializer.h"
#include "../IO/Log.h"
#include <stdio.h>

namespace Urho3D
{

static PListValue EMPTY_VALUE;
static PListValueMap EMPTY_VALUEMAP;
static PListValueVector EMPTY_VALUEVECTOR;

PListValue& PListValueMap::operator [](const QString& key)
{
    return HashMap<QString, PListValue>::operator [](key);
}

const PListValue& PListValueMap::operator [](const QString& key) const
{
    const_iterator i = find(key);
    if (i == end())
        return EMPTY_VALUE;

    return MAP_VALUE(i);
}

PListValue::PListValue() : type_(PLVT_NONE)
{
}

PListValue::PListValue(int value) : type_(PLVT_NONE)
{
    SetInt(value);
}

PListValue::PListValue(bool value) : type_(PLVT_NONE)
{
    SetBool(value);
}

PListValue::PListValue(float value) : type_(PLVT_NONE)
{
    SetFloat(value);
}

PListValue::PListValue(const QString& value) : type_(PLVT_NONE)
{
    SetString(value);
}

PListValue::PListValue(PListValueMap& valueMap) : type_(PLVT_NONE)
{
    SetValueMap(valueMap);
}

PListValue::PListValue(PListValueVector& valueVector) : type_(PLVT_NONE)
{
    SetValueVector(valueVector);
}

PListValue::PListValue(const PListValue& value) : type_(PLVT_NONE)
{
    *this = value;
}

PListValue::~PListValue()
{
    Reset();
}

PListValue& PListValue::operator = (const PListValue& rhs)
{
    switch (rhs.type_)
    {
    case PLVT_NONE:
        Reset();
        break;
    case PLVT_INT:
        SetInt(rhs.int_);
        break;
    case PLVT_BOOL:
        SetBool(rhs.bool_);
        break;
    case PLVT_FLOAT:
        SetFloat(rhs.float_);
        break;
    case PLVT_STRING:
        SetString(*rhs.string_);
        break;
    case PLVT_VALUEMAP:
        SetValueMap(*rhs.valueMap_);
        break;
    case PLVT_VALUEVECTOR:
        SetValueVector(*rhs.valueVector_);
        break;
    }

    return *this;
}

void PListValue::SetInt(int value)
{
    if (type_ != PLVT_INT)
    {
        Reset();
        type_ = PLVT_INT;
    }

    int_ = value;
}

void PListValue::SetBool(bool value)
{
    if (type_ != PLVT_BOOL)
    {
        Reset();
        type_ = PLVT_BOOL;
    }

    bool_ = value;
}

void PListValue::SetFloat(float value)
{
    if (type_ != PLVT_FLOAT)
    {
        Reset();
        type_ = PLVT_FLOAT;
    }
    float_ = value;
}

void PListValue::SetString(const QString& value)
{
    if (type_ != PLVT_STRING)
    {
        Reset();
        type_ = PLVT_STRING;
        string_ = new QString();
    }

    *string_ = value;
}

void PListValue::SetValueMap(const PListValueMap& valueMap)
{
    if (type_ != PLVT_VALUEMAP)
    {
        Reset();
        type_ = PLVT_VALUEMAP;
        valueMap_ = new PListValueMap();
    }

    *valueMap_ = valueMap;
}

void PListValue::SetValueVector(const PListValueVector& valueVector)
{
    if (type_ != PLVT_VALUEVECTOR)
    {
        Reset();
        type_ = PLVT_VALUEVECTOR;
        valueVector_ = new PListValueVector();
    }

    *valueVector_ = valueVector;
}

int PListValue::GetInt() const
{
    return type_ == PLVT_INT ? int_ : 0;
}

bool PListValue::GetBool() const
{
    return type_ == PLVT_BOOL ? bool_ : false;
}

float PListValue::GetFloat() const
{
    return type_ == PLVT_FLOAT? float_ : 0.0f;
}

const QString& PListValue::GetString() const
{
    return type_ == PLVT_STRING ? * string_ : s_dummy;
}

IntRect PListValue::GetIntRect() const
{
    if (type_ != PLVT_STRING)
        return IntRect::ZERO;

    int x, y, w, h;
    sscanf(qPrintable(*string_), "{{%d,%d},{%d,%d}}", &x, &y, &w, &h);
    return IntRect(x, y, x + w, y + h);
}

IntVector2 PListValue::GetIntVector2() const
{
    if (type_ != PLVT_STRING)
        return IntVector2::ZERO;

    int x, y;
    sscanf(qPrintable(*string_), "{%d,%d}", &x, &y);
    return IntVector2(x, y);
}

const PListValueMap& PListValue::GetValueMap() const
{
    return type_ == PLVT_VALUEMAP ? *valueMap_ : EMPTY_VALUEMAP;
}

const PListValueVector& PListValue::GetValueVector() const
{
    return type_ == PLVT_VALUEVECTOR? *valueVector_ : EMPTY_VALUEVECTOR;
}

PListValueMap& PListValue::ConvertToValueMap()
{
    if (type_ != PLVT_VALUEMAP)
    {
        Reset();
        type_ = PLVT_VALUEMAP;
        valueMap_ = new PListValueMap();
    }

    return *valueMap_;
}

PListValueVector& PListValue::ConvertToValueVector()
{
    if (type_ != PLVT_VALUEVECTOR)
    {
        Reset();
        type_ = PLVT_VALUEVECTOR;
        valueVector_ = new PListValueVector();
    }

    return *valueVector_;
}
void PListValue::Reset()
{
    if (type_ == PLVT_NONE)
        return;

    switch (type_)
    {
    case PLVT_STRING:
        delete string_;
        break;
    case PLVT_VALUEMAP:
        delete valueMap_;
        break;
    case PLVT_VALUEVECTOR:
        delete valueVector_;
        break;
    default:
        break;
    }

    type_ = PLVT_NONE;
}

PListFile::PListFile(Context* context) :
    Resource(context)
{
}

PListFile::~PListFile()
{
}

void PListFile::RegisterObject(Context* context)
{
    context->RegisterFactory<PListFile>();
}

bool PListFile::BeginLoad(Deserializer& source)
{
    if (GetName().isEmpty())
        SetName(source.GetName());

    XMLFile xmlFile(context_);
    if (!xmlFile.Load(source))
    {
        LOGERROR("Could not load property list");
        return false;
    }

    XMLElement plistElem = xmlFile.GetRoot("plist");
    if (!plistElem)
    {
        LOGERROR("Invalid property list file");
        return false;
    }

    root_.clear();

    XMLElement dictElem = plistElem.GetChild("dict");
    if (!LoadDict(root_, dictElem))
        return false;

    SetMemoryUse(source.GetSize());

    return true;
}

bool PListFile::LoadDict(PListValueMap& dict, const XMLElement& dictElem)
{
    if (!dictElem)
        return false;

    XMLElement keyElem = dictElem.GetChild("key");
    XMLElement valueElem = keyElem.GetNext();
    while (keyElem && valueElem)
    {
        QString key = keyElem.GetValue();
        XMLElement valueElem = keyElem.GetNext();

        PListValue value;
        if (!LoadValue(value, valueElem))
            return false;

        dict[key] = value;

        keyElem = valueElem.GetNext("key");
        valueElem = keyElem.GetNext();
    }

    return true;
}

bool PListFile::LoadArray(PListValueVector& array, const XMLElement& arrayElem)
{
    if (!arrayElem)
        return false;

    for (XMLElement valueElem = arrayElem.GetChild(); valueElem; valueElem = valueElem.GetNext())
    {
        PListValue value;

        if (!LoadValue(value, valueElem))
            return false;

        array.push_back(value);
    }

    return true;
}

bool PListFile::LoadValue(PListValue& value, XMLElement valueElem)
{
    QString valueType = valueElem.GetName();

    if (valueType == "string")
        value.SetString(valueElem.GetValue());
    else if (valueType == "real")
        value.SetFloat(valueElem.GetValue().toFloat());
    else if (valueType == "integer")
        value.SetInt(valueElem.GetValue().toInt());
    else if (valueType == "true")
        value.SetBool(true);
    else if (valueType == "false")
        value.SetBool(false);
    else if (valueType == "dict")
    {
        if (!LoadDict(value.ConvertToValueMap(), valueElem))
            return false;
    }
    else if (valueType == "array")
    {
        if (!LoadArray(value.ConvertToValueVector(), valueElem))
            return false;
    }
    else
    {
        LOGERROR("Supported value type");
        return false;
    }

    return true;
}


}