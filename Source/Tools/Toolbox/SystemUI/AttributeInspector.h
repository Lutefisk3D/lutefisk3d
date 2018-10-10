//
// Copyright (c) 2018 Rokas Kupstys
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


#include <array>

#include "ToolboxAPI.h"
#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Core/Object.h>
#include <Lutefisk3D/Core/Variant.h>
#include <Lutefisk3D/Engine/jlsignal/Signal.h>
#include <Lutefisk3D/SystemUI/SystemUI.h>

#include <QObject>

namespace Urho3D
{

class Viewport;


/// Automate tracking of initial values that are modified by ImGui widget.
template<typename TValue>
struct ModifiedStateTracker
{
    bool TrackModification(bool modified, std::function<TValue()> getInitial)
    {
        if (modified)
        {
            if (!lastFrameModified_)
            {
                initial_ = getInitial();
                lastFrameModified_ = true;
            }
            return false;
        }
        else if (!ui::IsAnyItemActive() && lastFrameModified_)
        {
            lastFrameModified_ = false;
            return true;
        }

        return false;
    }

    bool TrackModification(bool modified, const TValue& initialValue)
    {
        std::function<TValue()> fn = [&]() { return initialValue; };
        return TrackModification(modified, fn);
    }

    const TValue& GetInitialValue() { return initial_; }

protected:
    /// Initial value.
    TValue initial_{};
    /// Flag indicating if value was modified on previous frame.
    bool lastFrameModified_ = false;
};

/// A dummy object used as namespace for subscribing to events.
class URHO3D_TOOLBOX_API AttributeInspector : public QObject, public Object
{
    Q_OBJECT
    URHO3D_OBJECT(AttributeInspector, Object);
    bool RenderSingleAttribute(Object *eventNamespace, const AttributeInfo *info, Variant &value);
public:
    explicit AttributeInspector(Context* context) : Object(context) { }
/// Render attribute inspector of `item`.
/// If `filter` is not null then only attributes containing this substring will be rendered.
/// If `eventNamespace` is not null then this object will be used to send events.
    bool RenderAttributes(Serializable* item, const char* filter=nullptr);
    bool RenderSingleAttribute(Variant& value);
signals:
    void InspectorLocateResource(const QString &);
    void InspectorRenderStart(Serializable*);
    void InspectorRenderEnd();
    //AttributeInfo,Serializable,Handled,Modified
    void InspectorRenderAttribute(RefCounted *Serializable,const AttributeInfo *info,bool *Handled,bool *Modified);
    void AttributeInspectorAttribute(Serializable *ser,const AttributeInfo *attr,Color *c,bool *hidden,QString *tooltip);
    void AttributeInspectorValueModified(Serializable *ser,const AttributeInfo *attr,const Variant *oldValue,Variant *newValue);
    void AttributeInspectorMenu(Serializable *ser,const AttributeInfo *attr);
};


}

namespace ImGui
{

URHO3D_TOOLBOX_API void SameLine(Urho3D::VariantType type);

}
