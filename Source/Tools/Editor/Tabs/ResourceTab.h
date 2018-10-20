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


#include <Toolbox/SystemUI/ResourceBrowser.h>
#include "Assets/Inspector/ResourceInspector.h"
#include "Tabs/Tab.h"


namespace Urho3D
{

/// Resource browser tab.
class ResourceTab : public Tab
{
    Q_OBJECT
    URHO3D_OBJECT(ResourceTab, Tab)
public:
    /// Construct.
    explicit ResourceTab(Context* context);

    /// Render content of tab window. Returns false if tab was closed.
    bool RenderWindowContent() override;
signals:
    void RenderInspectorRequest(const char *filter);
protected:
    void OpenMaterialInspector(const QString& resourcePath);
    ResourceBrowser m_browser;
    /// Constructs a name for newly created resource based on specified template name.
    QString GetNewResourcePath(const QString& name);
    /// Sends a notification to inspector tab to show inspector of specified resource.
//    template<typename TInspector, typename TResource>
//    void OpenResourceInspector(const QString& resourcePath);

    /// Current open resource path.
    QString resourcePath_;
    /// Current selected resource file name.
    QString resourceSelection_;
    /// Resource browser flags.
    ResourceBrowserFlags flags_{RBF_NONE};
};

}
