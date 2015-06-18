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

#pragma once

#include "../Container/HashMap.h"
#include "../Container/Ptr.h"
#include "../Math/StringHash.h"
#include <QtCore/QVariant>

namespace Urho3D
{

/// Typed resource reference.
struct ResourceRef
{
    /// Construct.
    ResourceRef()
    {
    }

    /// Construct with type only and empty id.
    ResourceRef(StringHash type) :
        type_(type)
    {
    }

    /// Construct with type and resource name.
    ResourceRef(StringHash type, const QString& name) :
        type_(type),
        name_(name)
    {
    }

    // Construct from another ResourceRef.
    ResourceRef(const ResourceRef& rhs) :
        type_(rhs.type_),
        name_(rhs.name_)
    {
    }

    /// Object type.
    StringHash type_;
    /// Object name.
    QString name_;

    /// Test for equality with another reference.
    bool operator == (const ResourceRef& rhs) const { return type_ == rhs.type_ && name_ == rhs.name_; }
    /// Test for inequality with another reference.
    bool operator != (const ResourceRef& rhs) const { return type_ != rhs.type_ || name_ != rhs.name_; }
};

/// %List of typed resource references.
struct ResourceRefList
{
    /// Construct.
    ResourceRefList()
    {
    }

    /// Construct with type only.
    ResourceRefList(StringHash type) :
        type_(type)
    {
    }
    /// Construct with type and id list.
    ResourceRefList(StringHash type, const std::vector<QString>& names) :
        type_(type),
        names_(names)
    {
    }

    /// Object type.
    StringHash type_;
    /// List of object names.
    std::vector<QString> names_;
    ResourceRefList &operator=(const ResourceRefList &rhs) = default;
    /// Test for equality with another reference list.
    bool operator == (const ResourceRefList& rhs) const { return type_ == rhs.type_ && names_ == rhs.names_; }
    /// Test for inequality with another reference list.
    bool operator != (const ResourceRefList& rhs) const { return type_ != rhs.type_ || names_ != rhs.names_; }
};

/// Map of variants.
typedef HashMap<StringHash, QVariant> VariantMap;
}
Q_DECLARE_METATYPE(Urho3D::ResourceRef)
Q_DECLARE_METATYPE(Urho3D::ResourceRefList)
Q_DECLARE_METATYPE(Urho3D::VariantMap)
