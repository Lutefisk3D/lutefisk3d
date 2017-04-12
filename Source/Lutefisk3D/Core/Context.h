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

#include "Lutefisk3D/Core/Attribute.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Core/Object.h"
#include <QtCore/QSet>
#include <QtCore/QString>


namespace Urho3D
{

class URHO3D_API EventReceiverGroup : public RefCounted
{
public:
    void BeginSendEvent();
    void EndSendEvent();
    void Add(Object *object);
    void Remove(Object *object);
    std::vector<Object *> receivers_;

private:
    unsigned inSend_ = 0;
    bool     dirty_  = false;
};

class URHO3D_API Context : public RefCounted
{
    friend class Object;
    friend class Context_EventGuard;
public:
    Context();
    ~Context();

    template <class T> inline SharedPtr<T> CreateObject() { return StaticCast<T>(CreateObject(T::GetTypeStatic())); }
    SharedPtr<Object> CreateObject(StringHash objectType);
    void RegisterFactory(ObjectFactory* factory, const char* category=nullptr);
    void RegisterSubsystem(StringHash typeHash, Object* subsystem);
    void RemoveSubsystem(StringHash objectType);
    void RegisterAttribute(StringHash objectType, const AttributeInfo& attr);
    void RemoveAttribute(StringHash objectType, const char* name);
    void UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue);
    VariantMap& GetEventDataMap();
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
    const Variant &GetGlobalVar(StringHash key) const;
    const VariantMap& GetGlobalVars() const { return globalVars_; }
    
    void SetGlobalVar(StringHash key, const Variant &value);

    const HashMap<StringHash, SharedPtr<Object> >& GetSubsystems() const { return subsystems_; }
    const HashMap<StringHash, SharedPtr<ObjectFactory> >& GetObjectFactories() const { return factories_; }
    const HashMap<QString, std::vector<StringHash> >& GetObjectCategories() const { return objectCategories_; }

    Object* GetEventSender() const;
    EventHandler* GetEventHandler() const { return eventHandler_; }
    const QString& GetTypeName(StringHash objectType) const;

    AttributeInfo* GetAttribute(StringHash objectType, const char* name);
    template <class T> T* GetSubsystem() const;
    template <class T> AttributeInfo* GetAttribute(const char* name);

    const std::vector<AttributeInfo>* GetAttributes(StringHash type) const
    {
        HashMap<StringHash, std::vector<AttributeInfo> >::const_iterator i = attributes_.find(type);
        return i != attributes_.end() ? &(MAP_VALUE(i)) : nullptr;
    }

    const std::vector<AttributeInfo>* GetNetworkAttributes(StringHash type) const
    {
        HashMap<StringHash, std::vector<AttributeInfo> >::const_iterator i = networkAttributes_.find(type);
        return i != networkAttributes_.end() ? &(MAP_VALUE(i)) : nullptr;
    }

    const HashMap<StringHash, std::vector<AttributeInfo> >& GetAllAttributes() const { return attributes_; }

    EventReceiverGroup* GetEventReceivers(Object* sender, StringHash eventType)
    {
        auto i = specificEventReceivers_.find(sender);
        if (i != specificEventReceivers_.end())
        {
            auto j = MAP_VALUE(i).find(eventType);
            return j != MAP_VALUE(i).end() ? MAP_VALUE(j) : nullptr;
        }
        else
            return nullptr;
    }
    EventReceiverGroup* GetEventReceivers(StringHash eventType)
    {
        auto i = eventReceivers_.find(eventType);
        return i != eventReceivers_.end() ? MAP_VALUE(i) : nullptr;
    }

private:

    void AddEventReceiver(Object* receiver, StringHash eventType);
    void AddEventReceiver(Object* receiver, Object* sender, StringHash eventType);
    void RemoveEventSender(Object* sender);
    void RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType);
    void RemoveEventReceiver(Object* receiver, StringHash eventType);
    void BeginSendEvent(Object* sender, StringHash eventType);
    void EndSendEvent();
    void SetEventHandler(EventHandler* handler) { eventHandler_ = handler; }
    HashMap<StringHash, SharedPtr<ObjectFactory> > factories_;
    HashMap<StringHash, SharedPtr<Object> > subsystems_;
    HashMap<StringHash, std::vector<AttributeInfo> > attributes_;
    HashMap<StringHash, std::vector<AttributeInfo> > networkAttributes_;
    HashMap<StringHash, SharedPtr<EventReceiverGroup> > eventReceivers_;
    HashMap<Object*, HashMap<StringHash, SharedPtr<EventReceiverGroup> > > specificEventReceivers_;
    std::vector<Object*> eventSenders_;
    std::vector<VariantMap*> eventDataMaps_;
    EventHandler* eventHandler_;
    HashMap<QString, std::vector<StringHash> > objectCategories_;
    VariantMap globalVars_;
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
template <class T> T* Context::GetSubsystem() const { return static_cast<T*>(GetSubsystem(T::GetTypeStatic())); }
template <class T> AttributeInfo* Context::GetAttribute(const char* name) { return GetAttribute(T::GetTypeStatic(), name); }
template <class T> void Context::UpdateAttributeDefaultValue(const char* name, const Variant& defaultValue) { UpdateAttributeDefaultValue(T::GetTypeStatic(), name, defaultValue); }

}
