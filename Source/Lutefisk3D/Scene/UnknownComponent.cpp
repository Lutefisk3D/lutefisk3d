//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include "UnknownComponent.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Deserializer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/Serializer.h"
#include "Lutefisk3D/Resource/XMLElement.h"
#include "Lutefisk3D/Resource/JSONValue.h"

namespace Urho3D
{

static HashMap<StringHash, QString> unknownTypeToName;
static QLatin1String letters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

static QString GenerateNameFromType(StringHash typeHash)
{
    if (unknownTypeToName.contains(typeHash))
        return unknownTypeToName[typeHash];

    QString test;

    // Begin brute-force search
    unsigned numLetters = letters.size();
    unsigned combinations = numLetters;
    bool found = false;

    for (unsigned i = 1; i < 6; ++i)
    {
        test.resize(i);

        for (unsigned j = 0; j < combinations; ++j)
        {
            unsigned current = j;

            for (unsigned k = 0; k < i; ++k)
            {
                test[k] = letters.data()[current % numLetters];
                current /= numLetters;
            }

            if (StringHash(test) == typeHash)
            {
                found = true;
                break;
            }
        }

        if (found)
            break;

        combinations *= numLetters;
    }

    unknownTypeToName[typeHash] = test;
    return test;
}

UnknownComponent::UnknownComponent(Context* context) :
    Component(context),
    useXML_(false)
{
}

void UnknownComponent::RegisterObject(Context* context)
{
    context->RegisterFactory<UnknownComponent>();
}

bool UnknownComponent::Load(Deserializer& source, bool setInstanceDefault)
{
    useXML_ = false;
    xmlAttributes_.clear();
    xmlAttributeInfos_.clear();

    // Assume we are reading from a component data buffer, and the type has already been read
    unsigned dataSize = source.GetSize() - source.GetPosition();
    binaryAttributes_.resize(dataSize);
    return dataSize != 0u ? source.Read(&binaryAttributes_[0], dataSize) == dataSize : true;
    }

    bool UnknownComponent::LoadXML(const XMLElement& source, bool setInstanceDefault)
    {
    useXML_ = true;
    xmlAttributes_.clear();
    xmlAttributeInfos_.clear();
    binaryAttributes_.clear();

    XMLElement attrElem = source.GetChild("attribute");
    while (attrElem)
    {
        AttributeInfo attr;
        attr.mode_ = AM_FILE;
        attr.name_ = attrElem.GetAttribute("name");
        attr.type_ = VAR_STRING;

        if (!attr.name_.isEmpty())
        {
            QString attrValue = attrElem.GetAttribute("value");
            attr.defaultValue_ = QString();
            xmlAttributeInfos_.push_back(attr);
            xmlAttributes_.push_back(attrValue);
        }

        attrElem = attrElem.GetNext("attribute");
    }

    // Fix up pointers to the attributes after all have been read
    for (unsigned i = 0; i < xmlAttributeInfos_.size(); ++i)
        xmlAttributeInfos_[i].ptr_ = &xmlAttributes_[i];

    return true;
}

bool UnknownComponent::LoadJSON(const JSONValue& source, bool setInstanceDefault)
{
    useXML_ = true;
    xmlAttributes_.clear();
    xmlAttributeInfos_.clear();
    binaryAttributes_.clear();

    JSONArray attributesArray = source.Get("attributes").GetArray();
    for(const JSONValue& attrVal : attributesArray)
    {
        AttributeInfo attr;
        attr.mode_ = AM_FILE;
        attr.name_ = attrVal.Get("name").GetString();
        attr.type_ = VAR_STRING;

        if (!attr.name_.isEmpty())
        {
            QString attrValue = attrVal.Get("value").GetString();
            attr.defaultValue_ = QString();
            xmlAttributeInfos_.push_back(attr);
            xmlAttributes_.push_back(attrValue);
        }
    }

    // Fix up pointers to the attributes after all have been read
    for (unsigned i = 0; i < xmlAttributeInfos_.size(); ++i)
        xmlAttributeInfos_[i].ptr_ = &xmlAttributes_[i];

    return true;
}
bool UnknownComponent::Save(Serializer& dest) const
{
    if (useXML_)
        URHO3D_LOGWARNING("UnknownComponent loaded in XML mode, attributes will be empty for binary save");

    // Write type and ID
    if (!dest.WriteStringHash(GetType()))
        return false;
    if (!dest.WriteUInt(id_))
        return false;

    if (binaryAttributes_.size() == 0u)
        return true;

    return dest.Write(&binaryAttributes_[0], binaryAttributes_.size()) == binaryAttributes_.size();
}

bool UnknownComponent::SaveXML(XMLElement& dest) const
{
    if (dest.IsNull())
    {
        URHO3D_LOGERROR("Could not save " + GetTypeName() + ", null destination element");
        return false;
    }

    if (!useXML_)
        URHO3D_LOGWARNING("UnknownComponent loaded in binary or JSON mode, attributes will be empty for XML save");

    // Write type and ID
    if (!dest.SetString("type", GetTypeName()))
        return false;
    if (!dest.SetInt("id", id_))
        return false;

    for (unsigned i = 0; i < xmlAttributeInfos_.size(); ++i)
    {
        XMLElement attrElem = dest.CreateChild("attribute");
        attrElem.SetAttribute("name", xmlAttributeInfos_[i].name_);
        attrElem.SetAttribute("value", xmlAttributes_[i]);
    }
    return true;
}

bool UnknownComponent::SaveJSON(JSONValue& dest) const
{
    if (!useXML_)
        URHO3D_LOGWARNING("UnknownComponent loaded in binary mode, attributes will be empty for JSON save");

    // Write type and ID
    dest.Set("type", GetTypeName());
    dest.Set("id", (int) id_);

    JSONArray attributesArray;
    attributesArray.reserve(xmlAttributeInfos_.size());
    for (unsigned i = 0; i < xmlAttributeInfos_.size(); ++i)
    {
        JSONValue attrVal;
        attrVal.Set("name", xmlAttributeInfos_[i].name_);
        attrVal.Set("value", xmlAttributes_[i]);
        attributesArray.push_back(attrVal);
    }
    dest.Set("attributes", attributesArray);

    return true;
}

void UnknownComponent::SetTypeName(const QStringRef& typeName)
{
    typeName_ = typeName.toString();
    typeHash_ = typeName;
}

void UnknownComponent::SetType(StringHash typeHash)
{
    typeName_ = GenerateNameFromType(typeHash);
    typeHash_ = typeHash;
}

}
