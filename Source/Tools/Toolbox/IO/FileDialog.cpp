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

#include <QtWidgets/QFileDialog>
#include "FileDialog.h"


namespace Urho3D
{

//FileDialogResult OpenDialog(const QString& filterList, const QString& defaultPath, QString& outPath)
//{
//    QFileDialog::getOpenFileName(nullptr,"Open File",)
//    nfdchar_t* output = nullptr;
//    auto result = NFD_OpenDialog(qPrintable(filterList), qPrintable(defaultPath), &output);
//    if (output != nullptr)
//    {
//        outPath = output;
//        NFD_FreePath(output);
//    }
//    return static_cast<FileDialogResult>(result);
//}

//FileDialogResult OpenDialogMultiple(const QString& filterList, const QString& defaultPath, QStringList& outPaths)
//{
//    nfdpathset_t output{};
//    auto result = NFD_OpenDialogMultiple(qPrintable(filterList), qPrintable(defaultPath), &output);
//    if (result == NFD_OKAY)
//    {
//        for (size_t i = 0, end = NFD_PathSet_GetCount(&output); i < end; i++)
//            outPaths.push_back(NFD_PathSet_GetPath(&output, i));
//        NFD_PathSet_Free(&output);
//    }
//    return static_cast<FileDialogResult>(result);
//}

//FileDialogResult SaveDialog(const QString& filterList, const QString& defaultPath, QString& outPath)
//{
//    nfdchar_t* output = nullptr;
//    auto result = NFD_SaveDialog(qPrintable(filterList), qPrintable(defaultPath), &output);
//    if (output != nullptr)
//    {
//        outPath = output;
//        NFD_FreePath(output);
//    }
//    return static_cast<FileDialogResult>(result);
//}

//FileDialogResult PickFolder(const QString& defaultPath, QString& outPath)
//{
//    nfdchar_t* output = nullptr;
//    auto result = NFD_PickFolder(qPrintable(defaultPath), &output);
//    if (output != nullptr)
//    {
//        outPath = output;
//        NFD_FreePath(output);
//    }
//    return static_cast<FileDialogResult>(result);
//}

}
