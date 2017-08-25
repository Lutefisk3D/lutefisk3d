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

#include "Resource.h"

#include "XMLElement.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/Thread.h"

namespace Urho3D
{

Resource::Resource(Context* context) :
    Object(context),
    memoryUse_(0),
    asyncLoadState_(ASYNC_DONE)
{
}
/// Load resource synchronously. Call both BeginLoad() & EndLoad() and return true if both succeeded.
bool Resource::Load(Deserializer& source)
{
    // Because BeginLoad() / EndLoad() can be called from worker threads, where profiling would be a no-op,
    // create a type name -based profile block here
#ifdef LUTEFISK3D_PROFILING
    QString profileBlockName("Load" + GetTypeName());

    Profiler* profiler = context_->m_ProfilerSystem.get();
    if (profiler)
        profiler->BeginBlock(qPrintable(profileBlockName));
#endif

    // If we are loading synchronously in a non-main thread, behave as if async loading (for example use
    // GetTempResource() instead of GetResource() to load resource dependencies)
    SetAsyncLoadState(Thread::IsMainThread() ? ASYNC_DONE : ASYNC_LOADING);

    bool success = BeginLoad(source);
    if (success)
        success &= EndLoad();
    SetAsyncLoadState(ASYNC_DONE);

#ifdef LUTEFISK3D_PROFILING
    if (profiler)
        profiler->EndBlock();
#endif

    return success;
}
/// Finish resource loading. Always called from the main thread. Return true if successful.
bool Resource::EndLoad()
{
    // If no GPU upload step is necessary, no override is necessary
    return true;
}
/// Save resource. Return true if successful.
bool Resource::Save(Serializer& dest) const
{
    URHO3D_LOGERROR("Save not supported" + GetTypeName());
    return false;
}
/// Set name.
void Resource::SetName(const QString& name)
{
    name_ = name;
    nameHash_ = name;
}
/// Set memory use in bytes, possibly approximate.
void Resource::SetMemoryUse(unsigned size)
{
    memoryUse_ = size;
}
/// Reset last used timer.
void Resource::ResetUseTimer()
{
    useTimer_.Reset();
}
/// Set the asynchronous loading state. Called by ResourceCache. Resources in the middle of asynchronous loading are not normally returned to user.
void Resource::SetAsyncLoadState(AsyncLoadState newState)
{
    asyncLoadState_ = newState;
}

unsigned Resource::GetUseTimer()
{
    // If more references than the resource cache, return always 0 & reset the timer
    if (Refs() > 1)
    {
        useTimer_.Reset();
        return 0;
    }
    else
        return useTimer_.GetMSec(false);
}
void ResourceWithMetadata::AddMetadata(const QString& name, const Variant& value)
{
    auto insertStatus = metadata_.insert({StringHash(name), value});
    if (insertStatus.second)
        metadataKeys_.push_back(name);
}

void ResourceWithMetadata::RemoveMetadata(const QString& name)
{
    metadata_.erase(name);
    metadataKeys_.removeAll(name);
}

void ResourceWithMetadata::RemoveAllMetadata()
{
    metadata_.clear();
    metadataKeys_.clear();
}

const Urho3D::Variant& ResourceWithMetadata::GetMetadata(const QString& name) const
{
    auto value_iter = metadata_.find(name);
    return value_iter!=metadata_.end() ? MAP_VALUE(value_iter) : Variant::EMPTY;
}

bool ResourceWithMetadata::HasMetadata() const
{
    return !metadata_.isEmpty();
}

void ResourceWithMetadata::LoadMetadataFromXML(const XMLElement& source)
{
    for (XMLElement elem = source.GetChild("metadata"); elem; elem = elem.GetNext("metadata"))
        AddMetadata(elem.GetAttribute("name"), elem.GetVariant());
}

void ResourceWithMetadata::LoadMetadataFromJSON(const JSONArray& array)
{
    for (unsigned i = 0; i < array.size(); i++)
    {
        const JSONValue& value = array.at(i);
        AddMetadata(value.Get("name").GetString(), value.GetVariant());
    }
}

void ResourceWithMetadata::SaveMetadataToXML(XMLElement& destination) const
{
    for (unsigned i = 0; i < metadataKeys_.size(); ++i)
    {
        XMLElement elem = destination.CreateChild("metadata");
        elem.SetString("name", metadataKeys_[i]);
        elem.SetVariant(GetMetadata(metadataKeys_[i]));
    }
}

void ResourceWithMetadata::CopyMetadata(const ResourceWithMetadata& source)
{
    metadata_ = source.metadata_;
    metadataKeys_ = source.metadataKeys_;
}

}
