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
#include "DebugHud.h"

#include "SystemUI.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Engine/Engine.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/Renderer.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/IO/Log.h"
#ifdef LUTEFISK3D_UI
#include "Lutefisk3D/UI/UI.h"
#endif

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
    "16 bit Simple",
    "24 bit Simple",
    "16 Bit PCF",
    "24 Bit PCF",
    "VSM",
    "Blurred VSM",
};

static const unsigned FPS_UPDATE_INTERVAL_MS = 500;

DebugHud::DebugHud(Context* context) :
    Object(context),
    useRendererStats_(true),
    mode_(DEBUGHUD_SHOW_NONE),
    fps_(0)
{
    SetExtents();
    g_coreSignals.update.Connect(this,&DebugHud::RenderUi);
}

DebugHud::~DebugHud()
{
    UnsubscribeFromAllEvents();
}

void DebugHud::SetExtents(const IntVector2& position, IntVector2 size)
{

    if (size == IntVector2::ZERO)
    {
        size = {context_->m_Graphics->GetWidth(), context_->m_Graphics->GetHeight()};
        if (!g_graphicsSignals.newScreenMode.isConnected(this,&DebugHud::screenModeChanged))
        {
            g_graphicsSignals.newScreenMode.Connect(this,&DebugHud::screenModeChanged);
        }
    }
    else
        g_graphicsSignals.newScreenMode.Disconnect(this);

    auto bottomRight = position + size;
    extents_ = IntRect(position.x_, position.y_, bottomRight.x_, bottomRight.y_);
    RecalculateWindowPositions();
}
void DebugHud::screenModeChanged(int,int,bool,bool,bool,bool,int,int)
{
    SetExtents(IntVector2::ZERO, IntVector2::ZERO);
}
void DebugHud::RecalculateWindowPositions()
{
    posMode_ = WithinExtents({ui::GetStyle().WindowPadding.x, -ui::GetStyle().WindowPadding.y - 10});
    posStats_ = WithinExtents({ui::GetStyle().WindowPadding.x, ui::GetStyle().WindowPadding.y});
}

void DebugHud::SetMode(DebugHudModeFlags mode)
{
    mode_ = mode;
}

void DebugHud::CycleMode()
{
    switch (mode_.AsInteger())
    {
    case DEBUGHUD_SHOW_NONE:
        SetMode(DEBUGHUD_SHOW_STATS);
        break;
    case DEBUGHUD_SHOW_STATS:
        SetMode(DEBUGHUD_SHOW_MODE);
        break;
    case DEBUGHUD_SHOW_MODE:
        SetMode(DEBUGHUD_SHOW_ALL);
        break;
    case DEBUGHUD_SHOW_ALL:
    default:
        SetMode(DEBUGHUD_SHOW_NONE);
        break;
    }
}

void DebugHud::SetUseRendererStats(bool enable)
{
    useRendererStats_ = enable;
}

void DebugHud::Toggle(DebugHudModeFlags mode)
{
    SetMode(GetMode() ^ mode);
}

void DebugHud::ToggleAll()
{
    Toggle(DEBUGHUD_SHOW_ALL);
}

void DebugHud::SetAppStats(const QString& label, const Variant& stats)
{
    SetAppStats(label, stats.ToString());
}

void DebugHud::SetAppStats(const QString& label, const QString& stats)
{
    bool newLabel = !hashContains(appStats_,label);
    appStats_[label] = stats;
//    if (newLabel)
//        appStats_.Sort();
}

bool DebugHud::ResetAppStats(const QString& label)
{
    return appStats_.erase(label);
}

void DebugHud::ClearAppStats()
{
    appStats_.clear();
}

Vector2 DebugHud::WithinExtents(Vector2 pos)
{
    if (pos.x_ < 0)
        pos.x_ += extents_.right_;
    else if (pos.x_ > 0)
        pos.x_ += extents_.left_;
    else
        pos.x_ = extents_.left_;

    if (pos.y_ < 0)
        pos.y_ += extents_.bottom_;
    else if (pos.y_ > 0)
        pos.y_ += extents_.top_;
    else
        pos.y_ = extents_.top_;

    return pos;
}

void DebugHud::RenderUi(float ts)
{
    Renderer* renderer = context_->m_Renderer.get();
    Graphics* graphics = context_->m_Graphics.get();

    ui::SetNextWindowPos({0, 0});
    ui::SetNextWindowSize({(float)extents_.Width(), (float)extents_.Height()});
    ui::PushStyleColor(ImGuiCol_WindowBg, 0);
    if (ui::Begin("DebugHud mode", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar|
                                            ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoInputs))
    {
        if (mode_ & DEBUGHUD_SHOW_MODE)
        {
            ui::SetCursorPos({posMode_.x_, posMode_.y_});
            ui::Text("Tex:%s Mat:%s Spec:%s Shadows:%s Size:%i Quality:%s Occlusion:%s Instancing:%s API:%s",
                     qualityTexts[renderer->GetTextureQuality()],
                     qualityTexts[renderer->GetMaterialQuality()],
                     renderer->GetSpecularLighting() ? "On" : "Off",
                     renderer->GetDrawShadows() ? "On" : "Off",
                     renderer->GetShadowMapSize(),
                     shadowQualityTexts[renderer->GetShadowQuality()],
                     renderer->GetMaxOccluderTriangles() > 0 ? "On" : "Off",
                     renderer->GetDynamicInstancing() ? "On" : "Off",
                     qPrintable(graphics->GetApiName()));
        }

        if (mode_ & DEBUGHUD_SHOW_STATS)
        {
            // Update stats regardless of them being shown.
            if (fpsTimer_.GetMSec(false) > FPS_UPDATE_INTERVAL_MS)
            {
                fps_ = static_cast<unsigned int>(round(context_->m_TimeSystem->GetFramesPerSecond()));
                fpsTimer_.Reset();
            }

            QString stats;
            unsigned primitives, batches;
            if (!useRendererStats_)
            {
                primitives = graphics->GetNumPrimitives();
                batches = graphics->GetNumBatches();
            }
            else
            {
                primitives = context_->m_Renderer->GetNumPrimitives();
                batches = context_->m_Renderer->GetNumBatches();
            }

            ui::SetCursorPos({posStats_.x_, posStats_.y_});
            ui::Text("FPS %d", fps_);
            ui::Text("Triangles %u", primitives);
            ui::Text("Batches %u", batches);
            ui::Text("Views %u", renderer->GetNumViews());
            ui::Text("Lights %u", renderer->GetNumLights(true));
            ui::Text("Shadowmaps %u", renderer->GetNumShadowMaps(true));
            ui::Text("Occluders %u", renderer->GetNumOccluders(true));

            for (auto i = appStats_.begin(); i != appStats_.end(); ++i)
                ui::Text("%s %s", qPrintable(i->first), qPrintable(i->second));
        }
    }
    ui::End();
    ui::PopStyleColor();
}

}
