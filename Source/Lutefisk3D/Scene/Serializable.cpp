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

#include "Serializable.h"

#include "../Core/Context.h"
#include "../IO/Deserializer.h"
#include "../IO/Log.h"
#include "ReplicationState.h"
#include "SceneEvents.h"
#include "../IO/Serializer.h"
#include "../Resource/XMLElement.h"

namespace Urho3D
{

static unsigned RemapAttributeIndex(const std::vector<AttributeInfo>* attributes, const AttributeInfo& netAttr, unsigned netAttrIndex)
{
    if (!attributes)
        return netAttrIndex; // Could not remap

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        // Compare either the accessor or offset to avoid name string compare
        if (attr.accessor_.Get() && attr.accessor_.Get() == netAttr.accessor_.Get())
            return i;
        else if (!attr.accessor_.Get() && attr.offset_ == netAttr.offset_)
            return i;
    }

    return netAttrIndex; // Could not remap
}
Serializable::Serializable(Context* context) :
    Object(context),
    networkState_(nullptr),
    instanceDefaultValues_(nullptr),
    temporary_(false)
{
}

Serializable::~Serializable()
{
    delete networkState_;
    networkState_ = nullptr;
    delete instanceDefaultValues_;
    instanceDefaultValues_ = nullptr;
}

void Serializable::OnSetAttribute(const AttributeInfo& attr, const Variant& src)
{
    // Check for accessor function mode
    if (attr.accessor_)
    {
        attr.accessor_->Set(this, src);
        return;
    }

    // Calculate the destination address
    void* dest = attr.ptr_ ? attr.ptr_ : reinterpret_cast<unsigned char*>(this) + attr.offset_;

    switch (attr.type_)
    {
    case VAR_INT:
        // If enum type, use the low 8 bits only
        if (attr.enumNames_)
            *(reinterpret_cast<unsigned char*>(dest)) = src.GetInt();
        else
            *(reinterpret_cast<int*>(dest)) = src.GetInt();
        break;

    case VAR_BOOL:
        *(reinterpret_cast<bool*>(dest)) = src.GetBool();
        break;

    case VAR_FLOAT:
        *(reinterpret_cast<float*>(dest)) = src.GetFloat();
        break;

    case VAR_VECTOR2:
        *(reinterpret_cast<Vector2*>(dest)) = src.GetVector2();
        break;

    case VAR_VECTOR3:
        *(reinterpret_cast<Vector3*>(dest)) = src.GetVector3();
        break;

    case VAR_VECTOR4:
        *(reinterpret_cast<Vector4*>(dest)) = src.GetVector4();
        break;

    case VAR_QUATERNION:
        *(reinterpret_cast<Quaternion*>(dest)) = src.GetQuaternion();
        break;

    case VAR_COLOR:
        *(reinterpret_cast<Color*>(dest)) = src.GetColor();
        break;

    case VAR_STRING:
        *(reinterpret_cast<QString*>(dest)) = src.GetString();
        break;

    case VAR_BUFFER:
        *(reinterpret_cast<std::vector<unsigned char>*>(dest)) = src.GetBuffer();
        break;

    case VAR_RESOURCEREF:
        *(reinterpret_cast<ResourceRef*>(dest)) = src.GetResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        *(reinterpret_cast<ResourceRefList*>(dest)) = src.GetResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        *(reinterpret_cast<VariantVector*>(dest)) = src.GetVariantVector();
        break;

    case VAR_VARIANTMAP:
        *(reinterpret_cast<VariantMap*>(dest)) = src.GetVariantMap();
        break;

    case VAR_INTRECT:
        *(reinterpret_cast<IntRect*>(dest)) = src.GetIntRect();
        break;

    case VAR_INTVECTOR2:
        *(reinterpret_cast<IntVector2*>(dest)) = src.GetIntVector2();
        break;

    default:
        LOGERROR("Unsupported attribute type for OnSetAttribute()");
        return;
    }

    // If it is a network attribute then mark it for next network update
    if (attr.mode_ & AM_NET)
        MarkNetworkUpdate();
}

void Serializable::OnGetAttribute(const AttributeInfo& attr, Variant& dest) const
{
    // Check for accessor function mode
    if (attr.accessor_)
    {
        attr.accessor_->Get(this, dest);
        return;
    }

    // Calculate the source address
    const void* src = attr.ptr_ ? attr.ptr_ : reinterpret_cast<const unsigned char*>(this) + attr.offset_;

    switch (attr.type_)
    {
    case VAR_INT:
        // If enum type, use the low 8 bits only
        if (attr.enumNames_)
            dest = *(reinterpret_cast<const unsigned char*>(src));
        else
            dest = *(reinterpret_cast<const int*>(src));
        break;

    case VAR_BOOL:
        dest = *(reinterpret_cast<const bool*>(src));
        break;

    case VAR_FLOAT:
        dest = *(reinterpret_cast<const float*>(src));
        break;

    case VAR_VECTOR2:
        dest = *(reinterpret_cast<const Vector2*>(src));
        break;

    case VAR_VECTOR3:
        dest = *(reinterpret_cast<const Vector3*>(src));
        break;

    case VAR_VECTOR4:
        dest = *(reinterpret_cast<const Vector4*>(src));
        break;

    case VAR_QUATERNION:
        dest = *(reinterpret_cast<const Quaternion*>(src));
        break;

    case VAR_COLOR:
        dest = *(reinterpret_cast<const Color*>(src));
        break;

    case VAR_STRING:
        dest = *(reinterpret_cast<const QString*>(src));
        break;

    case VAR_BUFFER:
        dest = *(reinterpret_cast<const std::vector<unsigned char>*>(src));
        break;

    case VAR_RESOURCEREF:
        dest = *(reinterpret_cast<const ResourceRef*>(src));
        break;

    case VAR_RESOURCEREFLIST:
        dest = *(reinterpret_cast<const ResourceRefList*>(src));
        break;

    case VAR_VARIANTVECTOR:
        dest = *(reinterpret_cast<const VariantVector*>(src));
        break;

    case VAR_VARIANTMAP:
        dest = *(reinterpret_cast<const VariantMap*>(src));
        break;

    case VAR_INTRECT:
        dest = *(reinterpret_cast<const IntRect*>(src));
        break;

    case VAR_INTVECTOR2:
        dest = *(reinterpret_cast<const IntVector2*>(src));
        break;

    default:
        LOGERROR("Unsupported attribute type for OnGetAttribute()");
        return;
    }
}

const std::vector<AttributeInfo>* Serializable::GetAttributes() const
{
    return context_->GetAttributes(GetType());
}

const std::vector<AttributeInfo>* Serializable::GetNetworkAttributes() const
{
    return networkState_ ? networkState_->attributes_ : context_->GetNetworkAttributes(GetType());
}

bool Serializable::Load(Deserializer& source, bool setInstanceDefault)
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!(attr.mode_ & AM_FILE))
            continue;

        if (source.IsEof())
        {
            LOGERROR("Could not load " + GetTypeName() + ", stream not open or at end");
            return false;
        }

        Variant varValue = source.ReadVariant(attr.type_);
        OnSetAttribute(attr, varValue);

        if (setInstanceDefault)
            SetInstanceDefault(attr.name_, varValue);
    }

    return true;
}

bool Serializable::Save(Serializer& dest) const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    Variant value;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!(attr.mode_ & AM_FILE))
            continue;

        OnGetAttribute(attr, value);

        if (!dest.WriteVariantData(value))
        {
            LOGERROR("Could not save " + GetTypeName() + ", writing to stream failed");
            return false;
        }
    }

    return true;
}

bool Serializable::LoadXML(const XMLElement& source, bool setInstanceDefault)
{
    if (source.IsNull())
    {
        LOGERROR("Could not load " + GetTypeName() + ", null source element");
        return false;
    }

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    XMLElement attrElem = source.GetChild("attribute");
    unsigned startIndex = 0;

    while (attrElem)
    {
        QString name = attrElem.GetAttribute("name");
        unsigned i = startIndex;
        unsigned attempts = attributes->size();

        while (attempts)
        {
            const AttributeInfo& attr = attributes->at(i);
            if ((attr.mode_ & AM_FILE) && !attr.name_.compare(name, Qt::CaseInsensitive))
            {
                Variant varValue;

                // If enums specified, do enum lookup and int assignment. Otherwise assign the variant directly
                if (attr.enumNames_)
                {
                    QString value = attrElem.GetAttribute("value");
                    bool enumFound = false;
                    int enumValue = 0;
                    const char** enumPtr = attr.enumNames_;
                    while (*enumPtr)
                    {
                        if (!value.compare(*enumPtr))
                        {
                            enumFound = true;
                            break;
                        }
                        ++enumPtr;
                        ++enumValue;
                    }
                    if (enumFound)
                        varValue = enumValue;
                    else
                        LOGWARNING("Unknown enum value " + value + " in attribute " + attr.name_);
                }
                else
                    varValue = attrElem.GetVariantValue(attr.type_);

                if (!varValue.IsEmpty())
                {
                    OnSetAttribute(attr, varValue);

                    if (setInstanceDefault)
                        SetInstanceDefault(attr.name_, varValue);
                }

                startIndex = (i + 1) % attributes->size();
                break;
            }
            else
            {
                i = (i + 1) % attributes->size();
                --attempts;
            }
        }

        if (!attempts)
            LOGWARNING("Unknown attribute " + name + " in XML data");

        attrElem = attrElem.GetNext("attribute");
    }

    return true;
}

bool Serializable::SaveXML(XMLElement& dest) const
{
    if (dest.IsNull())
    {
        LOGERROR("Could not save " + GetTypeName() + ", null destination element");
        return false;
    }

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    Variant value;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!(attr.mode_ & AM_FILE))
            continue;

        OnGetAttribute(attr, value);
        Variant defaultValue(GetAttributeDefault(i));

        // In XML serialization default values can be skipped. This will make the file easier to read or edit manually
        if (value == defaultValue && !SaveDefaultAttributes())
            continue;

        XMLElement attrElem = dest.CreateChild("attribute");
        attrElem.SetAttribute("name", attr.name_);
        // If enums specified, set as an enum string. Otherwise set directly as a Variant
        if (attr.enumNames_)
        {
            int enumValue = value.GetInt();
            attrElem.SetAttribute("value", attr.enumNames_[enumValue]);
        }
        else
            attrElem.SetVariantValue(value);
    }

    return true;
}

bool Serializable::SetAttribute(unsigned index, const Variant& value)
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        LOGERROR(GetTypeName() + " has no attributes");
        return false;
    }
    if (index >= attributes->size())
    {
        LOGERROR("Attribute index out of bounds");
        return false;
    }

    const AttributeInfo& attr = attributes->at(index);

    // Check that the new value's type matches the attribute type
    if (value.GetType() == attr.type_)
    {
        OnSetAttribute(attr, value);
        return true;
    }
    else
    {
        LOGERROR("Could not set attribute " + attr.name_ + ": expected type " + Variant::GetTypeName(attr.type_) +
            " but got " + value.GetTypeName());
        return false;
    }
}

bool Serializable::SetAttribute(const QString& name, const Variant& value)
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        LOGERROR(GetTypeName() + " has no attributes");
        return false;
    }

    for (std::vector<AttributeInfo>::const_iterator i = attributes->begin(); i != attributes->end(); ++i)
    {
        if (!i->name_.compare(name, Qt::CaseInsensitive))
        {
            // Check that the new value's type matches the attribute type
            if (value.GetType() == i->type_)
            {
                OnSetAttribute(*i, value);
                return true;
            }
            else
            {
                LOGERROR("Could not set attribute " + i->name_ + ": expected type " + Variant::GetTypeName(i->type_)
                    + " but got " + value.GetTypeName());
                return false;
            }
        }
    }

    LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return false;
}

void Serializable::ResetToDefault()
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (attr.mode_ & (AM_NOEDIT | AM_NODEID | AM_COMPONENTID | AM_NODEIDVECTOR))
            continue;

        Variant defaultValue = GetInstanceDefault(attr.name_);
        if (defaultValue.IsEmpty())
            defaultValue = attr.defaultValue_;

        OnSetAttribute(attr, defaultValue);
    }
}

void Serializable::RemoveInstanceDefault()
{
    delete instanceDefaultValues_;
    instanceDefaultValues_ = nullptr;
}

void Serializable::SetTemporary(bool enable)
{
    if (enable != temporary_)
    {
        temporary_ = enable;

        using namespace TemporaryChanged;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_SERIALIZABLE] = this;

        SendEvent(E_TEMPORARYCHANGED, eventData);
    }
}

void Serializable::SetInterceptNetworkUpdate(const QString& attributeName, bool enable)
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return;

    AllocateNetworkState();

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.name_.compare(attributeName, Qt::CaseInsensitive))
        {
            if (enable)
                networkState_->interceptMask_ |= 1ULL << i;
            else
                networkState_->interceptMask_ &= ~(1ULL << i);
            break;
        }
    }
}

void Serializable::AllocateNetworkState()
{
    if (!networkState_)
    {
        const std::vector<AttributeInfo>* networkAttributes = GetNetworkAttributes();
        networkState_ = new NetworkState();
        networkState_->attributes_ = networkAttributes;
    }
}

void Serializable::WriteInitialDeltaUpdate(Serializer& dest, unsigned char timeStamp)
{
    if (!networkState_)
    {
        LOGERROR("WriteInitialDeltaUpdate called without allocated NetworkState");
        return;
    }

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    unsigned numAttributes = attributes->size();
    DirtyBits attributeBits;

    // Compare against defaults
    for (unsigned i = 0; i < numAttributes; ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (networkState_->currentValues_[i] != attr.defaultValue_)
            attributeBits.Set(i);
    }

    // First write the change bitfield, then attribute data for non-default attributes
    dest.WriteUByte(timeStamp);
    dest.Write(attributeBits.data_, (numAttributes + 7) >> 3);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributeBits.IsSet(i))
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

void Serializable::WriteDeltaUpdate(Serializer& dest, const DirtyBits& attributeBits, unsigned char timeStamp)
{
    if (!networkState_)
    {
        LOGERROR("WriteDeltaUpdate called without allocated NetworkState");
        return;
    }

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    unsigned numAttributes = attributes->size();

    // First write the change bitfield, then attribute data for changed attributes
    // Note: the attribute bits should not contain LATESTDATA attributes
    dest.WriteUByte(timeStamp);
    dest.Write(attributeBits.data_, (numAttributes + 7) >> 3);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributeBits.IsSet(i))
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

void Serializable::WriteLatestDataUpdate(Serializer& dest, unsigned char timeStamp)
{
    if (!networkState_)
    {
        LOGERROR("WriteLatestDataUpdate called without allocated NetworkState");
        return;
    }

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    unsigned numAttributes = attributes->size();
    dest.WriteUByte(timeStamp);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributes->at(i).mode_ & AM_LATESTDATA)
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

bool Serializable::ReadDeltaUpdate(Deserializer& source)
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return false;

    unsigned numAttributes = attributes->size();
    DirtyBits attributeBits;
    bool changed = false;

    unsigned long long interceptMask = networkState_ ? networkState_->interceptMask_ : 0;
    unsigned char timeStamp = source.ReadUByte();
    source.Read(attributeBits.data_, (numAttributes + 7) >> 3);

    for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
    {
        if (attributeBits.IsSet(i))
        {
            const AttributeInfo& attr = attributes->at(i);
            if (!(interceptMask & (1ULL << i)))
            {
            OnSetAttribute(attr, source.ReadVariant(attr.type_));
                changed = true;
            }
            else
            {
                using namespace InterceptNetworkUpdate;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_SERIALIZABLE] = this;
                eventData[P_TIMESTAMP] = (unsigned)timeStamp;
                eventData[P_INDEX] = RemapAttributeIndex(GetAttributes(), attr, i);
                eventData[P_NAME] = attr.name_;
                eventData[P_VALUE] = source.ReadVariant(attr.type_);
                SendEvent(E_INTERCEPTNETWORKUPDATE, eventData);
        }
    }
}

    return changed;
}

bool Serializable::ReadLatestDataUpdate(Deserializer& source)
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return false;

    unsigned numAttributes = attributes->size();
    bool changed = false;

    unsigned long long interceptMask = networkState_ ? networkState_->interceptMask_ : 0;
    unsigned char timeStamp = source.ReadUByte();

    for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (attr.mode_ & AM_LATESTDATA)
        {
            if (!(interceptMask & (1ULL << i)))
            {
            OnSetAttribute(attr, source.ReadVariant(attr.type_));
                changed = true;
    }
            else
            {
                using namespace InterceptNetworkUpdate;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_SERIALIZABLE] = this;
                eventData[P_TIMESTAMP] = (unsigned)timeStamp;
                eventData[P_INDEX] = RemapAttributeIndex(GetAttributes(), attr, i);
                eventData[P_NAME] = attr.name_;
                eventData[P_VALUE] = source.ReadVariant(attr.type_);
                SendEvent(E_INTERCEPTNETWORKUPDATE, eventData);
            }
        }
    }

    return changed;
}

Variant Serializable::GetAttribute(unsigned index) const
{
    Variant ret;

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        LOGERROR(GetTypeName() + " has no attributes");
        return ret;
    }
    if (index >= attributes->size())
    {
        LOGERROR("Attribute index out of bounds");
        return ret;
    }

    OnGetAttribute(attributes->at(index), ret);
    return ret;
}

Variant Serializable::GetAttribute(const QString& name) const
{
    Variant ret;

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        LOGERROR(GetTypeName() + " has no attributes");
        return ret;
    }

    for (const AttributeInfo & attribute : *attributes)
    {
        if (!attribute.name_.compare(name, Qt::CaseInsensitive))
        {
            OnGetAttribute(attribute, ret);
            return ret;
        }
    }

    LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return ret;
}

Variant Serializable::GetAttributeDefault(unsigned index) const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        LOGERROR(GetTypeName() + " has no attributes");
        return Variant::EMPTY;
    }
    if (index >= attributes->size())
    {
        LOGERROR("Attribute index out of bounds");
        return Variant::EMPTY;
    }

    AttributeInfo attr = attributes->at(index);
    Variant defaultValue = GetInstanceDefault(attr.name_);
    return defaultValue.IsEmpty() ? attr.defaultValue_ : defaultValue;
}

Variant Serializable::GetAttributeDefault(const QString& name) const
{
    Variant defaultValue = GetInstanceDefault(name);
    if (!defaultValue.IsEmpty())
        return defaultValue;

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        LOGERROR(GetTypeName() + " has no attributes");
        return Variant::EMPTY;
    }

    for (const AttributeInfo & attribute : *attributes)
    {
        if (!attribute.name_.compare(name, Qt::CaseInsensitive))
            return attribute.defaultValue_;
    }

    LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return Variant::EMPTY;
}

unsigned Serializable::GetNumAttributes() const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    return attributes ? attributes->size() : 0;
}

unsigned Serializable::GetNumNetworkAttributes() const
{
    const std::vector<AttributeInfo>* attributes = networkState_ ? networkState_->attributes_ :
        context_->GetNetworkAttributes(GetType());
    return attributes ? attributes->size() : 0;
}

bool Serializable::GetInterceptNetworkUpdate(const QString& attributeName) const
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return false;

    unsigned long long interceptMask = networkState_ ? networkState_->interceptMask_ : 0;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.name_.compare(attributeName, Qt::CaseInsensitive))
            return interceptMask & (1ULL << i) ? true : false;
    }

    return false;
}

void Serializable::SetInstanceDefault(const QString& name, const Variant& defaultValue)
{
    // Allocate the instance level default value
    if (!instanceDefaultValues_)
        instanceDefaultValues_ = new VariantMap();
    instanceDefaultValues_->operator[] (name) = defaultValue;
}

Variant Serializable::GetInstanceDefault(const QString& name) const
{
    if (instanceDefaultValues_)
    {
        VariantMap::const_iterator i = instanceDefaultValues_->find(name);
        if (i != instanceDefaultValues_->end())
            return MAP_VALUE(i);
    }

    return Variant::EMPTY;
}

}
