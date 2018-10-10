#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Scene/SceneEvents.h>
#include <Lutefisk3D/SystemUI/SystemUIEvents.h>
#include <Toolbox/SystemUI/AttributeInspector.h>
#include <Toolbox/SystemUI/Gizmo.h>
#include "UndoManager.h"

namespace Urho3D
{

namespace Undo
{

Manager::Manager(Context* ctx)
    : Object(ctx)
{
    g_coreSignals.endFrame.ConnectL([this]()
    {
        if (trackingEnabled_ && !currentFrameStates_.empty())
        {
            stack_.resize(index_);              // Discard unneeded states
            stack_.push_back(currentFrameStates_);
            index_++;
            currentFrameStates_.clear();
        }
    });
}
void Manager::onUndo(uint32_t target)
{
    if (index_ > 0)
    {
        // Pick a state with latest time
        auto time = stack_[index_ - 1][0]->time_;
        if (target < time)
        {
            // use undo manager with latest state recording
            this->Undo();
        }
    }
}
void Manager::onRedo(uint32_t target)
{
    if (index_ < stack_.size())
    {
        auto time = stack_[index_][0]->time_;
        if (target > time)
        {
            // Find and return undo manager with latest state recording
            this->Redo();
        }
    }
}
void Manager::Undo()
{
    bool isTracking = IsTrackingEnabled();
    SetTrackingEnabled(false);
    if (index_ > 0)
    {
        index_--;
        const auto& actions = stack_[index_];
        for (int i = actions.size() - 1; i >= 0; --i)
            actions[i]->Undo();
    }
    SetTrackingEnabled(isTracking);
}

void Manager::Redo()
{
    bool isTracking = IsTrackingEnabled();
    SetTrackingEnabled(false);
    if (index_ < stack_.size())
    {
        for (auto& action : stack_[index_])
            action->Redo();
        index_++;
    }
    SetTrackingEnabled(isTracking);
}

void Manager::Clear()
{
    stack_.clear();
    currentFrameStates_.clear();
    index_ = 0;
}

void Manager::Connect(Scene* scene)
{
    scene->nodeAdded.ConnectL([&](Scene *,Node *,Node *node)
    {
        if (!trackingEnabled_)
            return;
        Track<CreateNodeAction>(node);
    });

    scene->nodeRemoved.ConnectL([&](Scene *,Node *,Node *node)
    {
        if (!trackingEnabled_)
            return;
        Track<DeleteNodeAction>(node);
    });
    scene->componentAdded.ConnectL([&](Scene *,Node *,Component *comp)
    {
        if (!trackingEnabled_)
            return;
        Track<CreateComponentAction>(comp);
    });

    scene->componentRemoved.ConnectL([&](Scene *,Node *,Component *comp)
    {
        if (!trackingEnabled_)
            return;
        Track<DeleteComponentAction>(comp);
    });
}

void Manager::Connect(AttributeInspector* inspector)
{
    connect(inspector,&AttributeInspector::AttributeInspectorValueModified,
            [&](Serializable *ser,const AttributeInfo *attr,const Variant *oldValue,Variant *nv)
    {
        if (!trackingEnabled_)
            return;
        const auto& name = attr->name_;
        const auto& newValue = ser->GetAttribute(name);
        if (*oldValue != newValue)
        {
            // Dummy attributes are used for rendering custom inspector widgets that do not map to Variant values.
            // These dummy values are not modified, however inspector event is still useful for tapping into their
            // modifications. State tracking for these dummy values is not needed and would introduce extra ctrl+z
            // presses that do nothing.
            Track<EditAttributeAction>(ser, name, *oldValue, newValue);
        }
    });
}

void Manager::Connect(UIElement* root)
{
    SubscribeToEvent(E_ELEMENTADDED, [&, root](StringHash, VariantMap& args)
    {
        if (!trackingEnabled_)
            return;
        using namespace ElementAdded;
        auto eventRoot = dynamic_cast<UIElement*>(args[P_ROOT].GetPtr());
        if (root != eventRoot)
            return;
        Track<CreateUIElementAction>(dynamic_cast<UIElement*>(args[P_ELEMENT].GetPtr()));
    });

    SubscribeToEvent(E_ELEMENTREMOVED, [&, root](StringHash, VariantMap& args)
    {
        if (!trackingEnabled_)
            return;
        using namespace ElementRemoved;
        auto eventRoot = dynamic_cast<UIElement*>(args[P_ROOT].GetPtr());
        if (root != eventRoot)
            return;
        Track<DeleteUIElementAction>(dynamic_cast<UIElement*>(args[P_ELEMENT].GetPtr()));
    });
}

void Manager::Connect(Gizmo* gizmo)
{
    this->connect(gizmo,&Gizmo::nodeModified,[&](Node *node, Matrix3x4 oldTransform,Matrix3x4 newTransform)
    {
        if (!trackingEnabled_)
            return;
        Track<EditAttributeAction>(node, "Position", oldTransform.Translation(), newTransform.Translation());
        Track<EditAttributeAction>(node, "Rotation", oldTransform.Rotation(), newTransform.Rotation());
        Track<EditAttributeAction>(node, "Scale", oldTransform.Scale(), newTransform.Scale());
    });
}

}

}
