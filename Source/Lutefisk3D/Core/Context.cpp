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
#include "context_p.h"

#include "Thread.h"
#include "Attribute.h"
#include "EventProfiler.h"

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Core/Variant.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/Input/Input.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/Renderer.h"
#include "Lutefisk3D/Core/WorkQueue.h"
#ifndef LUTEFISK3D_UILESS
#include "Lutefisk3D/UI/UI.h"
#endif
#ifndef MINI_URHO

#include <GLFW/glfw3.h>

#endif
namespace std
{
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::Log>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::FileSystem>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::Input>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::ResourceCache>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::Graphics>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::Renderer>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::Time>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::Profiler>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::EventProfiler>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::WorkQueue>;
template class LUTEFISK3D_EXPORT std::unique_ptr<Urho3D::UI>;
}
namespace Urho3D
{
template class LUTEFISK3D_EXPORT SharedPtr<AttributeAccessor>;
/*!
  \class Context
  \brief Urho3D execution context. Provides access to subsystems, object factories and attributes, and event receivers.
*/
/*!
  \fn SharedPtr<T> Context::CreateObject()
  \brief Create an object by type. Return pointer to it or null if no factory found.
*/
/*!
  \fn void Context::RegisterFactory(const char *category=nullptr)
  \param category
  \brief Template version of registering an object factory with optional category.
*/
/*!
  \fn const VariantMap& Context::GetGlobalVars() const
  \brief Return all global variables.
*/
/*!
  \fn const QString &Context::GetObjectCategory(StringHash objType) const
  \brief Return category name for given Object type
*/
/*!
  \fn void Context::RemoveSubsystem()
  \brief Template version of removing a subsystem.
*/
/*!
  \fn void Context::RegisterAttribute(const AttributeInfo& attr)
  \brief Template version of registering an object attribute.
*/
/*!
  \fn void Context::RemoveAttribute(const char* name)
  \brief Template version of removing an object attribute.
*/
/*!
  \fn void Context::CopyBaseAttributes()
  \brief Template version of copying base class attributes to derived class.
*/
/*!
  \fn void Context::UpdateAttributeDefaultValue(const char* name, const Variant& defaultValue)
  \brief Template version of updating an object attribute's default value.
*/
/*!
  \fn EventHandler* Context::GetEventHandler() const
  \brief Return active event handler. Set by Object. Null outside event handling.
*/
/*!
  \fn T* Context::GetSubsystem() const
  \brief Template version of returning a subsystem.
*/
/*!
  \fn AttributeInfo* Context::GetAttribute(const char* name)
  \brief Template version of returning a specific attribute description.
*/
/*!
  \fn const std::vector<AttributeInfo>* Context::GetAttributes(StringHash type) const
  \brief Return attribute descriptions for an object type, or null if none defined.
*/
/*!
  \fn const std::vector<AttributeInfo>* Context::GetNetworkAttributes(StringHash type) const
  \brief Return network replication attribute descriptions for an object type, or null if none defined.
*/
/*!
  \fn EventReceiverGroup* Context::GetEventReceivers(Object* sender, StringHash eventType)
  \brief Return event receivers for a sender and event type, or null if they do not exist.
*/
/*!
  \fn EventReceiverGroup* Context::GetEventReceivers(StringHash eventType)
  \brief Return event receivers for an event type, or null if they do not exist.
*/
/*!
  \fn void Context::SetEventHandler(EventHandler* handler)
  \brief Set current event handler. Called by Object.
*/
/*!
  \fn void Context::SetEventHandler(EventHandler* handler)
  \brief Set current event handler. Called by Object.
*/

/*!
  \var HashMap<Object*, HashMap<StringHash, SharedPtr<EventReceiverGroup> > > ContextPrivate::specificEventReceivers_
  \brief
 */
/*!
  \var std::vector<Object*> Context::eventSenders_
  \brief Event sender stack.
 */
/*!
  \var EventHandler* Context::eventHandler_
  \brief Active event handler. Not stored in a stack for performance reasons; is needed only in esoteric cases.
 */
/*!
  \class EventReceiverGroup
  \brief Tracking structure for event receivers.
*/

/*!
  \fn void EventReceiverGroup::BeginSendEvent()
  \brief Begin event send. When receivers are removed during send, group has to be cleaned up afterward.
 */
/*!
  \fn void EventReceiverGroup::EndSendEvent()
  \brief End event send. Clean up if necessary.
 */
/*!
  \fn void EventReceiverGroup::Add(Object* object)
  \brief Add receiver. Same receiver must not be double-added!
 */
/*!
  \fn void EventReceiverGroup::Remove(Object* object)
  \brief Remove receiver. Leave holes during send, which requires later cleanup.
 */
/*!
  \var std::vector<Object*> EventReceiverGroup::receivers_
  \brief Receivers. May contain holes during sending.
 */
/*!
  \var unsigned EventReceiverGroup::inSend_
  \brief "In send" recursion counter.
 */
/*!
  \var bool EventReceiverGroup::dirty_
  \brief Cleanup required flag.
 */
class ContextPrivate
{
public:
    ~ContextPrivate()
    {
        factories_.clear();
        subsystems_.clear();
        eventReceivers_.clear();
        specificEventReceivers_.clear();
        attributes_.clear();
        networkAttributes_.clear();
        // Delete allocated event data maps
        for (VariantMap* elem : eventDataMaps_)
            delete elem;
    }
    HashMap<QString, std::vector<StringHash> > objectCategories_;
    HashMap<StringHash, SharedPtr<ObjectFactory> > factories_;
    HashMap<StringHash, SharedPtr<Object> > subsystems_;
    HashMap<StringHash, SharedPtr<EventReceiverGroup> > eventReceivers_; //!<Event receivers for non-specific events.
    //! Event receivers for specific senders' events.
    HashMap<Object*, HashMap<StringHash, SharedPtr<EventReceiverGroup> > > specificEventReceivers_;
    std::vector<VariantMap*> eventDataMaps_; //!<Event data stack.
    HashMap<StringHash, std::vector<AttributeInfo> > attributes_; //!<Attribute descriptions per object type.
    //!Network replication attribute descriptions per object type.v
    HashMap<StringHash, std::vector<AttributeInfo> > networkAttributes_;
};

const QString &Context::GetObjectCategory(StringHash objType) const
{
    for (auto iter = d->objectCategories_.begin(),fin=d->objectCategories_.end(); iter!=fin; ++iter)
    {
        const std::vector<StringHash> &entries(MAP_VALUE(iter));
        if(entries.end() != std::find(entries.begin(),entries.end(),objType))
            return MAP_KEY(iter);
    }
    return s_dummy;
}
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
        for (size_t i = receivers_.size() - 1; i < receivers_.size(); --i)
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

Context::Context() : d(new ContextPrivate)
{
    // Set the main thread ID (assuming the Context is created in it)
    Thread::SetMainThread();
}

Context::~Context()
{
    // Remove subsystems that use SDL in reverse order of construction, so that Graphics can shut down SDL last
    /// \todo Context should not need to know about subsystems
    m_ResourceCache.reset();
    RemoveSubsystem("Audio");
#ifndef LUTEFISK3D_UILESS
    m_UISystem.reset();
#endif
    m_InputSystem.reset();
    m_Renderer.reset();
    m_Graphics.reset();
}

Log * Context::LogSystem() const
{
    return m_LogSystem.get();
}

void Context::SetLogSystem(Log * l)
{
    m_LogSystem.reset(l);
}
/// Create an object by type hash. Return pointer to it or null if no factory found.
SharedPtr<Object> Context::CreateObject(StringHash objectType)
{
    HashMap<StringHash, SharedPtr<ObjectFactory> >::const_iterator i = d->factories_.find(objectType);
    if (i != d->factories_.end())
        return MAP_VALUE(i)->CreateObject();
    else
        return SharedPtr<Object>();
}
/// Register a factory for an object type and specify the object category.
void Context::RegisterFactory(ObjectFactory* factory, const char* category)
{
    if (!factory)
        return;

    d->factories_[factory->GetType()] = factory;

    if (category && category[0]!=0)
        d->objectCategories_[category].push_back(factory->GetType());
}
/// Register a subsystem.
void Context::RegisterSubsystem(StringHash typeHash,Object* object)
{
    if (!object)
        return;

    d->subsystems_[typeHash] = object;
}
/// Remove a subsystem.
void Context::RemoveSubsystem(StringHash objectType)
{
    auto i = d->subsystems_.find(objectType);
    if (i != d->subsystems_.end())
        d->subsystems_.erase(i);
}
/// Register object attribute.
void Context::RegisterAttribute(StringHash objectType, const AttributeInfo& attr)
{
    // None or pointer types can not be supported
    if (attr.type_ == VAR_NONE || attr.type_ == VAR_VOIDPTR || attr.type_ == VAR_PTR)
    {
        URHO3D_LOGWARNING("Attempt to register unsupported attribute type " + Variant::GetTypeName(attr.type_) + " to class " +
                          GetTypeName(objectType));
        return;
    }
    d->attributes_[objectType].push_back(attr);

    if (attr.mode_ & AM_NET)
        d->networkAttributes_[objectType].push_back(attr);
}
/// Remove object attribute.
void Context::RemoveAttribute(StringHash objectType, const char* name)
{
    RemoveNamedAttribute(d->attributes_, objectType, name);
    RemoveNamedAttribute(d->networkAttributes_, objectType, name);
}

/// Update object attribute's default value.
void Context::UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue)
{
    AttributeInfo* info = GetAttribute(objectType, name);
    if (info)
        info->defaultValue_ = defaultValue;
}

///
/// \brief Used for optimization to avoid constant re-allocation of event data maps.
/// \return preallocated map for event data
HashMap<StringHash, Variant> & Context::GetEventDataMap()
{
    size_t nestingLevel = eventSenders_.size();
    while (d->eventDataMaps_.size() < nestingLevel + 1)
        d->eventDataMaps_.push_back(new VariantMap());

    HashMap<StringHash, Variant>& ret = *d->eventDataMaps_[nestingLevel];
    ret.clear();
    return ret;
}

/// Copy base class attributes to derived class.
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
    std::vector<AttributeInfo> &target(d->attributes_[derivedType]);
    for (const AttributeInfo& attr : *baseAttributes)
    {
        target.push_back(attr);
        if (attr.mode_ & AM_NET)
            d->networkAttributes_[derivedType].push_back(attr);
    }
}
/// Return subsystem by type.
Object* Context::GetSubsystem(StringHash type) const
{
    HashMap<StringHash, SharedPtr<Object> >::const_iterator i = d->subsystems_.find(type);
    if (i != d->subsystems_.end())
        return MAP_VALUE(i);
    else
        return nullptr;
}

/// Return active event sender. Null outside event handling.
Object* Context::GetEventSender() const
{
    if (!eventSenders_.empty())
        return eventSenders_.back();
    return nullptr;
}
/// Return object type name from hash, or empty if unknown.
const QString& Context::GetTypeName(StringHash objectType) const
{
    // Search factories to find the hash-to-name mapping
    HashMap<StringHash, SharedPtr<ObjectFactory> >::const_iterator i = d->factories_.find(objectType);
    return i != d->factories_.end() ? MAP_VALUE(i)->GetTypeName() : s_dummy;
}
/// Return a specific attribute description for an object, or null if not found.
AttributeInfo* Context::GetAttribute(StringHash objectType, const char* name)
{
    auto i = d->attributes_.find(objectType);
    if (i == d->attributes_.end())
        return nullptr;

    std::vector<AttributeInfo>& infos = MAP_VALUE(i);

    for (AttributeInfo &j : infos)
    {
        if (!j.name_.compare(name))
            return &j;
    }
    return nullptr;
}

const std::vector<AttributeInfo> *Context::GetAttributes(StringHash type) const
{
    HashMap<StringHash, std::vector<AttributeInfo> >::const_iterator i = d->attributes_.find(type);
    return i != d->attributes_.end() ? &(MAP_VALUE(i)) : nullptr;
}

const std::vector<AttributeInfo> *Context::GetNetworkAttributes(StringHash type) const
{
    HashMap<StringHash, std::vector<AttributeInfo> >::const_iterator i = d->networkAttributes_.find(type);
    return i != d->networkAttributes_.end() ? &(MAP_VALUE(i)) : nullptr;
}

EventReceiverGroup *Context::GetEventReceivers(Object * sender, StringHash eventType)
{
    auto i = d->specificEventReceivers_.find(sender);
    if (i != d->specificEventReceivers_.end())
    {
        auto j = MAP_VALUE(i).find(eventType);
        return j != MAP_VALUE(i).end() ? MAP_VALUE(j) : nullptr;
    }
    return nullptr;
}

EventReceiverGroup *Context::GetEventReceivers(StringHash eventType)
{
    auto i = d->eventReceivers_.find(eventType);
    return i != d->eventReceivers_.end() ? MAP_VALUE(i) : nullptr;
}
/// Add event receiver.
void Context::AddEventReceiver(Object* receiver, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = d->eventReceivers_[eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}
/// Add event receiver for specific event.
void Context::AddEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = d->specificEventReceivers_[sender][eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}
/// Remove an event sender from all receivers. Called on its destruction.
void Context::RemoveEventSender(Object* sender)
{
    auto i = d->specificEventReceivers_.find(sender);
    if (i == d->specificEventReceivers_.end())
        return;
    for (const std::pair<const StringHash,SharedPtr<EventReceiverGroup> > & elem : MAP_VALUE(i))
    {
        for (Object* receiver : ELEMENT_VALUE(elem)->receivers_)
        {
            if(receiver)
                receiver->RemoveEventSender(sender);
        }
    }
    d->specificEventReceivers_.erase(i);
}
/// Remove event receiver from non-specific events.
void Context::RemoveEventReceiver(Object* receiver, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(eventType);
    if (group)
        group->Remove(receiver);
}
/// Remove event receiver from specific events.
void Context::RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(sender, eventType);
    if (group)
        group->Remove(receiver);
}
/// Begin event send.
void Context::BeginSendEvent(Object* sender, StringHash eventType)
{
#ifdef LUTEFISK3D_PROFILING
    if (EventProfiler::IsActive())
    {
        if (m_EventProfilerSystem)
            m_EventProfilerSystem->BeginBlock(eventType);
    }
#endif

    eventSenders_.push_back(sender);
}
/// End event send. Clean up event receivers removed in the meanwhile.
void Context::EndSendEvent()
{
    eventSenders_.pop_back();

#ifdef LUTEFISK3D_PROFILING
    if (EventProfiler::IsActive())
    {
        if (m_EventProfilerSystem)
            m_EventProfilerSystem->EndBlock();
    }
#endif
}

}
