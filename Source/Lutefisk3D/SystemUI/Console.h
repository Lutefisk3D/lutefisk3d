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

#pragma once

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Math/Vector2.h"


namespace Urho3D
{
enum LogLevels : int32_t;
/// %Console window with log history and command line prompt.
class LUTEFISK3D_EXPORT Console : public Object
{
    URHO3D_OBJECT(Console, Object)

public:
    Console(Context* context);
    ~Console() override;

    /// Show or hide.
    void SetVisible(bool enable);
    /// Toggle visibility.
    void Toggle();

    /// Automatically set console to visible when receiving an error log message.
    void SetAutoVisibleOnError(bool enable) { autoVisibleOnError_ = enable; }

    /// Set the command interpreter.
    void SetCommandInterpreter(const QString& interpreter);

    /// Set command history maximum size, 0 disables history.
    void SetNumHistoryRows(unsigned rows);

    /// Return whether is visible.
    bool IsVisible() const;

    /// Return true when console is set to automatically visible when receiving an error log message.
    bool IsAutoVisibleOnError() const { return autoVisibleOnError_; }

    /// Return the last used command interpreter.
    const std::string& GetCommandInterpreter() const { return interpreters_[currentInterpreter_]; }

    /// Return history maximum size.
    unsigned GetNumHistoryRows() const { return historyRows_; }

    /// Remove all rows.
    void Clear();
    /// Render contents of the console window. Useful for embedding console into custom UI.
    void RenderContent();
    /// Populate the command line interpreters that could handle the console command.
    void RefreshInterpreters();

private:
    /// Update console size on application window changes.
    void HandleScreenMode(int, int, bool, bool, bool, bool, int, int);
    /// Handle a log message.
    void HandleLogMessage(LogLevels, const QString &);
    /// Render system ui.
    void RenderUi(float dt);

    /// Auto visible on error flag.
    bool autoVisibleOnError_;
    /// List of command interpreters.
    std::vector<std::string> interpreters_;
    /// References to strings in interpreters_ list for efficient UI rendering.
    std::vector<const char *> interpretersPointers_;
    /// Last used command interpreter.
    int currentInterpreter_;
    /// Command history.
    std::vector<std::pair<int, QString>> history_;
    /// Command history maximum rows.
    unsigned historyRows_;
    /// Is console window open.
    bool isOpen_;
    /// Input box buffer.
    char inputBuffer_[0x1000]{};
    IntVector2 windowSize_;
    bool scrollToEnd_ = false;
    bool focusInput_ = false;
};

}
