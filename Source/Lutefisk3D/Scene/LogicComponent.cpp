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

#include "LogicComponent.h"
#include "Scene.h"
#include "SceneEvents.h"

#include "Lutefisk3D/IO/Log.h"
#if defined(LUTEFISK3D_PHYSICS) || defined(LUTEFISK3D_URHO2D)
#include "Lutefisk3D/Physics/PhysicsEvents.h"
#include "Lutefisk3D/Physics/PhysicsWorld.h"
#endif

namespace Urho3D
{

LogicComponent::LogicComponent(Context* context) :
    Component(context),
    updateEventMask_(USE_UPDATE | USE_POSTUPDATE | USE_FIXEDUPDATE | USE_FIXEDPOSTUPDATE),
    currentEventMask_(0),
    delayedStartCalled_(false)
{
}

void LogicComponent::OnSetEnabled()
{
    UpdateEventSubscription();
}

void LogicComponent::Update(float timeStep)
{
}

void LogicComponent::PostUpdate(float timeStep)
{
}

void LogicComponent::FixedUpdate(float timeStep)
{
}

void LogicComponent::FixedPostUpdate(float timeStep)
{
}

void LogicComponent::SetUpdateEventMask(unsigned char mask)
{
    if (updateEventMask_ != mask)
    {
        updateEventMask_ = mask;
        UpdateEventSubscription();
    }
}

void LogicComponent::OnNodeSet(Node* node)
{
    if (node != nullptr)
    {
        // Execute the user-defined start function
        Start();
    }
    else
    {
        // We are being detached from a node: execute user-defined stop function and prepare for destruction
        Stop();
    }
}

void LogicComponent::OnSceneSet(Scene* scene)
{
    if (scene != nullptr)
        UpdateEventSubscription();
    else
    {
        UnsubscribeFromEvent(E_SCENEUPDATE);
        UnsubscribeFromEvent(E_SCENEPOSTUPDATE);
#if defined(LUTEFISK3D_PHYSICS) || defined(LUTEFISK3D_URHO2D)
        UnsubscribeFromEvent(E_PHYSICSPRESTEP);
        UnsubscribeFromEvent(E_PHYSICSPOSTSTEP);
#endif
        currentEventMask_ = 0;
    }
}

void LogicComponent::UpdateEventSubscription()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    bool enabled = IsEnabledEffective();

    bool needUpdate = enabled && (((updateEventMask_ & USE_UPDATE) != 0) || !delayedStartCalled_);
    if (needUpdate && ((currentEventMask_ & USE_UPDATE) == 0))
    {
        SubscribeToEvent(scene, E_SCENEUPDATE, URHO3D_HANDLER(LogicComponent, HandleSceneUpdate));
        currentEventMask_ |= USE_UPDATE;
    }
    else if (!needUpdate && ((currentEventMask_ & USE_UPDATE) != 0))
    {
        UnsubscribeFromEvent(scene, E_SCENEUPDATE);
        currentEventMask_ &= ~USE_UPDATE;
    }

    bool needPostUpdate = enabled && ((updateEventMask_ & USE_POSTUPDATE) != 0);
    if (needPostUpdate && ((currentEventMask_ & USE_POSTUPDATE) == 0))
    {
        SubscribeToEvent(scene, E_SCENEPOSTUPDATE, URHO3D_HANDLER(LogicComponent, HandleScenePostUpdate));
        currentEventMask_ |= USE_POSTUPDATE;
    }
    else if (!needPostUpdate && ((currentEventMask_ & USE_POSTUPDATE) != 0))
    {
        UnsubscribeFromEvent(scene, E_SCENEPOSTUPDATE);
        currentEventMask_ &= ~USE_POSTUPDATE;
    }

#if defined(LUTEFISK3D_PHYSICS) || defined(LUTEFISK3D_URHO2D)
    Component* world = GetFixedUpdateSource();
    if (world == nullptr)
        return;

    bool needFixedUpdate = enabled && ((updateEventMask_ & USE_FIXEDUPDATE) != 0);
    if (needFixedUpdate && ((currentEventMask_ & USE_FIXEDUPDATE) == 0))
    {
        SubscribeToEvent(world, E_PHYSICSPRESTEP, URHO3D_HANDLER(LogicComponent, HandlePhysicsPreStep));
        currentEventMask_ |= USE_FIXEDUPDATE;
    }
    else if (!needFixedUpdate && ((currentEventMask_ & USE_FIXEDUPDATE) != 0))
    {
        UnsubscribeFromEvent(world, E_PHYSICSPRESTEP);
        currentEventMask_ &= ~USE_FIXEDUPDATE;
    }

    bool needFixedPostUpdate = enabled && ((updateEventMask_ & USE_FIXEDPOSTUPDATE) != 0);
    if (needFixedPostUpdate && ((currentEventMask_ & USE_FIXEDPOSTUPDATE) == 0))
    {
        SubscribeToEvent(world, E_PHYSICSPOSTSTEP, URHO3D_HANDLER(LogicComponent, HandlePhysicsPostStep));
        currentEventMask_ |= USE_FIXEDPOSTUPDATE;
    }
    else if (!needFixedPostUpdate && ((currentEventMask_ & USE_FIXEDPOSTUPDATE) != 0))
    {
        UnsubscribeFromEvent(world, E_PHYSICSPOSTSTEP);
        currentEventMask_ &= ~USE_FIXEDPOSTUPDATE;
    }
#endif
}

void LogicComponent::HandleSceneUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace SceneUpdate;

    // Execute user-defined delayed start function before first update
    if (!delayedStartCalled_)
    {
        DelayedStart();
        delayedStartCalled_ = true;

        // If did not need actual update events, unsubscribe now
        if ((updateEventMask_ & USE_UPDATE) == 0)
        {
            UnsubscribeFromEvent(GetScene(), E_SCENEUPDATE);
            currentEventMask_ &= ~USE_UPDATE;
            return;
        }
    }

    // Then execute user-defined update function
    Update(eventData[P_TIMESTEP].GetFloat());
}

void LogicComponent::HandleScenePostUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace ScenePostUpdate;

    // Execute user-defined post-update function
    PostUpdate(eventData[P_TIMESTEP].GetFloat());
}

#if defined(LUTEFISK3D_PHYSICS) || defined(LUTEFISK3D_URHO2D)
void LogicComponent::HandlePhysicsPreStep(StringHash eventType, VariantMap& eventData)
{
    using namespace PhysicsPreStep;

    // Execute user-defined delayed start function before first fixed update if not called yet
    if (!delayedStartCalled_)
    {
        DelayedStart();
        delayedStartCalled_ = true;
    }
    // Execute user-defined fixed update function
    FixedUpdate(eventData[P_TIMESTEP].GetFloat());
}

void LogicComponent::HandlePhysicsPostStep(StringHash eventType, VariantMap& eventData)
{
    using namespace PhysicsPostStep;

    // Execute user-defined fixed post-update function
    FixedPostUpdate(eventData[P_TIMESTEP].GetFloat());
}
#endif

}
