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

#pragma once

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Container/FlagSet.h"
#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"

#include <QtCore/QStringList>

namespace Urho3D
{

class Button;
class DropDownList;
class Font;
class LineEdit;
class ListView;
class ResourceCache;
class Text;
class UIElement;
class Window;
class XMLFile;

/// %File selector's list entry (file or directory.)
struct FileSelectorEntry
{
    /// Name.
    QString name_;
    /// Directory flag.
    bool directory_;
};

/// %File selector dialog.
class LUTEFISK3D_EXPORT FileSelector : public Object
{
    URHO3D_OBJECT(FileSelector,Object)

public:
    /// Construct.
    explicit FileSelector(Context* context);
    /// Destruct.
    ~FileSelector() override;
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Set fileselector UI style.
    void SetDefaultStyle(XMLFile* style);
    /// Set title text.
    void SetTitle(const QString& text);
    /// Set button texts.
    void SetButtonTexts(const QString& okText, const QString& cancelText);
    /// Set current path.
    void SetPath(const QString& path);
    /// Set current filename.
    void SetFileName(const QString& fileName);
    /// Set filters.
    void SetFilters(const QStringList& filters, unsigned defaultIndex);
    /// Set directory selection mode. Default false.
    void SetDirectoryMode(bool enable);
    /// Update elements to layout properly. Call this after manually adjusting the sub-elements.
    void UpdateElements();

    /// Return the UI style file.
    XMLFile* GetDefaultStyle() const;
    /// Return fileselector window.
    Window* GetWindow() const { return window_; }
    /// Return window title text element.
    Text* GetTitleText() const { return titleText_; }
    /// Return file list.
    ListView* GetFileList() const { return fileList_; }
    /// Return path editor.
    LineEdit* GetPathEdit() const { return pathEdit_; }
    /// Return filename editor.
    LineEdit* GetFileNameEdit() const { return fileNameEdit_; }
    /// Return filter dropdown.
    DropDownList* GetFilterList() const { return filterList_; }
    /// Return OK button.
    Button* GetOKButton() const { return okButton_; }
    /// Return cancel button.
    Button* GetCancelButton() const { return cancelButton_; }
    /// Return close button.
    Button* GetCloseButton() const { return closeButton_; }
    /// Return window title.
    const QString& GetTitle() const;
    /// Return current path.
    const QString& GetPath() const { return path_; }
    /// Return current filename.
    const QString& GetFileName() const;
    /// Return current filter.
    const QString& GetFilter() const;
    /// Return current filter index.
    unsigned GetFilterIndex() const;
    /// Return directory mode flag.
    bool GetDirectoryMode() const { return directoryMode_; }

private:
    /// Set the text of an edit field and ignore the resulting event.
    void SetLineEditText(LineEdit* edit, const QString& text);
    /// Refresh the directory listing.
    void RefreshFiles();
    /// Enter a directory or confirm a file. Return true if a directory entered.
    bool EnterFile();
    void HandleFileNameFinished(UIElement *, const QString &, float);
    void HandleFileAccepted(bool byButton);
    void HandleModalChanged(UIElement *e, bool modal);
    /// Handle filter changed.
    void HandleFilterChanged(UIElement *el, int sel);
    /// Handle path edited.
    void HandlePathChanged(UIElement *, const QString &, float);
    /// Handle file selected from the list.
    void HandleFileSelected(UIElement *el, int sel);
    /// Handle file doubleclicked from the list (enter directory / OK the file selection.)
    void HandleFileDoubleClicked(UIElement *, UIElement *, int, int, unsigned, unsigned);
    /// Handle file list key pressed.
    void HandleFileListKey(UIElement *, int, unsigned, unsigned);
    /// Handle OK button pressed.
    void HandleOKPressed(UIElement *);
    /// Handle cancel button pressed.
    void HandleCancelPressed(UIElement *);

    /// Fileselector window.
    SharedPtr<Window> window_;
    /// Title layout.
    UIElement* titleLayout;
    /// Window title text.
    Text* titleText_;
    /// File list.
    ListView* fileList_;
    /// Path editor.
    LineEdit* pathEdit_;
    /// Filename editor.
    LineEdit* fileNameEdit_;
    /// Filter dropdown.
    DropDownList* filterList_;
    /// OK button.
    Button* okButton_;
    /// OK button text.
    Text* okButtonText_;
    /// Cancel button.
    Button* cancelButton_;
    /// Cancel button text.
    Text* cancelButtonText_;
    /// Close button.
    Button* closeButton_;
    /// Filename and filter layout.
    UIElement* fileNameLayout_;
    /// Separator layout.
    UIElement* separatorLayout_;
    /// Button layout.
    UIElement* buttonLayout_;
    /// Current directory.
    QString path_;
    /// Filters.
    QStringList filters_;
    /// File entries.
    std::vector<FileSelectorEntry> fileEntries_;
    /// Filter used to get the file list.
    QString lastUsedFilter_;
    /// Directory mode flag.
    bool directoryMode_;
    /// Ignore events flag, used when changing line edits manually.
    bool ignoreEvents_;
};

}
