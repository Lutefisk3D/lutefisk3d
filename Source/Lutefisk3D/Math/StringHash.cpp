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

#include "StringHash.h"

#include "MathDefs.h"

#include <QtCore/QString>
#include <cstdio>

namespace Urho3D
{

const StringHash StringHash::ZERO;

StringHash::StringHash(const char* str) :
    value_(Calculate(str))
{
}

StringHash::StringHash(const QString& str) :
    value_(Calculate(qPrintable(str)))
{
}
StringHash::StringHash(const QLatin1String& str) :
    value_(Calculate(str.data()))
{
}

StringHash::StringHash(const QStringRef& str)
{
    value_ = 0;

    if (str.string())
    {
        for(QChar c : str)
        {
            // Perform the actual hashing as case-insensitive
            value_ = SDBMHash(value_, c.toLower().toLatin1());
        }
    }
}
unsigned StringHash::Calculate(const char* str)
{
    unsigned hash = 0;

    if (!str)
        return hash;

    while (*str)
    {
        // Perform the actual hashing as case-insensitive
        char c = *str;
        hash = SDBMHash(hash, tolower(c));
        ++str;
    }

    return hash;
}

QString StringHash::ToString() const
{
    return QString("%1").arg(value_,8,16,QChar('0'));
}

}
