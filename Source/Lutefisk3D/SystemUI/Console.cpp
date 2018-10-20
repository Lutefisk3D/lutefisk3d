//
// Copyright (c) 2017 the Urho3D project.
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Engine/EngineEvents.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/Input/Input.h"
#include "Lutefisk3D/IO/IOEvents.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "SystemUI.h"
#include "SystemUIEvents.h"
#include "Console.h"

namespace Urho3D
{

static const int DEFAULT_HISTORY_SIZE = 512;

Console::Console(Context* context) :
    Object(context),
    autoVisibleOnError_(false),
    historyRows_(DEFAULT_HISTORY_SIZE),
    isOpen_(false),
    windowSize_(M_MAX_INT, 200),     // Width gets clamped by HandleScreenMode()
    currentInterpreter_(0)
{
    inputBuffer_[0] = 0;

    SetNumHistoryRows(DEFAULT_HISTORY_SIZE);
    HandleScreenMode(0,0,false,false,false,false,0,0);
    RefreshInterpreters();

    g_graphicsSignals.newScreenMode.Connect(this,&Console::HandleScreenMode);
    g_LogSignals.logMessageSignal.Connect(this,&Console::HandleLogMessage);
}

Console::~Console()
{
    UnsubscribeFromAllEvents();
}

void Console::SetVisible(bool enable)
{
    isOpen_ = enable;
    if (isOpen_)
    {
        focusInput_ = true;
        g_coreSignals.update.Connect(this,&Console::RenderUi);
    }
    else
    {
        g_coreSignals.update.Disconnect(this);
        ui::SetWindowFocus(nullptr);
    }
}

void Console::Toggle()
{
    SetVisible(!IsVisible());
}

void Console::SetNumHistoryRows(unsigned rows)
{
    historyRows_ = rows;
    if (history_.size() > rows)
        history_.resize(rows);
}

bool Console::IsVisible() const
{
    return isOpen_;
}

void Console::RefreshInterpreters()
{
    interpreters_.clear();
    interpretersPointers_.clear();

    std::string currentInterpreterName;
    g_consoleSignals.consoleCommand.OnAllObservers([this,&currentInterpreterName](SignalObserver *receiver)->bool {
        if (currentInterpreter_ < interpreters_.size())
            currentInterpreterName = interpreters_[currentInterpreter_];
        if (receiver)
        {
            interpreters_.push_back(static_cast<Object *>(receiver)->GetTypeName().toStdString());
            interpretersPointers_.push_back(interpreters_.back().c_str());
        }
        return false;
    });
    std::sort(interpreters_.begin(), interpreters_.end());
    auto loc = std::find(interpreters_.begin(), interpreters_.end(),currentInterpreterName);
    if(loc!=interpreters_.end())
        currentInterpreter_ = loc-interpreters_.begin();
    if (currentInterpreter_ == interpreters_.size())
        currentInterpreter_ = 0;
}

void Console::HandleLogMessage(LogLevels level,const QString &msg)
{
    // The message may be multi-line, so split to rows in that case
    QStringList rows = msg.split('\n');
    for (const auto& row : rows)
        history_.push_back(std::pair<int, QString>(level, row));
    scrollToEnd_ = true;

    if (autoVisibleOnError_ && level == LOG_ERROR && !IsVisible())
        SetVisible(true);
}

void Console::RenderContent()
{
    auto region = ui::GetContentRegionAvail();
    auto showCommandInput = !interpretersPointers_.empty();
    ui::BeginChild("ConsoleScrollArea", ImVec2(region.x, region.y - (showCommandInput ? 30 : 0)), false,
                   ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& row : history_)
    {
        ImColor color;
        switch (row.first)
        {
        case LOG_ERROR:
            color = ImColor(247, 168, 168);
            break;
        case LOG_WARNING:
            color = ImColor(247, 247, 168);
            break;
        case LOG_DEBUG:
            color = ImColor(200, 200, 200);
            break;
        case LOG_INFO:
        default:
            color = IM_COL32_WHITE;
            break;
        }
        ui::TextColored(color, "%s", qPrintable(row.second));
    }

    if (scrollToEnd_)
    {
        ui::SetScrollHere();
        scrollToEnd_ = false;
    }

    ui::EndChild();

    if (showCommandInput)
    {
        ui::PushItemWidth(110);
        if (ui::Combo("##ConsoleInterpreter", &currentInterpreter_, &interpretersPointers_.front(),
                      interpretersPointers_.size()))
        {

        }
        ui::PopItemWidth();
        ui::SameLine();
        ui::PushItemWidth(region.x - 120);
        if (focusInput_)
        {
            ui::SetKeyboardFocusHere();
            focusInput_ = false;
        }
        if (ui::InputText("##ConsoleInput", inputBuffer_, sizeof(inputBuffer_), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            focusInput_ = true;
            QString line(inputBuffer_);
            if (line.length() && currentInterpreter_ < interpreters_.size())
            {
                // Store to history, then clear the lineedit
                URHO3D_LOGINFO(QString::asprintf("> %s", qPrintable(line)));
                if (history_.size() > historyRows_)
                    history_.erase(history_.begin());
                scrollToEnd_ = true;
                inputBuffer_[0] = 0;

                // Send the command as an event for script subsystem
                g_consoleSignals.consoleCommand(line,QString::fromStdString(interpreters_[currentInterpreter_]));
            }
        }
        ui::PopItemWidth();
    }
}

void Console::RenderUi(float dt)
{
    Graphics* graphics = context_->m_Graphics.get();
    ui::SetNextWindowPos(ImVec2(0, 0));
    bool wasOpen = isOpen_;
    ImVec2 size(graphics->GetWidth(), windowSize_.y_);
    ui::SetNextWindowSize(size);

    auto old_rounding = ui::GetStyle().WindowRounding;
    ui::GetStyle().WindowRounding = 0;
    if (ui::Begin("Debug Console", &isOpen_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoSavedSettings))
    {
        RenderContent();
    }
    else if (wasOpen)
    {
        SetVisible(false);
        ui::SetWindowFocus(nullptr);
        SendEvent(E_CONSOLECLOSED);
    }

    windowSize_.y_ = ui::GetWindowHeight();

    ui::End();

    ui::GetStyle().WindowRounding = old_rounding;
}

void Console::Clear()
{
    history_.clear();
}

void Console::SetCommandInterpreter(const QString& interpreter)
{
    RefreshInterpreters();
    int index=0;
    auto iter = std::find(interpreters_.begin(),interpreters_.end(),interpreter.toStdString());
    if (iter != interpreters_.end())
        index = iter-interpreters_.begin();

    currentInterpreter_ = index;
}

void Console::HandleScreenMode(int,int,bool,bool,bool,bool,int,int)
{
    Graphics* graphics = context_->m_Graphics.get();
    windowSize_.x_ = Clamp(windowSize_.x_, 0, graphics->GetWidth());
    windowSize_.y_ = Clamp(windowSize_.y_, 0, graphics->GetHeight());
}

}
