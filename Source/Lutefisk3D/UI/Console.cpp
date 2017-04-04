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

#include "Console.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/UI/DropDownList.h"
#include "Lutefisk3D/Engine/EngineEvents.h"
#include "Lutefisk3D/UI/Font.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/Input/Input.h"
#include "Lutefisk3D/Input/InputEvents.h"
#include "Lutefisk3D/IO/IOEvents.h"
#include "Lutefisk3D/UI/LineEdit.h"
#include "Lutefisk3D/UI/ListView.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/UI/ScrollBar.h"
#include "Lutefisk3D/UI/Text.h"
#include "Lutefisk3D/UI/UI.h"
#include "Lutefisk3D/UI/UIEvents.h"

namespace Urho3D
{

static const int DEFAULT_CONSOLE_ROWS = 16;
static const int DEFAULT_HISTORY_SIZE = 16;

const char* logStyles[] =
{
    "ConsoleDebugText",
    "ConsoleInfoText",
    "ConsoleWarningText",
    "ConsoleErrorText",
    "ConsoleText"
};
Console::Console(Context* context) :
    Object(context),
    autoVisibleOnError_(false),
    historyRows_(DEFAULT_HISTORY_SIZE),
    historyPosition_(0),
    autoCompletePosition_(0),
    historyOrAutoCompleteChange_(false),
    printing_(false)
{
    UI* ui = GetSubsystem<UI>();
    UIElement* uiRoot = ui->GetRoot();

    // By default prevent the automatic showing of the screen keyboard
    focusOnShow_ = !ui->GetUseScreenKeyboard();

    background_ = uiRoot->CreateChild<BorderImage>();
    background_->SetBringToBack(false);
    background_->SetClipChildren(true);
    background_->SetEnabled(true);
    background_->SetVisible(false); // Hide by default
    background_->SetPriority(200); // Show on top of the debug HUD
    background_->SetBringToBack(false);
    background_->SetLayout(LM_VERTICAL);

    rowContainer_ = background_->CreateChild<ListView>();
    rowContainer_->SetHighlightMode(HM_ALWAYS);
    rowContainer_->SetMultiselect(true);

    commandLine_ = background_->CreateChild<UIElement>();
    commandLine_->SetLayoutMode(LM_HORIZONTAL);
    commandLine_->SetLayoutSpacing(1);
    interpreters_ = commandLine_->CreateChild<DropDownList>();
    lineEdit_ = commandLine_->CreateChild<LineEdit>();
    lineEdit_->SetFocusMode(FM_FOCUSABLE);  // Do not allow defocus with ESC

    closeButton_ = uiRoot->CreateChild<Button>();
    closeButton_->SetVisible(false);
    closeButton_->SetPriority(background_->GetPriority() + 1);  // Show on top of console's background
    closeButton_->SetBringToBack(false);

    SetNumRows(DEFAULT_CONSOLE_ROWS);

    SubscribeToEvent(interpreters_, E_ITEMSELECTED, URHO3D_HANDLER(Console, HandleInterpreterSelected));
    SubscribeToEvent(lineEdit_, E_TEXTCHANGED, URHO3D_HANDLER(Console, HandleTextChanged));
    SubscribeToEvent(lineEdit_, E_TEXTFINISHED, URHO3D_HANDLER(Console, HandleTextFinished));
    SubscribeToEvent(lineEdit_, E_UNHANDLEDKEY, URHO3D_HANDLER(Console, HandleLineEditKey));
    SubscribeToEvent(closeButton_, E_RELEASED, URHO3D_HANDLER(Console, HandleCloseButtonPressed));
    SubscribeToEvent(uiRoot, E_RESIZED, URHO3D_HANDLER(Console, HandleRootElementResized));
    SubscribeToEvent(E_LOGMESSAGE, URHO3D_HANDLER(Console, HandleLogMessage));
    SubscribeToEvent(E_POSTUPDATE, URHO3D_HANDLER(Console, HandlePostUpdate));
}

Console::~Console()
{
    background_->Remove();
    closeButton_->Remove();
}

void Console::SetDefaultStyle(XMLFile* style)
{
    if (!style)
        return;

    background_->SetDefaultStyle(style);
    background_->SetStyle("ConsoleBackground");
    rowContainer_->SetStyleAuto();
    for (unsigned i = 0; i < rowContainer_->GetNumItems(); ++i)
        rowContainer_->GetItem(i)->SetStyle("ConsoleText");
    interpreters_->SetStyleAuto();
    for (unsigned i = 0; i < interpreters_->GetNumItems(); ++i)
        interpreters_->GetItem(i)->SetStyle("ConsoleText");
    lineEdit_->SetStyle("ConsoleLineEdit");

    closeButton_->SetDefaultStyle(style);
    closeButton_->SetStyle("CloseButton");

    UpdateElements();
}

void Console::SetVisible(bool enable)
{
    Input* input = GetSubsystem<Input>();
    UI* ui = GetSubsystem<UI>();
    Cursor* cursor = ui->GetCursor();

    background_->SetVisible(enable);
    closeButton_->SetVisible(enable);
    if (enable)
    {
        // Check if we have receivers for E_CONSOLECOMMAND every time here in case the handler is being added later dynamically
        bool hasInterpreter = PopulateInterpreter();
        commandLine_->SetVisible(hasInterpreter);
        if (hasInterpreter && focusOnShow_)
            ui->SetFocusElement(lineEdit_);

        // Ensure the background has no empty space when shown without the lineedit
        background_->SetHeight(background_->GetMinHeight());

        if (!cursor)
        {
            // Show OS mouse
            input->SetMouseMode(MM_FREE, true);
            input->SetMouseVisible(true, true);
        }

        input->SetMouseGrabbed(false, true);
    }
    else
    {
        rowContainer_->SetFocus(false);
        interpreters_->SetFocus(false);
        lineEdit_->SetFocus(false);

        if (!cursor)
        {
            // Restore OS mouse visibility
            input->ResetMouseMode();
            input->ResetMouseVisible();
        }

        input->ResetMouseGrabbed();
    }
}

void Console::Toggle()
{
    SetVisible(!IsVisible());
}

void Console::SetNumBufferedRows(unsigned rows)
{
    if (rows < displayedRows_)
        return;

    rowContainer_->DisableLayoutUpdate();

    int delta = rowContainer_->GetNumItems() - rows;
    if (delta > 0)
    {
        // We have more, remove oldest rows first
        for (int i = 0; i < delta; ++i)
            rowContainer_->RemoveItem((unsigned)0);
    }
    else
    {
        // We have less, add more rows at the top
        for (int i = 0; i > delta; --i)
        {
            Text* text = new Text(context_);
            // If style is already set, apply here to ensure proper height of the console when
            // amount of rows is changed
            if (background_->GetDefaultStyle())
                text->SetStyle("ConsoleText");
            rowContainer_->InsertItem(0, text);
        }
    }

    rowContainer_->EnsureItemVisibility(rowContainer_->GetItem(rowContainer_->GetNumItems() - 1));
    rowContainer_->EnableLayoutUpdate();
    rowContainer_->UpdateLayout();

    UpdateElements();
}

void Console::SetNumRows(unsigned rows)
{
    if (!rows)
        return;

    displayedRows_ = rows;
    if (GetNumBufferedRows() < rows)
        SetNumBufferedRows(rows);

    UpdateElements();
}

void Console::SetNumHistoryRows(unsigned rows)
{
    historyRows_ = rows;
    if (history_.size() > rows)
        history_.resize(rows);
    if (historyPosition_ > rows)
        historyPosition_ = rows;
}

void Console::SetFocusOnShow(bool enable)
{
    focusOnShow_ = enable;
}

void Console::AddAutoComplete(const QString& option)
{
    // Make sure it isn't a duplicate
    if(autoComplete_.contains(option))
        return ;
    autoComplete_.insert(option,false);
}

void Console::RemoveAutoComplete(const QString& option)
{
    int position = std::distance(autoComplete_.begin(),autoCompletePosition_);
    autoComplete_.remove(option);
    if (position >= autoComplete_.size())
        autoCompletePosition_ = autoComplete_.end();
}

void Console::UpdateElements()
{
    int width = GetSubsystem<UI>()->GetRoot()->GetWidth();
    const IntRect& border = background_->GetLayoutBorder();
    const IntRect& panelBorder = rowContainer_->GetScrollPanel()->GetClipBorder();
    rowContainer_->SetFixedWidth(width - border.left_ - border.right_);
    rowContainer_->SetFixedHeight(displayedRows_ * rowContainer_->GetItem((unsigned)0)->GetHeight() + panelBorder.top_ + panelBorder.bottom_ +
                                  (rowContainer_->GetHorizontalScrollBar()->IsVisible() ? rowContainer_->GetHorizontalScrollBar()->GetHeight() : 0));
    background_->SetFixedWidth(width);
    background_->SetHeight(background_->GetMinHeight());
}

XMLFile* Console::GetDefaultStyle() const
{
    return background_->GetDefaultStyle(false);
}

bool Console::IsVisible() const
{
    return background_ && background_->IsVisible();
}

unsigned Console::GetNumBufferedRows() const
{
    return rowContainer_->GetNumItems();
}

void Console::CopySelectedRows() const
{
    rowContainer_->CopySelectedItemsToClipboard();
}

const QString& Console::GetHistoryRow(unsigned index) const
{
    return index < history_.size() ? history_[index] : s_dummy;
}

bool Console::PopulateInterpreter()
{
    interpreters_->RemoveAllItems();

    EventReceiverGroup * group = context_->GetEventReceivers(E_CONSOLECOMMAND);
    if (!group || group->receivers_.empty())
        return false;

    QStringList names;
    for (unsigned i = 0; i < group->receivers_.size(); ++i)
    {
        Object* receiver = group->receivers_[i];
        if (receiver)
            names.push_back(receiver->GetTypeName());
    }
    std::sort(names.begin(), names.end());

    unsigned selection = M_MAX_UNSIGNED;
    for (unsigned i = 0; i < names.size(); ++i)
    {
        const QString& name = names[i];
        if (name == commandInterpreter_)
            selection = i;
        Text* text = new Text(context_);
        text->SetStyle("ConsoleText");
        text->SetText(name);
        interpreters_->AddItem(text);
    }

    const IntRect& border = interpreters_->GetPopup()->GetLayoutBorder();
    interpreters_->SetMaxWidth(interpreters_->GetListView()->GetContentElement()->GetWidth() + border.left_ + border.right_);
    bool enabled = interpreters_->GetNumItems() > 1;
    interpreters_->SetEnabled(enabled);
    interpreters_->SetFocusMode(enabled ? FM_FOCUSABLE_DEFOCUSABLE : FM_NOTFOCUSABLE);

    if (selection == M_MAX_UNSIGNED)
    {
        selection = 0;
        commandInterpreter_ = names[selection];
    }
    interpreters_->SetSelection(selection);

    return true;
}

void Console::HandleInterpreterSelected(StringHash eventType, VariantMap& eventData)
{
    commandInterpreter_ = static_cast<Text*>(interpreters_->GetSelectedItem())->GetText();
    lineEdit_->SetFocus(true);
}

void Console::HandleTextChanged(StringHash eventType, VariantMap & eventData)
{
    // Save the original line
    // Make sure the change isn't caused by auto complete or history
    if (!historyOrAutoCompleteChange_)
        autoCompleteLine_ = eventData[TextEntry::P_TEXT].GetString();

    historyOrAutoCompleteChange_ = false;
}
void Console::HandleTextFinished(StringHash eventType, VariantMap& eventData)
{
    using namespace TextFinished;

    QString line = lineEdit_->GetText();
    if (!line.isEmpty())
    {
        // Send the command as an event for script subsystem
        using namespace ConsoleCommand;
        SendEvent(E_CONSOLECOMMAND, P_COMMAND, line, P_ID, static_cast<Text*>(interpreters_->GetSelectedItem())->GetText());

        // Make sure the line isn't the same as the last one
        if (history_.empty() || line != history_.back())
        {
            // Store to history, then clear the lineedit
            history_.push_back(line);
            if (history_.size() > historyRows_)
                history_.erase(history_.begin());
        }

        historyPosition_ = history_.size(); // Reset
        autoCompletePosition_ = autoComplete_.end(); // Reset

        currentRow_.clear();
        lineEdit_->SetText(currentRow_);
    }
}

void Console::HandleLineEditKey(StringHash eventType, VariantMap& eventData)
{
    if (!historyRows_)
        return;

    using namespace UnhandledKey;

    bool changed = false;

    switch (eventData[P_KEY].GetInt())
    {
        case KEY_UP:
            if (autoCompletePosition_ == autoComplete_.begin())
                autoCompletePosition_ = autoComplete_.end();

            if (autoCompletePosition_ != autoComplete_.end())
            {
                // Search for auto completion that contains the contents of the line
                while(autoCompletePosition_ != autoComplete_.end())
                {
                    --autoCompletePosition_;
                    const QString& current = autoCompletePosition_.key();
                    if (current.startsWith(autoCompleteLine_))
                    {
                        historyOrAutoCompleteChange_ = true;
                        lineEdit_->SetText(current);
                        break;
                    }
                }

                // If not found
                if (autoCompletePosition_ == autoComplete_.end())
                {
                    // Reset the position
                    autoCompletePosition_ = autoComplete_.begin();
                    // Reset history position
                    historyPosition_ = history_.size();
                }
            }

            // If no more auto complete options and history options left
            if (autoCompletePosition_ == autoComplete_.end() && historyPosition_ > 0)
            {
                // If line text is not a history, save the current text value to be restored later
                if (historyPosition_ == history_.size())
                    currentRow_ = lineEdit_->GetText();
                // Use the previous option
                --historyPosition_;
                changed = true;
            }
            break;

        case KEY_DOWN:
            // If history options left
            if (historyPosition_ < history_.size())
            {
                // Use the next option
                ++historyPosition_;
                changed = true;
            }
            else
            {
                // Loop over
                if (autoCompletePosition_ == autoComplete_.end())
                    autoCompletePosition_ = autoComplete_.begin();
                else
                    ++autoCompletePosition_; // If not starting over, skip checking the currently found completion

                auto startPosition = autoCompletePosition_;

                // Search for auto completion that contains the contents of the line
                for (; autoCompletePosition_ != autoComplete_.end(); ++autoCompletePosition_)
                {
                    const QString& current = autoCompletePosition_.key();
                    if (current.startsWith(autoCompleteLine_))
                    {
                        historyOrAutoCompleteChange_ = true;
                        lineEdit_->SetText(current);
                        break;
                    }
                }

                // Continue to search the complete range
                if (autoCompletePosition_ == autoComplete_.end())
                {
                    for (autoCompletePosition_ = 0; autoCompletePosition_ != startPosition; ++autoCompletePosition_)
                    {
                        const QString& current = autoCompletePosition_.key();
                        if (current.startsWith(autoCompleteLine_))
                        {
                            historyOrAutoCompleteChange_ = true;
                            lineEdit_->SetText(current);
                            break;
                        }
                    }
                }
            }
            break;
        default: break;
    }

    if (changed)
    {
        historyOrAutoCompleteChange_ = true;
        // Set text to history option
        if (historyPosition_ < history_.size())
            lineEdit_->SetText(history_[historyPosition_]);
        else // restore the original line value before it was set to history values
        {
            lineEdit_->SetText(currentRow_);
            // Set the auto complete position according to the currentRow
            for (autoCompletePosition_ = autoComplete_.begin(); autoCompletePosition_ != autoComplete_.end(); ++autoCompletePosition_)
                if (autoCompletePosition_.key().startsWith(currentRow_))
                    break;
        }
    }
}

void Console::HandleCloseButtonPressed(StringHash eventType, VariantMap& eventData)
{
    SetVisible(false);
}

void Console::HandleRootElementResized(StringHash eventType, VariantMap& eventData)
{
    UpdateElements();
}

void Console::HandleLogMessage(StringHash eventType, VariantMap& eventData)
{
    // If printing a log message causes more messages to be logged (error accessing font), disregard them
    if (printing_)
        return;

    using namespace LogMessage;

    int level = eventData[P_LEVEL].GetInt();
    // The message may be multi-line, so split to rows in that case
    QStringList rows = eventData[P_MESSAGE].GetString().split('\n');

    for (QString & row : rows)
        pendingRows_.emplace_back(level, row);

    if (autoVisibleOnError_ && level == LOG_ERROR && !IsVisible())
        SetVisible(true);
}

void Console::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
{
    // Ensure UI-elements are not detached
    if (!background_->GetParent())
    {
        UI* ui = GetSubsystem<UI>();
        UIElement* uiRoot = ui->GetRoot();
        uiRoot->AddChild(background_);
        uiRoot->AddChild(closeButton_);
    }

    if (!rowContainer_->GetNumItems() || pendingRows_.empty())
        return;

    printing_ = true;
    rowContainer_->DisableLayoutUpdate();

    Text* text = nullptr;
    for (auto & elem : pendingRows_)
    {
        rowContainer_->RemoveItem((unsigned)0);
        text = new Text(context_);
        text->SetText(ELEMENT_VALUE(elem));
        // Highlight console messages based on their type
        text->SetStyle(logStyles[ELEMENT_KEY(elem)]);
        rowContainer_->AddItem(text);
    }

    pendingRows_.clear();

    rowContainer_->EnsureItemVisibility(text);
    rowContainer_->EnableLayoutUpdate();
    rowContainer_->UpdateLayout();
    UpdateElements();   // May need to readjust the height due to scrollbar visibility changes
    printing_ = false;
}

}
