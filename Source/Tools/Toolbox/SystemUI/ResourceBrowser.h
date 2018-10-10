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


#include "ToolboxAPI.h"
#include <Lutefisk3D/Container/FlagSet.h>
#include <Lutefisk3D/Core/Object.h>

#include <QObject>

namespace Urho3D
{

enum ResourceBrowserResult
{
    RBR_NOOP,
    RBR_ITEM_SELECTED,
    RBR_ITEM_OPEN,
    RBR_ITEM_CONTEXT_MENU,
};

enum ResourceBrowserFlag
{
    RBF_NONE,
    RBF_SCROLL_TO_CURRENT = 1,
    RBF_RENAME_CURRENT = 1 << 1,
    RBF_DELETE_CURRENT = 1 << 2,
};
URHO3D_FLAGSET(ResourceBrowserFlag, ResourceBrowserFlags);
struct URHO3D_TOOLBOX_API  ResourceBrowser : public QObject
{
    Q_OBJECT
public:
    ResourceBrowserFlags flags=RBF_NONE;
/// Create resource browser ui inside another window.
URHO3D_TOOLBOX_API ResourceBrowserResult updateAndRender(QString& path, QString& selected, ResourceBrowserFlags flags=RBF_NONE);
    void render();
signals:
    void ResourceBrowserRename(QString from,QString to);
    void ResourceBrowserDelete(QString name);
};


}
