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

#include "Context.h"
#include "Thread.h"
#include "EventProfiler.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Container/HashMap.h"
#ifndef MINI_URHO
#include <SDL2/SDL.h>
#endif
namespace Urho3D
{

// Keeps track of how many times SDL was initialised so we know when to call SDL_Quit().
static int sdlInitCounter = 0;
void EventReceiverGroup::BeginSendEvent()
{
    ++inSend_;
}

void EventReceiverGroup::EndSendEvent()
{
    assert(inSend_ > 0);
    --inSend_;

    if (inSend_ == 0 && dirty_)
    {
        /// \todo Could be optimized by erase-swap, but this keeps the receiver order
        for (unsigned i = receivers_.size() - 1; i < receivers_.size(); --i)
        {
            if (!receivers_[i])
                receivers_.erase(receivers_.begin()+i);
        }

        dirty_ = false;
    }
}

void EventReceiverGroup::Add(Object* object)
{
    if (object)
        receivers_.push_back(object);
}

void EventReceiverGroup::Remove(Object* object)
{
    auto i = std::find(receivers_.begin(),receivers_.end(),object);
    if (inSend_ > 0)
    {
        if (i != receivers_.end())
        {
            (*i) = nullptr;
            dirty_ = true;
        }
    }
    else
    {
        assert(i!=receivers_.end());
        receivers_.erase(i);
    }
}
void RemoveNamedAttribute(HashMap<StringHash, std::vector<AttributeInfo> >& attributes, StringHash objectType, const char* name)
{
    auto i = attributes.find(objectType);
    if (i == attributes.end())
        return;

    std::vector<AttributeInfo>& infos = MAP_VALUE(i);

    for (std::vector<AttributeInfo>::iterator j = infos.begin(); j != infos.end(); ++j)
    {
        if (!j->name_.compare(name))
        {
            infos.erase(j);
            break;
        }
    }

    // If the vector became empty, erase the object type from the map
    if (infos.empty())
        attributes.erase(i);
}

Context::Context() : eventHandler_(nullptr)
{
    // Set the main thread ID (assuming the Context is created in it)
    Thread::SetMainThread();
}

Context::~Context()
{
    // Remove subsystems that use SDL in reverse order of construction, so that Graphics can shut down SDL last
    /// \todo Context should not need to know about subsystems
    RemoveSubsystem("Audio");
    RemoveSubsystem("UI");
    RemoveSubsystem("Input");
    RemoveSubsystem("Renderer");
    RemoveSubsystem("Graphics");

    subsystems_.clear();
    factories_.clear();

    // Delete allocated event data maps
    for (VariantMap* elem : eventDataMaps_)
        delete elem;
    eventDataMaps_.clear();
}

SharedPtr<Object> Context::CreateObject(StringHash objectType)
{
    HashMap<StringHash, SharedPtr<ObjectFactory> >::const_iterator i = factories_.find(objectType);
    if (i != factories_.end())
        return MAP_VALUE(i)->CreateObject();
    else
        return SharedPtr<Object>();
}

void Context::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return;

    factories_[factory->GetType()] = factory;
}

void Context::RegisterFactory(ObjectFactory* factory, const char* category)
{
    if (!factory)
        return;

    RegisterFactory(factory);
    if (category && category[0]!=0)
        objectCategories_[category].push_back(factory->GetType());
}

void Context::RegisterSubsystem(Object* object)
{
    if (!object)
        return;

    subsystems_[object->GetType()] = object;
}

void Context::RemoveSubsystem(StringHash objectType)
{
    HashMap<StringHash, SharedPtr<Object> >::iterator i = subsystems_.find(objectType);
    if (i != subsystems_.end())
        subsystems_.erase(i);
}

void Context::RegisterAttribute(StringHash objectType, const AttributeInfo& attr)
{
    // None or pointer types can not be supported
    if (attr.type_ == VAR_NONE || attr.type_ == VAR_VOIDPTR || attr.type_ == VAR_PTR)
    {
        URHO3D_LOGWARNING("Attempt to register unsupported attribute type " + Variant::GetTypeName(attr.type_) + " to class " +
            GetTypeName(objectType));
        return;
    }
    attributes_[objectType].push_back(attr);

    if (attr.mode_ & AM_NET)
        networkAttributes_[objectType].push_back(attr);
}

void Context::RemoveAttribute(StringHash objectType, const char* name)
{
    RemoveNamedAttribute(attributes_, objectType, name);
    RemoveNamedAttribute(networkAttributes_, objectType, name);
}

void Context::UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue)
{
    AttributeInfo* info = GetAttribute(objectType, name);
    if (info)
        info->defaultValue_ = defaultValue;
}

VariantMap& Context::GetEventDataMap()
{
    unsigned nestingLevel = eventSenders_.size();
    while (eventDataMaps_.size() < nestingLevel + 1)
        eventDataMaps_.push_back(new VariantMap());

    VariantMap& ret = *eventDataMaps_[nestingLevel];
    ret.clear();
    return ret;
}

bool Context::RequireSDL(unsigned int sdlFlags)
{
#ifndef MINI_URHO
    // Always increment, the caller must match with ReleaseSDL(), regardless of
    // what happens.
    ++sdlInitCounter;

    // Need to call SDL_Init() at least once before SDL_InitSubsystem()
    if (sdlInitCounter == 0)
    {
        URHO3D_LOGDEBUG("Initialising SDL");
        if (SDL_Init(0) != 0)
        {
            URHO3D_LOGERROR(QString("Failed to initialise SDL: %1").arg(SDL_GetError()));
            return false;
        }
    }

    Uint32 remainingFlags = sdlFlags & ~SDL_WasInit(0);
    if (remainingFlags != 0)
    {
        if (SDL_InitSubSystem(remainingFlags) != 0)
        {
            URHO3D_LOGERROR(QString("Failed to initialise SDL subsystem: %1").arg(SDL_GetError()));
            return false;
        }
    }
#endif

    return true;
}

void Context::ReleaseSDL()
{
#ifndef MINI_URHO
    --sdlInitCounter;

    if (sdlInitCounter == 0)
    {
        URHO3D_LOGDEBUG("Quitting SDL");
        SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
        SDL_Quit();
    }

    if (sdlInitCounter < 0)
        URHO3D_LOGERROR("Too many calls to Context::ReleaseSDL()!");
#endif
}

void Context::CopyBaseAttributes(StringHash baseType, StringHash derivedType)
{
    // Prevent endless loop if mistakenly copying attributes from same class as derived
    if (baseType == derivedType)
    {
        URHO3D_LOGWARNING("Attempt to copy base attributes to itself for class " + GetTypeName(baseType));
        return;
    }

    const std::vector<AttributeInfo> * baseAttributes(GetAttributes(baseType));
    if(!baseAttributes)
        return;
    std::vector<AttributeInfo> &target(attributes_[derivedType]);
    for (const AttributeInfo& attr : *baseAttributes)
    {
        target.push_back(attr);
        if (attr.mode_ & AM_NET)
            networkAttributes_[derivedType].push_back(attr);
    }
}

Object* Context::GetSubsystem(StringHash type) const
{
    HashMap<StringHash, SharedPtr<Object> >::const_iterator i = subsystems_.find(type);
    if (i != subsystems_.end())
        return MAP_VALUE(i);
    else
        return nullptr;
}

const Variant &Context::GetGlobalVar(StringHash key) const
{
    auto i = globalVars_.find(key);
    return i != globalVars_.end() ? MAP_VALUE(i) : Variant::EMPTY;
}

void Context::SetGlobalVar(StringHash key, const Variant &value)
{
    globalVars_[key] = value;
}

Object* Context::GetEventSender() const
{
    if (!eventSenders_.empty())
        return eventSenders_.back();
    return nullptr;
}

const QString& Context::GetTypeName(StringHash objectType) const
{
    // Search factories to find the hash-to-name mapping
    HashMap<StringHash, SharedPtr<ObjectFactory> >::const_iterator i = factories_.find(objectType);
    return i != factories_.end() ? MAP_VALUE(i)->GetTypeName() : s_dummy;
}

AttributeInfo* Context::GetAttribute(StringHash objectType, const char* name)
{
    auto i = attributes_.find(objectType);
    if (i == attributes_.end())
        return nullptr;

    std::vector<AttributeInfo>& infos = MAP_VALUE(i);

    for (AttributeInfo &j : infos)
    {
        if (!j.name_.compare(name))
            return &j;
    }

    return nullptr;
}

void Context::AddEventReceiver(Object* receiver, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = eventReceivers_[eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::AddEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = specificEventReceivers_[sender][eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::RemoveEventSender(Object* sender)
{
    auto i = specificEventReceivers_.find(sender);
    if (i == specificEventReceivers_.end())
        return;
    for (const std::pair<const StringHash,SharedPtr<EventReceiverGroup> > & elem : MAP_VALUE(i))
    {
        for (Object* receiver : ELEMENT_VALUE(elem)->receivers_)
        {
            if(receiver)
                receiver->RemoveEventSender(sender);
        }
    }
    specificEventReceivers_.erase(i);
}

void Context::RemoveEventReceiver(Object* receiver, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(eventType);
    if (group)
        group->Remove(receiver);
}

void Context::RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(sender, eventType);
    if (group)
        group->Remove(receiver);
}

void Context::BeginSendEvent(Object* sender, StringHash eventType)
{
#ifdef LUTEFISK3D_PROFILING
    if (EventProfiler::IsActive())
    {
        EventProfiler* eventProfiler = GetSubsystem<EventProfiler>();
        if (eventProfiler)
            eventProfiler->BeginBlock(eventType);
    }
#endif

    eventSenders_.push_back(sender);
}

void Context::EndSendEvent()
{
    eventSenders_.pop_back();

#ifdef LUTEFISK3D_PROFILING
    if (EventProfiler::IsActive())
    {
        EventProfiler* eventProfiler = GetSubsystem<EventProfiler>();
        if (eventProfiler)
            eventProfiler->EndBlock();
    }
#endif
}

}
