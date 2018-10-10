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


#include <Lutefisk3D/Scene/Component.h>
#include <Lutefisk3D/Scene/Serializable.h>
#include <Lutefisk3D/Resource/XMLElement.h>

#include <QObject>

namespace Urho3D
{

/// Class handling common scene settings
class SceneSettings : public QObject, public Component
{
    Q_OBJECT
    URHO3D_OBJECT(SceneSettings, Component);
public:
    /// Construct
    explicit SceneSettings(Context* context);
    /// Register object with engine.
    static void RegisterObject(Context* context);

    /// Returns configured editor viewport renderpath.
    ResourceRef GetEditorViewportRenderPath() const { return editorViewportRenderPath_; }

    /// Handle attribute write access.
    void OnSetAttribute(const AttributeInfo& attr, const Variant& src) override;
signals:
    void SceneSettingModified(Scene *s,const QString &name,const Variant &value);
protected:
    /// Flag indicating that editor scene viewport should use PBR renderpath.
    ResourceRef editorViewportRenderPath_;
};

}

