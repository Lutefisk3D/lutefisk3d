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

#pragma once
#include "Lutefisk3D/Container/Allocator.h"
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"

#include <QtCore/QString>
#include <cassert>
#include <deque>
#include <functional>
namespace Urho3D
{
class Variant;
using VariantMap = HashMap<StringHash, Variant>;

class Context;
class EventHandler;
class Object;

/// Type info.
class LUTEFISK3D_EXPORT TypeInfo
{
public:
    /// Construct.
    TypeInfo(const char* typeName, const TypeInfo* baseTypeInfo);

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
class LUTEFISK3D_EXPORT Object : public jl::SignalObserver,public RefCounted
{
    friend class Context;

public:
    using ilEventHandler = PODVectorN<EventHandler *,2>::iterator;
    using cilEventHandler = PODVectorN<EventHandler *,2>::const_iterator;

    /// Construct.
    explicit Object(Context* context);
    /// Destruct. Clean up self from event sender & receiver structures.
     ~Object() override;

    virtual StringHash GetType() const = 0; //!< Return type hash.
    virtual const QString& GetTypeName() const = 0; //!< Return type name.
    virtual const TypeInfo* GetTypeInfo() const = 0; //!< Return type info.
    virtual void OnEvent(Object* sender, StringHash eventType, VariantMap& eventData); //!< Handle event.
    /// Return type info static.
    static const TypeInfo* GetTypeInfoStatic() { return nullptr; }
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
    /// Cast the object to specified most derived class.
    template<typename T> T* Cast() { return IsInstanceOf<T>() ? static_cast<T*>(this) : nullptr; }
    /// Cast the object to specified most derived class.
    template<typename T> const T* Cast() const { return IsInstanceOf<T>() ? static_cast<const T*>(this) : nullptr; }
    /// Subscribe to an event that can be sent by any sender.
    void SubscribeToEvent(StringHash eventType, EventHandler* handler);
    /// Subscribe to an event that can be sent by any sender.
    void SubscribeToEvent(StringHash eventType, const std::function<void(StringHash, VariantMap &)> &function,
                          void *userData = nullptr);
    /// Subscribe to a specific sender's event.
    void SubscribeToEvent(Object *sender, StringHash eventType,
                          const std::function<void(StringHash, VariantMap &)> &function, void *userData = nullptr);
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

    /// Return engine subsystem.
    class Engine* GetEngine() const;
    /// Return time subsystem.
    class Time* GetTime() const;
    /// Return work queue subsystem.
    class WorkQueue* GetWorkQueue() const;
    /// Return file system subsystem.
    class FileSystem* GetFileSystem() const;
    /// Return resource cache subsystem.
    class ResourceCache *GetCache() const;
    /// Return input subsystem.
    class Input *GetInput() const;
    /// Return audio subsystem.
    class Audio *GetAudio() const;
    /// Return UI subsystem.
    class UI *GetUI() const;
#if LUTEFISK3D_SYSTEMUI
    /// Return system ui subsystem.
    class SystemUI* GetSystemUI() const;
#endif
    /// Return graphics subsystem.
    class Graphics* GetContextGraphics() const;
protected:
    /// Execution context.
    Context* context_;

private:
    cilEventHandler FindEventHandler(StringHash eventType) const;
    cilEventHandler FindSpecificEventHandler(Object* sender) const;
    cilEventHandler FindSpecificEventHandler(Object* sender, StringHash eventType, EventHandler** previous = 0) const;
    void RemoveEventSender(Object* sender);
    /// Event handlers. Sender is null for non-specific handlers.
    PODVectorN<EventHandler *,2> eventHandlers_;
};

template <class T> T* Object::GetSubsystem() const { return static_cast<T*>(GetSubsystem(T::GetTypeStatic())); }


/// Base class for object factories.
class LUTEFISK3D_EXPORT ObjectFactory : public RefCounted
{
public:
    /// Construct.
    explicit ObjectFactory(Context* context) :
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
    Context* context_; //!< Execution context.
    const TypeInfo* typeInfo_; //!< Type info.
};

/// Template implementation of the object factory.
template <class T> 
class ObjectFactoryImpl : public ObjectFactory
{
public:
    /// Construct.
    explicit ObjectFactoryImpl(Context* context) :
        ObjectFactory(context)
    {
        typeInfo_ = T::GetTypeInfoStatic();
        allocator_ = AllocatorInitialize(sizeof(T));
    }
    ~ObjectFactoryImpl() override
    {
        AllocatorUninitialize(allocator_);
        allocator_ = nullptr;
    }
    /// Create an object of the specific type.
    SharedPtr<Object> CreateObject() override
    {
        auto* newObject = static_cast<T*>(AllocatorReserve(allocator_));
        new(newObject) T(context_);
        newObject->SetDeleter([this, newObject](RefCounted* /*refCounted*/) {
            newObject->~T();
            AllocatorFree(allocator_, newObject);
        });
        return SharedPtr<Object>(newObject);
    }

private:
    AllocatorBlock* allocator_;
};

/// Internal helper class for invoking event handler functions.
class LUTEFISK3D_EXPORT EventHandler
{
public:
    /// Construct with specified receiver and userdata.
    explicit EventHandler(Object* receiver, void* userData = nullptr) :
        receiver_(receiver),
        sender_(nullptr),
        userData_(userData)
    {
    }

    virtual ~EventHandler() = default;

    /// Set sender and event type.
    void SetSenderAndEventType(Object* sender, StringHash eventType)
    {
        sender_ = sender;
        eventType_ = eventType;
    }

    /// Invoke event handler function.
    virtual void Invoke(VariantMap& eventData) = 0;

    /// Return event receiver.
    Object* GetReceiver() const { return receiver_; }
    /// Return event sender. Null if the handler is non-specific.
    Object* GetSender() const { return sender_; }
    /// Return event type.
    const StringHash& GetEventType() const { return eventType_; }
    /// Return userdata.
    void* GetUserData() const { return userData_; }

protected:
    Object* receiver_;      //!< Event receiver.
    Object* sender_;        //!< Event sender.
    StringHash eventType_;  //!< Event type.
    void* userData_;        //!< Event user data.
};

/// Template implementation of the event handler invoke helper (stores a function pointer of specific class.)
template <class T> class EventHandlerImpl : public EventHandler
{
public:
    using HandlerFunctionPtr = void (T::*)(StringHash, VariantMap&);

    /// Construct with receiver and function pointers and userdata.
    EventHandlerImpl(T* receiver, HandlerFunctionPtr function, void* userData = nullptr) :
        EventHandler(receiver, userData),
        function_(function)
    {
        assert(receiver_);
        assert(function_);
    }

    /// Invoke event handler function.
    void Invoke(VariantMap& eventData) override
    {
        T* receiver = static_cast<T*>(receiver_);
        (receiver->*function_)(eventType_, eventData);
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

#define LUTEFISK_SUBSCRIBE_LAMBDA(sender,eventType,method)\
    SubscribeToEvent(sender,eventType, ([this](StringHash , VariantMap& eventData) { this->method(eventType,eventData); }))
#define LUTEFISK_SUBSCRIBE_GLOBAL(eventType,method)\
    SubscribeToEvent(eventType, ([this](StringHash , VariantMap& eventData) { this->method(eventType,eventData); }))
}
#include "Lutefisk3D/Container/DataHandle.h"
