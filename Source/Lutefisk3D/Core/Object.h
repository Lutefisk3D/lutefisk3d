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

#include "../Container/RefCounted.h"
#include "../Container/HashMap.h"
#include "../Container/Ptr.h"
#include "../Container/Str.h"
#include "../Core/Variant.h"
#include "../Math/StringHash.h"

#include <QtCore/QString>
#include <cassert>
#include <deque>

namespace Urho3D
{
typedef HashMap<StringHash, Variant> VariantMap;

class Context;
class EventHandler;
typedef std::deque<EventHandler *>::iterator ilEventHandler;
typedef std::deque<EventHandler *>::const_iterator cilEventHandler;

/// Type info.
class TypeInfo
{
public:
    /// Construct.
    TypeInfo(const char* typeName, const TypeInfo* baseTypeInfo);
    /// Destruct.
    ~TypeInfo();

    /// Check current type is type of specified type.
    bool IsTypeOf(StringHash type) const;
    /// Check current type is type of specified type.
    bool IsTypeOf(const TypeInfo* typeInfo) const;
    /// Check current type is type of specified class type.
    template<typename T> bool IsTypeOf() const { return IsTypeOf(T::GetTypeInfoStatic()); }

    /// Return type.
    StringHash GetType() const { return type_; }
    /// Return type name.
    const QString& GetTypeName() const { return typeName_;}
    /// Return base type info.
    const TypeInfo* GetBaseTypeInfo() const { return baseTypeInfo_; }

private:
    /// Type.
    StringHash type_;
    /// Type name.
    QString typeName_;
    /// Base class type info.
    const TypeInfo* baseTypeInfo_;
};
#define URHO3D_OBJECT(typeName, baseTypeName) \
    public: \
        typedef typeName ClassName; \
        typedef baseTypeName BaseClassName; \
        virtual Urho3D::StringHash GetType() const override { return GetTypeInfoStatic()->GetType(); } \
        virtual const QString& GetTypeName() const override { return GetTypeInfoStatic()->GetTypeName(); } \
        virtual const Urho3D::TypeInfo* GetTypeInfo() const override { return GetTypeInfoStatic(); } \
        static Urho3D::StringHash GetTypeStatic() { return GetTypeInfoStatic()->GetType(); } \
        static const QString& GetTypeNameStatic() { return GetTypeInfoStatic()->GetTypeName(); } \
        static const Urho3D::TypeInfo* GetTypeInfoStatic() { static const Urho3D::TypeInfo typeInfoStatic(#typeName, BaseClassName::GetTypeInfoStatic()); return &typeInfoStatic; } \


/// Base class for objects with type identification, subsystem access and event sending/receiving capability.
class Object : public RefCounted
{
    friend class Context;

public:
    /// Construct.
    Object(Context* context);
    /// Destruct. Clean up self from event sender & receiver structures.
    virtual ~Object();

    /// Return type hash.
    virtual StringHash GetType() const = 0;
    /// Return type name.
    virtual const QString& GetTypeName() const = 0;
    /// Return type info.
    virtual const TypeInfo* GetTypeInfo() const = 0;
    /// Handle event.
    virtual void OnEvent(Object* sender, StringHash eventType, VariantMap& eventData);

    /// Return type info static.
    static const TypeInfo* GetTypeInfoStatic() { return 0; }
    /// Check current type is type of specified type.
    static bool IsTypeOf(StringHash type);
    /// Check current type is type of specified type.
    static bool IsTypeOf(const TypeInfo* typeInfo);
    /// Check current type is type of specified class.
    template<typename T> static bool IsTypeOf() { return IsTypeOf(T::GetTypeInfoStatic()); }
    /// Check current instance is type of specified type.
    bool IsInstanceOf(StringHash type) const;
    /// Check current instance is type of specified type.
    bool IsInstanceOf(const TypeInfo* typeInfo) const;
    /// Check current instance is type of specified class.
    template<typename T> bool IsInstanceOf() const { return IsInstanceOf(T::GetTypeInfoStatic()); }
    /// Subscribe to an event that can be sent by any sender.
    void SubscribeToEvent(StringHash eventType, EventHandler* handler);
    /// Subscribe to a specific sender's event.
    void SubscribeToEvent(Object* sender, StringHash eventType, EventHandler* handler);
    /// Unsubscribe from an event.
    void UnsubscribeFromEvent(StringHash eventType);
    /// Unsubscribe from a specific sender's event.
    void UnsubscribeFromEvent(Object* sender, StringHash eventType);
    /// Unsubscribe from a specific sender's events.
    void UnsubscribeFromEvents(Object* sender);
    /// Unsubscribe from all events.
    void UnsubscribeFromAllEvents();
    /// Unsubscribe from all events except those listed, and optionally only those with userdata (script registered events.)
    void UnsubscribeFromAllEventsExcept(const SmallMembershipSet<StringHash,4> & exceptions, bool onlyUserData);
    /// Send event to all subscribers.
    void SendEvent(StringHash eventType);
    /// Send event with parameters to all subscribers.
    void SendEvent(StringHash eventType, VariantMap& eventData);
    /// Return a preallocated map for event data. Used for optimization to avoid constant re-allocation of event data maps.
    VariantMap& GetEventDataMap() const;

    /// Return execution context.
    Context* GetContext() const { return context_; }
    /// Return subsystem by type.
    Object* GetSubsystem(StringHash type) const;
    /// Return active event sender. Null outside event handling.
    Object* GetEventSender() const;
    /// Return active event handler. Null outside event handling.
    EventHandler* GetEventHandler() const;
    /// Return whether has subscribed to an event without specific sender.
    bool HasSubscribedToEvent(StringHash eventType) const;
    /// Return whether has subscribed to a specific sender's event.
    bool HasSubscribedToEvent(Object* sender, StringHash eventType) const;
    /// Return whether has subscribed to any event.
    bool HasEventHandlers() const { return !eventHandlers_.empty(); }
    /// Template version of returning a subsystem.
    template <class T> T* GetSubsystem() const;
    /// Return object category. Categories are (optionally) registered along with the object factory. Return an empty string if the object category is not registered.
    const QString& GetCategory() const;

protected:
    /// Execution context.
    Context* context_;

private:
    /// Find the first event handler with no specific sender.
    cilEventHandler FindEventHandler(StringHash eventType) const;
    /// Find the first event handler with specific sender.
    cilEventHandler FindSpecificEventHandler(Object* sender) const;
    /// Find the first event handler with specific sender and event type.
    cilEventHandler FindSpecificEventHandler(Object* sender, StringHash eventType, EventHandler** previous = 0) const;
    /// Remove event handlers related to a specific sender.
    void RemoveEventSender(Object* sender);

    /// Event handlers. Sender is null for non-specific handlers.
    std::deque<EventHandler *> eventHandlers_;
};

template <class T> T* Object::GetSubsystem() const { return static_cast<T*>(GetSubsystem(T::GetTypeStatic())); }

/// Base class for object factories.
class ObjectFactory : public RefCounted
{
public:
    /// Construct.
    ObjectFactory(Context* context) :
        context_(context)
    {
        assert(context_);
    }

    /// Create an object. Implemented in templated subclasses.
    virtual SharedPtr<Object> CreateObject() = 0;

    /// Return execution context.
    Context* GetContext() const { return context_; }
    /// Return type info of objects created by this factory.
    const TypeInfo* GetTypeInfo() const { return typeInfo_; }
    /// Return type hash of objects created by this factory.
    StringHash GetType() const { return typeInfo_->GetType(); }
    /// Return type name of objects created by this factory.
    const QString& GetTypeName() const { return typeInfo_->GetTypeName(); }

protected:
    /// Execution context.
    Context* context_;
    /// Type info.
    const TypeInfo* typeInfo_;
};

/// Template implementation of the object factory.
template <class T> class ObjectFactoryImpl : public ObjectFactory
{
public:
    /// Construct.
    ObjectFactoryImpl(Context* context) :
        ObjectFactory(context)
    {
        typeInfo_ = T::GetTypeInfoStatic();
    }

    /// Create an object of the specific type.
    virtual SharedPtr<Object> CreateObject() override { return SharedPtr<Object>(new T(context_)); }
};

/// Internal helper class for invoking event handler functions.
class EventHandler
{
public:
    /// Construct with specified receiver.
    EventHandler(Object* receiver) :
        receiver_(receiver),
        sender_(nullptr),
        userData_(nullptr)
    {
        assert(receiver_);
    }

    /// Construct with specified receiver and userdata.
    EventHandler(Object* receiver, void* userData) :
        receiver_(receiver),
        sender_(nullptr),
        userData_(userData)
    {
        assert(receiver_);
    }

    /// Destruct.
    virtual ~EventHandler() {}

    /// Set sender and event type.
    void SetSenderAndEventType(Object* sender, StringHash eventType)
    {
        sender_ = sender;
        eventType_ = eventType;
    }

    /// Invoke event handler function.
    virtual void Invoke(VariantMap& eventData) = 0;
    /// Return a unique copy of the event handler.
    virtual EventHandler* Clone() const = 0;

    /// Return event receiver.
    Object* GetReceiver() const { return receiver_; }
    /// Return event sender. Null if the handler is non-specific.
    Object* GetSender() const { return sender_; }
    /// Return event type.
    const StringHash& GetEventType() const { return eventType_; }
    /// Return userdata.
    void* GetUserData() const { return userData_; }

protected:
    /// Event receiver.
    Object* receiver_;
    /// Event sender.
    Object* sender_;
    /// Event type.
    StringHash eventType_;
    /// Userdata.
    void* userData_;
};

/// Template implementation of the event handler invoke helper (stores a function pointer of specific class.)
template <class T> class EventHandlerImpl : public EventHandler
{
public:
    typedef void (T::*HandlerFunctionPtr)(StringHash, VariantMap&);

    /// Construct with receiver and function pointers.
    EventHandlerImpl(T* receiver, HandlerFunctionPtr function) :
        EventHandler(receiver),
        function_(function)
    {
        assert(function_);
    }

    /// Construct with receiver and function pointers and userdata.
    EventHandlerImpl(T* receiver, HandlerFunctionPtr function, void* userData) :
        EventHandler(receiver, userData),
        function_(function)
    {
        assert(function_);
    }

    /// Invoke event handler function.
    virtual void Invoke(VariantMap& eventData) override
    {
        T* receiver = static_cast<T*>(receiver_);
        (receiver->*function_)(eventType_, eventData);
    }

    /// Return a unique copy of the event handler.
    virtual EventHandler* Clone() const override
    {
        return new EventHandlerImpl(static_cast<T*>(receiver_), function_, userData_);
    }

private:
    /// Class-specific pointer to handler function.
    HandlerFunctionPtr function_;
};

/// Describe an event's hash ID and begin a namespace in which to define its parameters.
#define URHO3D_EVENT(eventID, eventName) static const Urho3D::StringHash eventID(#eventName); namespace eventName
/// Describe an event's parameter hash ID. Should be used inside an event namespace.
#define URHO3D_PARAM(paramID, paramName) static const Urho3D::StringHash paramID(#paramName)
/// Convenience macro to construct an EventHandler that points to a receiver object and its member function.
#define URHO3D_HANDLER(className, function) (new Urho3D::EventHandlerImpl<className>(this, &className::function))
/// Convenience macro to construct an EventHandler that points to a receiver object and its member function, and also defines a userdata pointer.
#define URHO3D_HANDLER_USERDATA(className, function, userData) (new Urho3D::EventHandlerImpl<className>(this, &className::function, userData))

}
