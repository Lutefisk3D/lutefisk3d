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

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Core/Timer.h"
#include "Lutefisk3D/Resource/JSONValue.h"
#include "Lutefisk3D/Resource/ResourceEvents.h"
#include "Lutefisk3D/Resource/ResourceDecls.h"
#include "Lutefisk3D/Core/Variant.h" // for resourceref
namespace Urho3D
{

class Deserializer;
class Serializer;
class XMLElement;

/// Asynchronous loading state of a resource.
enum AsyncLoadState
{
    /// No async operation in progress.
    ASYNC_DONE = 0,
    /// Queued for asynchronous loading.
    ASYNC_QUEUED = 1,
    /// In progress of calling BeginLoad() in a worker thread.
    ASYNC_LOADING = 2,
    /// BeginLoad() succeeded. EndLoad() can be called in the main thread.
    ASYNC_SUCCESS = 3,
    /// BeginLoad() failed.
    ASYNC_FAIL = 4
};



/// Base class for resources.
class LUTEFISK3D_EXPORT Resource : public Object, public SingleResourceSignals
{
    URHO3D_OBJECT(Resource, Object)

public:
    /// Construct.
    Resource(Context* context);
    bool Load(Deserializer& source);
    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source)=0;
    virtual bool EndLoad();
    virtual bool Save(Serializer& dest) const;
    void SetName(const QString& name);
    void SetMemoryUse(size_t size);
    void ResetUseTimer();

    void SetAsyncLoadState(AsyncLoadState newState);

    /// Return name.
    const QString& GetName() const { return name_; }
    /// Return name hash.
    StringHash GetNameHash() const { return nameHash_; }
    /// Return memory use in bytes, possibly approximate.
    unsigned GetMemoryUse() const { return memoryUse_; }
    /// Return time since last use in milliseconds. If referred to elsewhere than in the resource cache, returns always zero.
    unsigned GetUseTimer();
    /// Return the asynchronous loading state.
    AsyncLoadState GetAsyncLoadState() const { return asyncLoadState_; }

private:
    QString        name_;           ///< Name.
    StringHash     nameHash_;       ///< Name hash.
    Timer          useTimer_;       ///< Last used timer.
    unsigned       memoryUse_;      ///< Memory use in bytes.
    AsyncLoadState asyncLoadState_; ///< Asynchronous loading state.
};
/// Base class for resources that support arbitrary metadata stored. Metadata serialization shall be implemented in derived classes.
class LUTEFISK3D_EXPORT ResourceWithMetadata : public Resource
{
    URHO3D_OBJECT(ResourceWithMetadata, Resource)

public:
    /// Construct.
    ResourceWithMetadata(Context* context);
    ~ResourceWithMetadata() override;
    /// Add new metadata variable or overwrite old value.
    void AddMetadata(const QString& name, const Variant& value);
    void AddMetadata(const QStringRef& name, const Variant& value);
    /// Remove metadata variable.
    void RemoveMetadata(const QString& name);
    /// Remove all metadata variables.
    void RemoveAllMetadata();
    /// Return metadata variable.
    const Variant& GetMetadata(const QString& name) const;
    /// Return whether the resource has metadata.
    bool HasMetadata() const;

protected:
    /// Load metadata from <metadata> children of XML element.
    void LoadMetadataFromXML(const XMLElement& source);
    /// Load metadata from JSON array.
    void LoadMetadataFromJSON(const JSONArray& array);
    /// Save as <metadata> children of XML element.
    void SaveMetadataToXML(XMLElement& destination) const;
    /// Copy metadata from another resource.
    void CopyMetadata(const ResourceWithMetadata& source);

private:
    void *privateData = nullptr;
};
inline QString GetResourceName(Resource* resource)
{
    return resource ? resource->GetName() : QLatin1Literal("");
}

inline StringHash GetResourceType(Resource* resource, StringHash defaultType)
{
    return resource ? resource->GetType() : defaultType;
}

inline ResourceRef GetResourceRef(Resource* resource, StringHash defaultType)
{
    return {GetResourceType(resource, defaultType), GetResourceName(resource)};
}

template <class T> QStringList GetResourceNames(const std::vector<SharedPtr<T> >& resources)
{
    QStringList ret(resources.size());
    for (unsigned i = 0; i < resources.size(); ++i)
        ret[i] = GetResourceName(resources[i]);

    return ret;
}

template <class T> ResourceRefList GetResourceRefList(const std::vector<SharedPtr<T> >& resources)
{
    return ResourceRefList(T::GetTypeStatic(), GetResourceNames(resources));
}

}
