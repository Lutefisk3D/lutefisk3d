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
#ifdef LUTEFISK3D_HASH_DEBUG
#include "Lutefisk3D/Core/StringHashRegister.h"
#endif
namespace Urho3D
{
#ifdef LUTEFISK3D_HASH_DEBUG

// Expose map to let Visual Studio debugger access it if Urho3D is linked statically.
const StringMap* hashReverseMap = nullptr;

// Hide static global variables in functions to ensure initialization order.
static StringHashRegister& GetGlobalStringHashRegister()
{
    static StringHashRegister stringHashRegister(true /*thread safe*/ );
    hashReverseMap = &stringHashRegister.GetInternalMap();
    return stringHashRegister;
}

#endif

const StringHash StringHash::ZERO;

#ifdef LUTEFISK3D_HASH_DEBUG
unsigned StringHash::Calculate(const char* str, unsigned hash)
{
    if (!str)
        return hash;

    while (*str)
        hash = SDBMHash(hash, (char)tolower(*str++));

    return hash;
}
#endif
StringHash::StringHash(const QString& str) :
    value_(Calculate(qPrintable(str)))
{
#ifdef LUTEFISK3D_HASH_DEBUG
    Urho3D::GetGlobalStringHashRegister().RegisterString(*this, qPrintable(str));
#endif
}
StringHash::StringHash(const QLatin1String& str) :
    value_(Calculate(str.data()))
{
#ifdef LUTEFISK3D_HASH_DEBUG
    Urho3D::GetGlobalStringHashRegister().RegisterString(*this, qPrintable(str));
#endif
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

unsigned int StringHash::Calculate(void* data, unsigned int length, unsigned int hash)
{
    if (!data)
        return hash;

    auto* bytes = static_cast<unsigned char*>(data);
    auto* end = bytes + length;
    while (bytes < end)
    {
        hash = SDBMHash(hash, *bytes);
        ++bytes;
    }

    return hash;
}
/// Construct from a C string case-insensitively.
#ifdef LUTEFISK3D_HASH_DEBUG
StringHash::StringHash(const char* str) noexcept :      // NOLINT(google-explicit-constructor)
    value_(Calculate(str))
{
    GetGlobalStringHashRegister()->RegisterString(*this, str);
}
#endif

StringHashRegister* StringHash::GetGlobalStringHashRegister()
{
#ifdef LUTEFISK3D_HASH_DEBUG
    return &Urho3D::GetGlobalStringHashRegister();
#else
    return nullptr;
#endif
}
QString StringHash::ToString() const
{
    return QString("%1").arg(value_,8,16,QChar('0'));
}

QString StringHash::Reverse() const
{
#ifdef LUTEFISK3D_HASH_DEBUG
    return Urho3D::GetGlobalStringHashRegister().GetStringCopy(*this);
#else
    return QString();
#endif
}

}
