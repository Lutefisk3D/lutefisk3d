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

#include "DebugHud.h"

#include "../Core/CoreEvents.h"
#include "../Engine/Engine.h"
#include "../UI/Font.h"
#include "../Graphics/Graphics.h"
#include "../IO/Log.h"
#include "../Core/Profiler.h"
#include "../Graphics/Renderer.h"
#include "../UI/Text.h"
#include "../UI/UI.h"
#include "../Resource/ResourceCache.h"
namespace Urho3D
{

static const char* qualityTexts[] =
{
    "Low",
    "Med",
    "High"
};

static const char* shadowQualityTexts[] =
{
    "16bit Simple",
    "24bit Simple",
    "16bit PCF",
    "24bit PCF",
    "VSM",
    "Blurred VSM"
};

DebugHud::DebugHud(Context* context) :
    Object(context),
    profilerMaxDepth_(M_MAX_UNSIGNED),
    profilerInterval_(1000),
    useRendererStats_(false),
    mode_(DEBUGHUD_SHOW_NONE)
{
    UI* ui = GetSubsystem<UI>();
    UIElement* uiRoot = ui->GetRoot();

    statsText_ = new Text(context_);
    statsText_->SetAlignment(HA_LEFT, VA_TOP);
    statsText_->SetPriority(100);
    statsText_->SetVisible(false);
    uiRoot->AddChild(statsText_);

    modeText_ = new Text(context_);
    modeText_->SetAlignment(HA_LEFT, VA_BOTTOM);
    modeText_->SetPriority(100);
    modeText_->SetVisible(false);
    uiRoot->AddChild(modeText_);

    profilerText_ = new Text(context_);
    profilerText_->SetAlignment(HA_RIGHT, VA_TOP);
    profilerText_->SetPriority(100);
    profilerText_->SetVisible(false);
    uiRoot->AddChild(profilerText_);
    memoryText_ = new Text(context_);
    memoryText_->SetAlignment(HA_RIGHT, VA_TOP);
    memoryText_->SetPriority(100);
    memoryText_->SetVisible(false);
    uiRoot->AddChild(memoryText_);

    SubscribeToEvent(E_POSTUPDATE, URHO3D_HANDLER(DebugHud, HandlePostUpdate));
}

DebugHud::~DebugHud()
{
    statsText_->Remove();
    modeText_->Remove();
    profilerText_->Remove();
    memoryText_->Remove();
}

void DebugHud::Update()
{
    Graphics* graphics = GetSubsystem<Graphics>();
    Renderer* renderer = GetSubsystem<Renderer>();
    if (!renderer || !graphics)
        return;

    // Ensure UI-elements are not detached
    if (!statsText_->GetParent())
    {
        UI* ui = GetSubsystem<UI>();
        UIElement* uiRoot = ui->GetRoot();
        uiRoot->AddChild(statsText_);
        uiRoot->AddChild(modeText_);
        uiRoot->AddChild(profilerText_);
    }

    if (statsText_->IsVisible())
    {
        unsigned primitives, batches;
        if (!useRendererStats_)
        {
            primitives = graphics->GetNumPrimitives();
            batches = graphics->GetNumBatches();
        }
        else
        {
            primitives = renderer->GetNumPrimitives();
            batches = renderer->GetNumBatches();
        }

        QString stats = QString("Triangles %1\nBatches %2\nViews %3\nLights %4\nShadowmaps %5\nOccluders %6")
            .arg(primitives)
            .arg(batches)
            .arg(renderer->GetNumViews())
            .arg(renderer->GetNumLights(true))
            .arg(renderer->GetNumShadowMaps(true))
            .arg(renderer->GetNumOccluders(true));

        if (!appStats_.isEmpty())
        {
            stats.append("\n");
            for (QMap<QString, QString>::const_iterator i = appStats_.cbegin(); i != appStats_.cend(); ++i)
                stats += QString("\n%1 %2").arg(i.key()).arg(*i);
        }

        statsText_->SetText(stats);
    }

    if (modeText_->IsVisible())
    {
        QString mode = QString("Tex:%1 Mat:%2 Spec:%3 Shadows:%4 Size:%5 Quality:%6 Occlusion:%7 Instancing:%8 API:%9")
            .arg(qualityTexts[renderer->GetTextureQuality()])
            .arg(qualityTexts[renderer->GetMaterialQuality()])
            .arg(renderer->GetSpecularLighting() ? "On" : "Off")
            .arg(renderer->GetDrawShadows() ? "On" : "Off")
            .arg(renderer->GetShadowMapSize())
            .arg(shadowQualityTexts[renderer->GetShadowQuality()])
            .arg(renderer->GetMaxOccluderTriangles() > 0 ? "On" : "Off")
            .arg(renderer->GetDynamicInstancing() ? "On" : "Off")
            .arg(graphics->GetApiName());

        modeText_->SetText(mode);
    }

    Profiler* profiler = GetSubsystem<Profiler>();
    if (profiler)
    {
        if (profilerTimer_.GetMSec(false) >= profilerInterval_)
        {
            profilerTimer_.Reset();

            if (profilerText_->IsVisible())
            {
                QString profilerOutput = profiler->PrintData(false, false, profilerMaxDepth_);
                profilerText_->SetText(profilerOutput);
            }

            profiler->BeginInterval();
        }
    }
    if (memoryText_->IsVisible())
    {
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        memoryText_->SetText(cache->PrintMemoryUsage());
    }
}

void DebugHud::SetDefaultStyle(XMLFile* style)
{
    if (!style)
        return;

    statsText_->SetDefaultStyle(style);
    statsText_->SetStyle("DebugHudText");
    modeText_->SetDefaultStyle(style);
    modeText_->SetStyle("DebugHudText");
    profilerText_->SetDefaultStyle(style);
    profilerText_->SetStyle("DebugHudText");
    memoryText_->SetDefaultStyle(style);
    memoryText_->SetStyle("DebugHudText");
}

void DebugHud::SetMode(unsigned mode)
{
    statsText_->SetVisible((mode & DEBUGHUD_SHOW_STATS) != 0);
    modeText_->SetVisible((mode & DEBUGHUD_SHOW_MODE) != 0);
    profilerText_->SetVisible((mode & DEBUGHUD_SHOW_PROFILER) != 0);
    memoryText_->SetVisible((mode & DEBUGHUD_SHOW_MEMORY) != 0);

    mode_ = mode;
}

void DebugHud::SetProfilerMaxDepth(unsigned depth)
{
    profilerMaxDepth_ = depth;
}

void DebugHud::SetProfilerInterval(float interval)
{
    profilerInterval_ = Max((int)(interval * 1000.0f), 0);
}

void DebugHud::SetUseRendererStats(bool enable)
{
    useRendererStats_ = enable;
}

void DebugHud::Toggle(unsigned mode)
{
    SetMode(GetMode() ^ mode);
}

void DebugHud::ToggleAll()
{
    Toggle(DEBUGHUD_SHOW_ALL);
}

XMLFile* DebugHud::GetDefaultStyle() const
{
    return statsText_->GetDefaultStyle(false);
}

float DebugHud::GetProfilerInterval() const
{
    return (float)profilerInterval_ / 1000.0f;
}

void DebugHud::SetAppStats(const QString& label, const Variant& stats)
{
    SetAppStats(label, stats.ToString());
}

void DebugHud::SetAppStats(const QString& label, const QString& stats)
{
    //bool newLabel = !appStats_.contains(label);
    appStats_[label] = stats;
    //if (newLabel)
    //   appStats_.Sort();
}

bool DebugHud::ResetAppStats(const QString& label)
{
    return appStats_.remove(label);
}

void DebugHud::ClearAppStats()
{
    appStats_.clear();
}

void DebugHud::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace PostUpdate;

    Update();
}

}
