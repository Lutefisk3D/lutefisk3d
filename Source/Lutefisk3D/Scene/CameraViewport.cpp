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
#include "CameraViewport.h"

#include "SceneMetadata.h"
#include "Node.h"
#include "Scene.h"
#include "SceneEvents.h"

#include "Lutefisk3D/Core/StringUtils.h"
#include "Lutefisk3D/Engine/PluginApplication.h"
#include "Lutefisk3D/Graphics/Camera.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/RenderPath.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/XMLFile.h"



namespace Urho3D
{

static ResourceRef defaultRenderPath{XMLFile::GetTypeStatic(), "RenderPaths/Forward.xml"};

CameraViewport::CameraViewport(Context* context)
    : Component(context)
    , viewport_(new Viewport(context))
    , rect_(fullScreenViewport)
    , renderPath_(defaultRenderPath)
{
}

void CameraViewport::SetNormalizedRect(const Rect& rect)
{
    rect_ = rect;
    IntVector2 screenRect = GetScreenSize();
    IntRect viewportRect(static_cast<int>(rect.Left() * screenRect.x_), static_cast<int>(rect.Top() * screenRect.y_),
        static_cast<int>(rect.Right() * screenRect.x_), static_cast<int>(rect.Bottom() * screenRect.y_));
    viewport_->SetRect(viewportRect);

    using namespace CameraViewportResized;
    VariantMap args{};
    args[P_VIEWPORT] = GetViewport();
    args[P_CAMERA] = GetViewport()->GetCamera();
    args[P_SIZE] = viewportRect;
    args[P_SIZENORM] = rect;
    SendEvent(E_CAMERAVIEWPORTRESIZED, args);
}

void CameraViewport::RegisterObject(Context* context)
{
    context->RegisterFactory<CameraViewport>("Scene");
}
void CameraViewport::OtherComponentWasAdded(Scene *,Node *n,Component *component)
{
    if (component)
    {
        if (Camera* camera = component->Cast<Camera>())
        {
            viewport_->SetCamera(camera);
            camera->SetViewMask(camera->GetViewMask() & ~(1U << 31));   // Do not render last layer.
        }
    }
}
void CameraViewport::OtherComponentWasRemoved(Scene *,Node *n,Component *component)
{
    if (component)
    {
        if (component->GetType() == Camera::GetTypeStatic())
            viewport_->SetCamera(nullptr);
    }
}
void CameraViewport::OnNodeSet(Node* node)
{
    if (node == nullptr)
        viewport_->SetCamera(nullptr);
    else
    {
        assert(GetScene());
        GetScene()->componentAdded.Connect(this,&CameraViewport::OtherComponentWasAdded);
        GetScene()->componentRemoved.Connect(this,&CameraViewport::OtherComponentWasRemoved);

        if (Camera* camera = node->GetComponent<Camera>())
            viewport_->SetCamera(camera);
    }
}

void CameraViewport::OnSceneSet(Scene* scene)
{
    if (scene)
    {
        if (SceneMetadata* metadata = scene->GetOrCreateComponent<SceneMetadata>())
            metadata->RegisterComponent(this);
    }
    else
    {
        if (Scene* oldScene = GetScene())
        {
            if (SceneMetadata* metadata = oldScene->GetComponent<SceneMetadata>())
                metadata->UnregisterComponent(this);
        }
    }
    viewport_->SetScene(scene);
}

IntVector2 CameraViewport::GetScreenSize() const
{
    const Variant screenSize; // = context_->GetGlobalVar("__GameScreenSize__");
    if (screenSize.IsEmpty())
        return context_->m_Graphics->GetSize();
    return screenSize.GetIntVector2();
}

const std::vector<AttributeInfo> *CameraViewport::GetAttributes() const
{
    if (attributesDirty_)
        const_cast<CameraViewport*>(this)->RebuildAttributes();
    return &attributes_;
}

template<typename T>
AttributeInfo& CameraViewport::RegisterAttribute(const AttributeInfo& attr)
{
    attributes_.push_back(attr);
    return attributes_.back();
}

void CameraViewport::RebuildAttributes()
{
    auto* context = this;
    // Normal attributes.
    URHO3D_ACCESSOR_ATTRIBUTE("Viewport", GetNormalizedRect, SetNormalizedRect, Rect, fullScreenViewport, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("RenderPath", GetLastRenderPath, SetRenderPath, ResourceRef, defaultRenderPath, AM_DEFAULT);

    // PostProcess effects are special. One file may contain multiple effects that can be enabled or disabled.
    {
        effects_.clear();
        for (const auto& dir: context_->resourceCache()->GetResourceDirs())
        {
            QStringList effects;
            QString resourcePath = "PostProcess/";
            QString scanDir = AddTrailingSlash(dir) + resourcePath;
            context_->m_FileSystem->ScanDir(effects, scanDir, "*.xml", SCAN_FILES, false);

            for (const auto& effectFileName : effects)
            {
                auto effectPath = resourcePath + effectFileName;
                auto* effect = context_->resourceCache()->GetResource<XMLFile>(effectPath);

                auto root = effect->GetRoot();
                QString tag;
                for (auto command = root.GetChild("command"); command.NotNull(); command = command.GetNext("command"))
                {
                    tag = command.GetAttribute("tag");

                    if (tag.isEmpty())
                    {
                        URHO3D_LOGWARNING("Invalid PostProcess effect with empty tag");
                        continue;
                    }

                    if (effects_.find(tag) != effects_.end())
                        continue;

                    effects_[tag] = resourcePath + effectFileName;
                }
            }
        }

        QStringList tags;
        tags.reserve(effects_.size());
        for(const auto &entry : effects_)
            tags<<entry.first;
        std::sort(tags.begin(), tags.end());

        for (auto& effect : effects_)
        {
            auto getter = [this, &effect](const CameraViewport&, Variant& value) {
                value = viewport_->GetRenderPath()->IsEnabled(effect.first);
            };

            auto setter = [this, &effect](const CameraViewport&, const Variant& value) {
                RenderPath* path = viewport_->GetRenderPath();
                if (!path->IsAdded(effect.first))
                    path->Append(context_->resourceCache()->GetResource<XMLFile>(effect.second));
                path->SetEnabled(effect.first, value.GetBool());
            };
            URHO3D_CUSTOM_ATTRIBUTE(qPrintable(effect.first), getter, setter, bool, false, AM_DEFAULT);
        }
    }

    attributesDirty_ = false;
}

RenderPath* CameraViewport::RebuildRenderPath()
{
    if (viewport_.Null())
        return nullptr;

    SharedPtr<RenderPath> oldRenderPath(viewport_->GetRenderPath());

    if (XMLFile* renderPathFile = context_->resourceCache()->GetResource<XMLFile>(renderPath_.name_))
    {
        viewport_->SetRenderPath(renderPathFile);
        RenderPath* newRenderPath = viewport_->GetRenderPath();

        for (const auto& effect : effects_)
        {
            if (oldRenderPath->IsEnabled(effect.first))
            {
                if (!newRenderPath->IsAdded(effect.first))
                    newRenderPath->Append(context_->resourceCache()->GetResource<XMLFile>(effect.second));
                newRenderPath->SetEnabled(effect.first, true);
            }
        }

        return newRenderPath;
    }

    return nullptr;
}

void CameraViewport::SetRenderPath(const ResourceRef& renderPathResource)
{
    if (viewport_.Null())
        return;

    if (renderPathResource.type_ != XMLFile::GetTypeStatic())
    {
        URHO3D_LOGWARNING("Incorrect RenderPath file type.");
        return;
    }

    SharedPtr<RenderPath> oldRenderPath(viewport_->GetRenderPath());

    const QString& renderPathFileName = renderPathResource.name_.isEmpty() ? defaultRenderPath.name_ : renderPathResource.name_;
    if (XMLFile* renderPathFile = context_->resourceCache()->GetResource<XMLFile>(renderPathFileName))
    {
        if (!viewport_->SetRenderPath(renderPathFile))
        {
            URHO3D_LOGERROR(QString("Loading renderpath from %s failed. File probably is not a renderpath.")+
                renderPathFileName);
            return;
        }
        RenderPath* newRenderPath = viewport_->GetRenderPath();

        for (const auto& effect : effects_)
        {
            if (oldRenderPath->IsEnabled(effect.first))
            {
                if (!newRenderPath->IsAdded(effect.first))
                    newRenderPath->Append(context_->resourceCache()->GetResource<XMLFile>(effect.second));
                newRenderPath->SetEnabled(effect.first, true);
            }
        }

        renderPath_.name_ = renderPathFileName;
    }
    else
    {
        URHO3D_LOGERROR(QString("Loading renderpath from %1 failed. File is missing or you have no permissions to read it.")
                         .arg(renderPathFileName));
    }
}

}
