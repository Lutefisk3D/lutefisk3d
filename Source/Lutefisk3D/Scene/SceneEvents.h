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
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include <jlsignal/Signal.h>

class QString;
namespace Urho3D
{
class Object;
class Scene;
class Node;
class Component;
class Serializable;
class Variant;
struct LUTEFISK3D_EXPORT SceneSignals
{
    /// Variable timestep scene update.
    jl::Signal<Scene *,float> sceneUpdate; //Scene *scene,float TimeStep
    /// A network attribute update from the server has been intercepted.
    /// Scene *scene,TimeStamp(0-255),Index,Name,Value
    jl::Signal<Serializable*,uint8_t,unsigned,const QString &,Variant> interceptNetworkUpdate;
    /// A serializable's temporary state has changed.
    jl::Signal<Serializable*> temporaryChanged;
    void init(jl::ScopedAllocator *allocator)
    {
        sceneUpdate.SetAllocator(allocator);
        interceptNetworkUpdate.SetAllocator(allocator);
        temporaryChanged.SetAllocator(allocator);
    }
};
extern LUTEFISK3D_EXPORT SceneSignals g_sceneSignals;

struct SingularSceneSignals {
    /// Scene drawable update finished. Custom animation (eg. IK) can be done at this point.
    jl::Signal<Scene *,float> sceneDrawableUpdateFinished; //Scene *scene,float TimeStep
    /// Variable timestep scene post-update.
    jl::Signal<Scene *,float> scenePostUpdate; //Scene *scene,float TimeStep
    /// Scene subsystem update.
    jl::Signal<Scene *,float> sceneSubsystemUpdate; //Scene *scene,float TimeStep
    /// Scene transform smoothing update.
    jl::Signal<float,float> updateSmoothing; //Constant, SquaredSnapThreshold

    /// A node's name has changed.
    jl::Signal<Scene *,Node *> nodeNameChagned; //Scene *scene,Node *
    ///// A node's tag has been added.
    jl::Signal<Scene *,Node *,const QString &> nodeTagAdded; //Scene *scene,Node *,const QString &tag
    ///// A node's tag has been added.
    jl::Signal<Scene *,Node *,const QString &> nodeTagRemoved; //Scene *scene,Node *,const QString &tag
    ///// A child node is about to be removed from a parent node. Note that individual component removal events will not be sent.
    jl::Signal<Scene *,Node *,Node *> nodeRemoved; //scene,parent,node
    ///// A child node has been added to a parent node.
    jl::Signal<Scene *,Node *,Node *> nodeAdded; //scene,parent,node
    ///// A node (and its children and components) has been cloned.
    jl::Signal<Scene *,Node *,Node *> nodeCloned; //scene,Node,CloneNode

    ///// A component has been cloned.
    jl::Signal<Scene *,Component *,Component *> componentCloned; //scene,component,it's clone
    ///// A component has been created to a node.
    jl::Signal<Scene *,Node *,Component *> componentAdded;
    ///// A component is about to be removed from a node.
    jl::Signal<Scene *,Node *,Component *> componentRemoved;
    ///// A component's enabled state has changed.
    jl::Signal<Scene *,Node *,Component *> componentEnabledChanged;

    ///// A node's enabled state has changed.
    jl::Signal<Scene *,Node *> nodeEnabledChanged; //Scene *scene,Node *

    ///// Scene attribute animation update.
    jl::Signal<Scene *,float> attributeAnimationUpdate; // Scene,TimeStep

    /// Asynchronous scene loading progress.
    /// Scene,Progress,LoadedNodes,TotalNodes,LoadedResources,TotalResources
    jl::Signal<Scene *,float,int,int,int,int> asyncLoadProgress;
    ///// Asynchronous scene loading finished.
    jl::Signal<Scene *> asyncLoadFinished;
};
struct ObjectAnimationSignals {
    ///// Attribute animation added to object animation.
    jl::Signal<Object *,const QString &> attributeAnimationAdded; // ObjectAnimation,AttributeAnimationName
    ///// Attribute animation removed from object animation.
    jl::Signal<Object *,const QString &> attributeAnimationRemoved; // ObjectAnimation,AttributeAnimationName
};

struct SmoothedTransformSignals {
    /// SmoothedTransform target position changed.
    jl::Signal<> targetPositionChanged;
    /// SmoothedTransform target position changed.
    jl::Signal<> targetRotationChanged;
};
}
