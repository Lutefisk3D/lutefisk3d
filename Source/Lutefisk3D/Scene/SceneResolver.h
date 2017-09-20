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
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include <memory>
//TODO: make this header private ?
namespace Urho3D
{
class LUTEFISK3D_EXPORT Component;
class LUTEFISK3D_EXPORT Node;
class SceneResolverPrivate;

/// Utility class that resolves node & component IDs after a scene or partial scene load.
/// \note This class only holds weak references to the nodes and components
class LUTEFISK3D_EXPORT SceneResolver
{
public:
    SceneResolver();
    ~SceneResolver();
    void Reset();
    void AddNode(unsigned oldID, Node *node);
    void AddComponent(unsigned oldID, Component *component);
    void Resolve();

private:
    std::unique_ptr<SceneResolverPrivate> d;
};
}
