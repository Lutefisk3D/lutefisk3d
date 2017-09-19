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
class Graphics;
class Time;
class Profiler;
class EventProfiler;
class Renderer;
class WorkQueue;

template<class T> class ObjectFactoryImpl;
template<class U,class V> class HashMap;
template<class T> class SharedPtr;
template <class T, class U> SharedPtr<T> StaticCast(const SharedPtr<U>& ptr);

struct AttributeInfo;
class LUTEFISK3D_EXPORT EventReceiverGroup;
class ContextPrivate;
class LUTEFISK3D_EXPORT Context
{
    friend class Object;
    friend class Context_EventGuard;
public:
    Context();
    ~Context();

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

    jl::ScopedAllocator *          m_signal_allocator; // Those point to static instances, no need to free them
    jl::ScopedAllocator *          m_observer_allocator;

    template <class T> inline SharedPtr<T> CreateObject() { return StaticCast<T>(CreateObject(T::GetTypeStatic())); }
    SharedPtr<Object> CreateObject(StringHash objectType);
    void RegisterFactory(ObjectFactory* factory, const char* category=nullptr);
    void RegisterSubsystem(StringHash typeHash, Object* subsystem);
    void RemoveSubsystem(StringHash objectType);
    void RegisterAttribute(StringHash objectType, const AttributeInfo& attr);
    void RemoveAttribute(StringHash objectType, const char* name);
    void UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue);
    HashMap<StringHash, Variant>& GetEventDataMap();
    bool RequireSDL(unsigned int sdlFlags);
    void ReleaseSDL();

    void CopyBaseAttributes(StringHash baseType, StringHash derivedType);

    template <class T> void RegisterFactory(const char* category=nullptr);
    template <class T> void RemoveSubsystem();
    template <class T> void RegisterAttribute(const AttributeInfo& attr);
    template <class T> void RemoveAttribute(const char* name);
    template <class T, class U> void CopyBaseAttributes();
    template <class T> void UpdateAttributeDefaultValue(const char* name, const Variant& defaultValue);


    Object* GetSubsystem(StringHash type) const;

    const QString &GetObjectCategory(StringHash objType) const;
    Object* GetEventSender() const;
    EventHandler* GetEventHandler() const { return eventHandler_; }
    const QString& GetTypeName(StringHash objectType) const;

    AttributeInfo* GetAttribute(StringHash objectType, const char* name);
    template <class T> T* GetSubsystemT() const;
    template <class T> AttributeInfo* GetAttribute(const char* name);
    const std::vector<AttributeInfo>* GetAttributes(StringHash type) const;
    const std::vector<AttributeInfo>* GetNetworkAttributes(StringHash type) const;
    EventReceiverGroup* GetEventReceivers(Object* sender, StringHash eventType);
    EventReceiverGroup* GetEventReceivers(StringHash eventType);

private:

    void AddEventReceiver(Object* receiver, StringHash eventType);
    void AddEventReceiver(Object* receiver, Object* sender, StringHash eventType);
    void RemoveEventSender(Object* sender);
    void RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType);
    void RemoveEventReceiver(Object* receiver, StringHash eventType);
    void BeginSendEvent(Object* sender, StringHash eventType);
    void EndSendEvent();
    void SetEventHandler(EventHandler* handler) { eventHandler_ = handler; }
    std::unique_ptr<ContextPrivate> d;
    std::vector<Object*> eventSenders_;
    EventHandler* eventHandler_=nullptr;
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
template <class T> void Context::RegisterAttribute(const AttributeInfo& attr) { RegisterAttribute(T::GetTypeStatic(), attr); }
template <class T> void Context::RemoveAttribute(const char* name) { RemoveAttribute(T::GetTypeStatic(), name); }
template <class T, class U> void Context::CopyBaseAttributes() { CopyBaseAttributes(T::GetTypeStatic(), U::GetTypeStatic()); }
template <class T> T* Context::GetSubsystemT() const { return static_cast<T*>(GetSubsystem(T::GetTypeStatic())); }
template <class T> AttributeInfo* Context::GetAttribute(const char* name) { return GetAttribute(T::GetTypeStatic(), name); }
template <class T> void Context::UpdateAttributeDefaultValue(const char* name, const Variant& defaultValue) { UpdateAttributeDefaultValue(T::GetTypeStatic(), name, defaultValue); }

}
