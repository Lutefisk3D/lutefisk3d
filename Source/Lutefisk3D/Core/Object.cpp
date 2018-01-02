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
#include "Object.h"

#include "Context.h"
#include "context_p.h"
#include "Variant.h"
#include "Lutefisk3D/IO/Log.h"
#include "Thread.h"
#include <QSet>
#include <algorithm>

using namespace Urho3D;
namespace Urho3D
{
template class LUTEFISK3D_EXPORT SharedPtr<Object>;
}
namespace
{
/// Template implementation of the event handler invoke helper (std::function instance).
class EventHandler11Impl final : public EventHandler
{
public:
    /// Construct with receiver and function pointers and userdata.
    EventHandler11Impl(std::function<void(StringHash, VariantMap&)> function, void* userData = 0) :
        EventHandler(0, userData),
        function_(function)
    {
        assert(function_);
    }

    /// Invoke event handler function.
    void Invoke(VariantMap& eventData)  override
    {
        function_(eventType_, eventData);
    }

private:
    /// Class-specific pointer to handler function.
    std::function<void(StringHash, VariantMap&)> function_;
};

// RAII based Begin/End SendEvent guard
class EventReceiverGroup_Guard
{
    EventReceiverGroup &m_guarded;
public:
    EventReceiverGroup_Guard(EventReceiverGroup &guarded) : m_guarded(guarded) { m_guarded.BeginSendEvent(); }
    ~EventReceiverGroup_Guard() { m_guarded.EndSendEvent(); }
};
}

TypeInfo::TypeInfo(const char* typeName, const TypeInfo* baseTypeInfo) :
    type_(typeName),
    typeName_(typeName),
    baseTypeInfo_(baseTypeInfo)
{
}

bool TypeInfo::IsTypeOf(StringHash type) const
{
    const TypeInfo* current = this;
    while (current)
    {
        if (current->GetType() == type)
            return true;

        current = current->GetBaseTypeInfo();
    }

    return false;
}

bool TypeInfo::IsTypeOf(const TypeInfo* typeInfo) const
{
    const TypeInfo* current = this;
    while (current)
    {
        if (current == typeInfo)
            return true;

        current = current->GetBaseTypeInfo();
    }

    return false;
}
Object::Object(Context* context) :
    context_(context)
{
    assert(context_);
}

Object::~Object()
{
    UnsubscribeFromAllEvents();
    context_->RemoveEventSender(this);
}

void Object::OnEvent(Object* sender, StringHash eventType, VariantMap& eventData)
{
    // Make a copy of the context pointer in case the object is destroyed during event handler invocation
    Context* context = context_;
    EventHandler *specific = nullptr;
    EventHandler *nonSpecific = nullptr;

    for(EventHandler * handler : eventHandlers_)
    {
        if (handler->GetEventType() == eventType)
        {
            if (!handler->GetSender())
                nonSpecific = handler;
            else if (handler->GetSender() == sender)
            {
                specific = handler;
                break;
            }
        }
    }

    // Specific event handlers have priority, so if found, invoke first
    if (specific)
    {
        context->SetEventHandler(specific);
        specific->Invoke(eventData);
        context->SetEventHandler(nullptr);
        return;
    }

    if (nonSpecific)
    {
        context->SetEventHandler(nonSpecific);
        nonSpecific->Invoke(eventData);
        context->SetEventHandler(nullptr);
    }
}

bool Object::IsTypeOf(StringHash type)
{
    return GetTypeInfoStatic()->IsTypeOf(type);
}

bool Object::IsTypeOf(const TypeInfo* typeInfo)
{
    return GetTypeInfoStatic()->IsTypeOf(typeInfo);
}

bool Object::IsInstanceOf(StringHash type) const
{
    return GetTypeInfo()->IsTypeOf(type);
}

bool Object::IsInstanceOf(const TypeInfo* typeInfo) const
{
    return GetTypeInfo()->IsTypeOf(typeInfo);
}
void Object::SubscribeToEvent(StringHash eventType, EventHandler* handler)
{
    if (!handler)
        return;

    handler->SetSenderAndEventType(nullptr, eventType);
    // Remove old event handler first
    cilEventHandler oldHandler = FindSpecificEventHandler(nullptr, eventType);
    if (oldHandler!=eventHandlers_.end()) {
        delete *oldHandler;
        eventHandlers_.erase(oldHandler);
    }

    eventHandlers_.push_front(handler);

    context_->AddEventReceiver(this, eventType);
}

void Object::SubscribeToEvent(Object* sender, StringHash eventType, EventHandler* handler)
{
    // If a null sender was specified, the event can not be subscribed to. Delete the handler in that case
    if (!sender || !handler)
    {
        delete handler;
        return;
    }

    handler->SetSenderAndEventType(sender, eventType);
    // Remove old event handler first
    cilEventHandler oldHandler = FindSpecificEventHandler(sender, eventType);
    if (oldHandler!=eventHandlers_.end()) {
        delete *oldHandler;
        eventHandlers_.erase(oldHandler);
    }

    eventHandlers_.push_front(handler);

    context_->AddEventReceiver(this, sender, eventType);
}
void Object::SubscribeToEvent(StringHash eventType, const std::function<void(StringHash, VariantMap&)>& function, void* userData/*=0*/)
{
    SubscribeToEvent(eventType, new EventHandler11Impl(function, userData));
}

void Object::SubscribeToEvent(Object* sender, StringHash eventType, const std::function<void(StringHash, VariantMap&)>& function, void* userData/*=0*/)
{
    SubscribeToEvent(sender, eventType, new EventHandler11Impl(function, userData));
}

void Object::UnsubscribeFromEvent(StringHash eventType)
{
    for (;;)
    {
        cilEventHandler handler = FindEventHandler(eventType);
        if (handler!=eventHandlers_.end())
        {
            EventHandler *hndl = *handler;
            if (hndl->GetSender())
                context_->RemoveEventReceiver(this, hndl->GetSender(), eventType);
            else
                context_->RemoveEventReceiver(this, eventType);
            delete hndl;
            eventHandlers_.erase(handler);
        }
        else
            break;
    }
}

void Object::UnsubscribeFromEvent(Object* sender, StringHash eventType)
{
    if (!sender)
        return;

    cilEventHandler handler = FindSpecificEventHandler(sender, eventType);
    if (handler!=eventHandlers_.end())
    {
        context_->RemoveEventReceiver(this, (*handler)->GetSender(), eventType);
        delete *handler;
        eventHandlers_.erase(handler);
    }
}

void Object::UnsubscribeFromEvents(Object* sender)
{
    if (!sender)
        return;

    for (;;)
    {
        cilEventHandler handler = FindSpecificEventHandler(sender);
        if(handler==eventHandlers_.end())
            break;
        context_->RemoveEventReceiver(this, (*handler)->GetSender(), (*handler)->GetEventType());
        delete *handler;
        eventHandlers_.erase(handler);
    }
}

void Object::UnsubscribeFromAllEvents()
{
    while(not eventHandlers_.empty())
    {
        EventHandler *handler = eventHandlers_.front();
        if (handler->GetSender())
            context_->RemoveEventReceiver(this, handler->GetSender(), handler->GetEventType());
        else
            context_->RemoveEventReceiver(this, handler->GetEventType());
        delete handler;
        eventHandlers_.erase(eventHandlers_.begin());
    }
}

void Object::UnsubscribeFromAllEventsExcept(const SmallMembershipSet<Urho3D::StringHash, 4> & exceptions, bool onlyUserData)
{
    ilEventHandler handler = eventHandlers_.begin();

    while (handler!=eventHandlers_.end())
    {

        if ((!onlyUserData || (*handler)->GetUserData()) && !exceptions.contains((*handler)->GetEventType()))
        {
            if ((*handler)->GetSender())
                context_->RemoveEventReceiver(this, (*handler)->GetSender(), (*handler)->GetEventType());
            else
                context_->RemoveEventReceiver(this, (*handler)->GetEventType());
            delete *handler;
            handler = eventHandlers_.erase(handler);
        }
        else
            ++handler;
    }
}

void Object::SendEvent(StringHash eventType)
{
    VariantMap noEventData;

    SendEvent(eventType, noEventData);
}

void Object::SendEvent(StringHash eventType, VariantMap& eventData)
{
    if (!Thread::IsMainThread())
    {
        URHO3D_LOGERROR("Sending events is only supported from the main thread");
        return;
    }

    // Make a weak pointer to self to check for destruction during event handling
    WeakPtr<Object> self(this);
    Context* context = context_;
    QSet<Object*> processed;
    Context_EventGuard context_guard(*context,this,eventType);


    // Check first the specific event receivers
    // Note: group is held alive with a shared ptr, as it may get destroyed along with the sender
    SharedPtr<EventReceiverGroup> group(context->GetEventReceivers(this, eventType));
    if (group)
    {
        EventReceiverGroup_Guard event_guard(*group);
        //Prevent sending events for subscribers added during event handling.
        const size_t receiver_count = group->receivers_.size();
        for (size_t i = 0; i < receiver_count; ++i)
        {
            Object* receiver = group->receivers_[i];
            // Holes may exist if receivers removed during send
            if (!receiver)
                continue;

            receiver->OnEvent(this, eventType, eventData);

            // If self has been destroyed as a result of event handling, exit
            if (self.Expired())
            {
                return;
            }

            processed.insert(receiver);
        }
    }

    // Then the non-specific receivers
    group = context->GetEventReceivers(eventType);
    if (!group)
        return;

    EventReceiverGroup_Guard event_guard(*group);
    if (processed.isEmpty())
    {
        //Prevent sending events for subscribers added during event handling.
        const size_t receiver_count = group->receivers_.size();
        for (size_t i = 0; i < receiver_count; ++i)
        {
            Object* receiver = group->receivers_[i];
            if (!receiver)
                continue;

            receiver->OnEvent(this, eventType, eventData);

            if (self.Expired())
                return;
        }
    }
    else
    {
        //Prevent sending events for subscribers added during event handling.
        const size_t receiver_count = group->receivers_.size();
        // If there were specific receivers, check that the event is not sent doubly to them
        for (size_t i = 0; i < receiver_count; ++i)
        {
            Object* receiver = group->receivers_[i];
            if (!receiver || processed.contains(receiver))
                continue;

            receiver->OnEvent(this, eventType, eventData);

            if (self.Expired())
                return;
        }
    }
}

VariantMap& Object::GetEventDataMap() const
{
    return context_->GetEventDataMap();
}

Object* Object::GetSubsystem(StringHash type) const
{
    return context_->GetSubsystem(type);
}

Object* Object::GetEventSender() const
{
    return context_->GetEventSender();
}

EventHandler* Object::GetEventHandler() const
{
    return context_->GetEventHandler();
}

bool Object::HasSubscribedToEvent(StringHash eventType) const
{
    return FindEventHandler(eventType) != eventHandlers_.end();
}

bool Object::HasSubscribedToEvent(Object* sender, StringHash eventType) const
{
    if (!sender)
        return false;
    else
        return FindSpecificEventHandler(sender, eventType) != eventHandlers_.end();
}

const QString& Object::GetCategory() const
{
    return context_->GetObjectCategory(GetType());
}
/// Find the first event handler with no specific sender.
Object::cilEventHandler Object::FindEventHandler(StringHash eventType) const
{
    cilEventHandler handler = eventHandlers_.cbegin();
    while (handler!=eventHandlers_.cend())
    {
        if ((*handler)->GetEventType() == eventType)
            return handler;
        ++handler;
    }

    return eventHandlers_.cend();
}
/// Find the first event handler with specific sender.
Object::cilEventHandler Object::FindSpecificEventHandler(Object* sender) const
{
    cilEventHandler handler = eventHandlers_.cbegin();

    while (handler!=eventHandlers_.cend())
    {
        if ((*handler)->GetSender() == sender)
            return handler;
        ++handler;
    }

    return eventHandlers_.cend();
}
/// Find the first event handler with specific sender and event type.
Object::cilEventHandler Object::FindSpecificEventHandler(Object* sender, StringHash eventType, EventHandler** previous) const
{
    cilEventHandler handler = eventHandlers_.cbegin();
    if(previous)
        *previous = nullptr;

    while (handler!=eventHandlers_.cend())
    {
        if ((*handler)->GetSender() == sender && (*handler)->GetEventType() == eventType)
            return handler;
        if (previous)
            *previous = (EventHandler *)&(*handler);
        ++handler;
    }

    return eventHandlers_.end();
}
/// Remove event handlers related to a specific sender.
void Object::RemoveEventSender(Object* sender)
{
    ilEventHandler handler = eventHandlers_.begin();

    while (handler!=eventHandlers_.end())
    {
        if ((*handler)->GetSender() == sender)
        {
            delete *handler;
            handler = eventHandlers_.erase(handler);
        }
        else
        {
            ++handler;
        }
    }
}
