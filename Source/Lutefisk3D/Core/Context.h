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
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Core/Attribute.h"
#include <memory>
#include <vector>
#ifdef _MSC_VER
#include <iso646.h>
#endif
namespace jl {
class ScopedAllocator;
}
class QString;
namespace Urho3D
{
#ifndef LUTEFISK3D_UILESS
class UI;
#else
class UI {};
#endif
class EventHandler;
class ObjectFactory;
class Variant;
class Log;
class Object;
class FileSystem;
class Input;
class ResourceCache;
class LUTEFISK3D_EXPORT Graphics;
class Time;
class Profiler;
class EventProfiler;
class Renderer;
class WorkQueue;

template<class T> class ObjectFactoryImpl;
template<class U,class V> class HashMap;
template<class T> class SharedPtr;
template <class T, class U> SharedPtr<T> StaticCast(const SharedPtr<U>& ptr);
}
namespace Urho3D
{
enum OSInterfaceFlags {
    GFX_SYS = 1,
    INPUT_SYS = 2,
    AUDIO_SYS = 4,
};
struct AttributeInfo;
class LUTEFISK3D_EXPORT EventReceiverGroup;
class ContextPrivate;
class Context
{
    friend class Object;
    friend class Context_EventGuard;
    friend class Engine; //Engine initializes m_signal_allocator/m_observer_allocator
public:
    LUTEFISK3D_EXPORT Context();
    LUTEFISK3D_EXPORT ~Context();
    jl::ScopedAllocator *signalAllocator() { return m_signal_allocator; }
    jl::ScopedAllocator *observerAllocator() { return m_observer_allocator; }
    ResourceCache* resourceCache() const { return m_ResourceCache.get(); }

    template <class T> 
    SharedPtr<T> CreateObject() { return StaticCast<T>(CreateObject(T::GetTypeStatic())); }
    LUTEFISK3D_EXPORT SharedPtr<Object> CreateObject(StringHash objectType);
    LUTEFISK3D_EXPORT void RegisterFactory(ObjectFactory* factory, const char* category=nullptr);
    LUTEFISK3D_EXPORT void RegisterSubsystem(StringHash typeHash, Object* subsystem);
    LUTEFISK3D_EXPORT void RemoveSubsystem(StringHash objectType);
    LUTEFISK3D_EXPORT AttributeHandle RegisterAttribute(StringHash objectType, const AttributeInfo& attr);
    LUTEFISK3D_EXPORT void RemoveAttribute(StringHash objectType, const char* name);
    LUTEFISK3D_EXPORT void UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue);
    LUTEFISK3D_EXPORT HashMap<StringHash, Variant>& GetEventDataMap();

    LUTEFISK3D_EXPORT void CopyBaseAttributes(StringHash baseType, StringHash derivedType);

    template <class T> void RegisterFactory(const char* category=nullptr);
    template <class T> void RemoveSubsystem();
    template <class T> AttributeHandle RegisterAttribute(const AttributeInfo& attr);
    template <class T> void RemoveAttribute(const char* name);
    template <class T, class U> void CopyBaseAttributes();
    template <class T> void UpdateAttributeDefaultValue(const char* name, const Variant& defaultValue);


    LUTEFISK3D_EXPORT Object* GetSubsystem(StringHash type) const;

    LUTEFISK3D_EXPORT const QString &GetObjectCategory(StringHash objType) const;
    LUTEFISK3D_EXPORT Object* GetEventSender() const;
    EventHandler* GetEventHandler() const { return eventHandler_; }
    LUTEFISK3D_EXPORT const QString& GetTypeName(StringHash objectType) const;

    LUTEFISK3D_EXPORT AttributeInfo* GetAttribute(StringHash objectType, const char* name);
    template <class T> T* GetSubsystemT() const;
    template <class T> AttributeInfo* GetAttribute(const char* name);
    LUTEFISK3D_EXPORT const std::vector<AttributeInfo>* GetAttributes(StringHash type) const;
    LUTEFISK3D_EXPORT const std::vector<AttributeInfo>* GetNetworkAttributes(StringHash type) const;
    LUTEFISK3D_EXPORT EventReceiverGroup* GetEventReceivers(Object* sender, StringHash eventType);
    LUTEFISK3D_EXPORT EventReceiverGroup* GetEventReceivers(StringHash eventType);

    std::unique_ptr<Log>           m_LogSystem;
    std::unique_ptr<FileSystem>    m_FileSystem;
    std::unique_ptr<Input>         m_InputSystem;
    std::unique_ptr<ResourceCache> m_ResourceCache;
    std::unique_ptr<Graphics>      m_Graphics;
    std::unique_ptr<Renderer>      m_Renderer;
    std::unique_ptr<Time>          m_TimeSystem;
    std::unique_ptr<Profiler>      m_ProfilerSystem;
    std::unique_ptr<EventProfiler> m_EventProfilerSystem;
    std::unique_ptr<WorkQueue>     m_WorkQueueSystem;
    std::unique_ptr<UI>            m_UISystem;

private:

    void AddEventReceiver(Object* receiver, StringHash eventType);
    void AddEventReceiver(Object* receiver, Object* sender, StringHash eventType);
    void RemoveEventSender(Object* sender);
    void RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType);
    void RemoveEventReceiver(Object* receiver, StringHash eventType);
    void BeginSendEvent(Object* sender, StringHash eventType);
    void EndSendEvent();
    void SetEventHandler(EventHandler* handler) { eventHandler_ = handler; }
    // Those two allocators point to static instances, no need to free them
    jl::ScopedAllocator *  m_signal_allocator; 
    jl::ScopedAllocator *  m_observer_allocator;

    std::unique_ptr<ContextPrivate> d;
    std::vector<Object*> eventSenders_;
    EventHandler* eventHandler_=nullptr;
    uint32_t initializedOSInterfaces_=0; //OSInterfaceFlags
};

class Context_EventGuard
{
    Context &m_guarded;
public:
    Context_EventGuard(Context & guarded,Object *ob,StringHash etype) : m_guarded(guarded) {
        m_guarded.BeginSendEvent(ob,etype);
    }
    ~Context_EventGuard() {
        m_guarded.EndSendEvent();
    }
};
template <class T> void Context::RegisterFactory(const char* category) { RegisterFactory(new ObjectFactoryImpl<T>(this), category); }
template <class T> void Context::RemoveSubsystem() { RemoveSubsystem(T::GetTypeStatic()); }
template <class T> AttributeHandle Context::RegisterAttribute(const AttributeInfo& attr) { return RegisterAttribute(T::GetTypeStatic(), attr); }
template <class T> void Context::RemoveAttribute(const char* name) { RemoveAttribute(T::GetTypeStatic(), name); }
template <class T, class U> void Context::CopyBaseAttributes() { CopyBaseAttributes(T::GetTypeStatic(), U::GetTypeStatic()); }
template <class T> T* Context::GetSubsystemT() const { return static_cast<T*>(GetSubsystem(T::GetTypeStatic())); }
template <class T> AttributeInfo* Context::GetAttribute(const char* name) { return GetAttribute(T::GetTypeStatic(), name); }
template <class T> void Context::UpdateAttributeDefaultValue(const char* name, const Variant& defaultValue)
{
    UpdateAttributeDefaultValue(T::GetTypeStatic(), name, defaultValue);
}

}
