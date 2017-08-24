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
#include "Lutefisk3D/Container/Ptr.h"
#include "SceneResolver.h"


#include "Component.h"
#include "Node.h"
#include "Lutefisk3D/IO/Log.h"


#include <QtCore/QSet>

namespace Urho3D
{

SceneResolver::SceneResolver()
{
}

SceneResolver::~SceneResolver()
{
}
/// Reset. Clear all remembered nodes and components.
void SceneResolver::Reset()
{
    nodes_.clear();
    components_.clear();
}
/// Remember a created node.
void SceneResolver::AddNode(unsigned oldID, Node* node)
{
    if (node != nullptr)
        nodes_[oldID] = node;
}
/// Remember a created component.
void SceneResolver::AddComponent(unsigned oldID, Component* component)
{
    if (component != nullptr)
        components_[oldID] = component;
}
/// Resolve component and node ID attributes and reset.
void SceneResolver::Resolve()
{
    // Nodes do not have component or node ID attributes, so only have to go through components
    QSet<StringHash> noIDAttributes;
    for (auto & elem : components_)
    {
        Component * component = ELEMENT_VALUE(elem);
        if ((component == nullptr) || noIDAttributes.contains(component->GetType()))
            continue;

        bool hasIDAttributes = false;
        const std::vector<AttributeInfo>* attributes = component->GetAttributes();
        if (attributes == nullptr)
        {
            noIDAttributes.insert(component->GetType());
            continue;
        }

        for (unsigned j = 0; j < attributes->size(); ++j)
        {
            const AttributeInfo& info = attributes->at(j);
            if ((info.mode_ & AM_NODEID) != 0u)
            {
                hasIDAttributes = true;
                unsigned oldNodeID = component->GetAttribute(j).GetUInt();

                if (oldNodeID != 0u)
                {
                    HashMap<unsigned, WeakPtr<Node> >::const_iterator k = nodes_.find(oldNodeID);

                    if (k != nodes_.end() && MAP_VALUE(k))
                    {
                        unsigned newNodeID = MAP_VALUE(k)->GetID();
                        component->SetAttribute(j, Variant(newNodeID));
                    }
                    else
                        URHO3D_LOGWARNING("Could not resolve node ID " + QString::number(oldNodeID));
                }
            }
            else if ((info.mode_ & AM_COMPONENTID) != 0u)
            {
                hasIDAttributes = true;
                unsigned oldComponentID = component->GetAttribute(j).GetUInt();

                if (oldComponentID != 0u)
                {
                    HashMap<unsigned, WeakPtr<Component> >::const_iterator k = components_.find(oldComponentID);

                    if (k != components_.end() && MAP_VALUE(k))
                    {
                        unsigned newComponentID = MAP_VALUE(k)->GetID();
                        component->SetAttribute(j, Variant(newComponentID));
                    }
                    else
                        URHO3D_LOGWARNING("Could not resolve component ID " + QString::number(oldComponentID));
                }
            }
            else if ((info.mode_ & AM_NODEIDVECTOR) != 0u)
            {
                hasIDAttributes = true;
                const VariantVector& oldNodeIDs = component->GetAttribute(j).GetVariantVector();

                if (oldNodeIDs.size() != 0u)
                {
                    // The first index stores the number of IDs redundantly. This is for editing
                    unsigned numIDs = oldNodeIDs[0].GetUInt();
                    VariantVector newIDs;
                    newIDs.push_back(numIDs);

                    for (unsigned k = 1; k < oldNodeIDs.size(); ++k)
                    {
                        unsigned oldNodeID = oldNodeIDs[k].GetUInt();
                        HashMap<unsigned, WeakPtr<Node> >::const_iterator l = nodes_.find(oldNodeID);

                        if (l != nodes_.end() && MAP_VALUE(l))
                            newIDs.push_back(MAP_VALUE(l)->GetID());
                        else
                        {
                            // If node was not found, retain number of elements, just store ID 0
                            newIDs.push_back(0);
                            URHO3D_LOGWARNING("Could not resolve node ID " + QString::number(oldNodeID));
                        }
                    }

                    component->SetAttribute(j, newIDs);
                }
            }
        }

        // If component type had no ID attributes, cache this fact for optimization
        if (!hasIDAttributes)
            noIDAttributes.insert(component->GetType());
    }

    // Attributes have been resolved, so no need to remember the nodes after this
    Reset();
}

}
