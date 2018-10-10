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

#pragma once

#include "Lutefisk3D/Core/Variant.h"

namespace Urho3D
{

/// Parse a bool from a string. Check for the first non-empty character (converted to lowercase) being either 't', 'y' or '1'.
LUTEFISK3D_EXPORT bool ToBool(const QString& source);
/// Parse a bool from a C string. Check for the first non-empty character (converted to lowercase) being either 't', 'y' or '1'.
LUTEFISK3D_EXPORT bool ToBool(const char* source);
/// Parse a Color from a string.
LUTEFISK3D_EXPORT Color ToColor(const QString& source);
/// Parse a Color from a C string.
LUTEFISK3D_EXPORT Color ToColor(const char* source);
/// Parse an IntRect from a string.
LUTEFISK3D_EXPORT IntRect ToIntRect(const QString& source);
/// Parse an IntRect from a C string.
LUTEFISK3D_EXPORT IntRect ToIntRect(const char* source);
/// Parse an IntVector2 from a string.
LUTEFISK3D_EXPORT IntVector2 ToIntVector2(const QString& source);
/// Parse an IntVector2 from a C string.
LUTEFISK3D_EXPORT IntVector2 ToIntVector2(const char* source);
/// Parse an IntVector3 from a string.
LUTEFISK3D_EXPORT  IntVector3 ToIntVector3(const QString& source);
/// Parse an IntVector3 from a C string.
LUTEFISK3D_EXPORT  IntVector3 ToIntVector3(const char* source);
/// Parse a Quaternion from a string. If only 3 components specified, convert Euler angles (degrees) to quaternion.
LUTEFISK3D_EXPORT Quaternion ToQuaternion(const QString& source);
/// Parse a Quaternion from a C string. If only 3 components specified, convert Euler angles (degrees) to quaternion.
LUTEFISK3D_EXPORT Quaternion ToQuaternion(const char* source);
/// Parse a Rect from a string.
LUTEFISK3D_EXPORT Rect ToRect(const QString& source);
/// Parse a Rect from a C string.
LUTEFISK3D_EXPORT Rect ToRect(const char* source);
/// Parse a Vector2 from a string.
LUTEFISK3D_EXPORT Vector2 ToVector2(const QString& source);
/// Parse a Vector2 from a C string.
LUTEFISK3D_EXPORT Vector2 ToVector2(const char* source);
/// Parse a Vector3 from a string.
LUTEFISK3D_EXPORT Vector3 ToVector3(const QString& source);
/// Parse a Vector3 from a C string.
LUTEFISK3D_EXPORT Vector3 ToVector3(const char* source);
/// Parse a Vector4 from a string.
LUTEFISK3D_EXPORT Vector4 ToVector4(const QString& source, bool allowMissingCoords = false);
/// Parse a Vector4 from a C string.
LUTEFISK3D_EXPORT Vector4 ToVector4(const char* source, bool allowMissingCoords = false);
/// Parse a float, Vector or Matrix variant from a string. Return empty variant on illegal input.
LUTEFISK3D_EXPORT Variant ToVectorVariant(const QString& source);
/// Parse a float, Vector or Matrix variant from a C string. Return empty variant on illegal input.
LUTEFISK3D_EXPORT Variant ToVectorVariant(const char* source);
/// Parse a Matrix3 from a string.
LUTEFISK3D_EXPORT Matrix3 ToMatrix3(const QString& source);
/// Parse a Matrix3 from a C string.
LUTEFISK3D_EXPORT Matrix3 ToMatrix3(const char* source);
/// Parse a Matrix3x4 from a string.
LUTEFISK3D_EXPORT Matrix3x4 ToMatrix3x4(const QString& source);
/// Parse a Matrix3x4 from a C string.
LUTEFISK3D_EXPORT Matrix3x4 ToMatrix3x4(const char* source);
/// Parse a Matrix4 from a string.
LUTEFISK3D_EXPORT Matrix4 ToMatrix4(const QString& source);
/// Parse a Matrix4 from a C string.
LUTEFISK3D_EXPORT Matrix4 ToMatrix4(const char* source);
/// Convert a pointer to string (returns hexadecimal.)
LUTEFISK3D_EXPORT QString ToString(void* value);
/// Convert an unsigned integer to string as hexadecimal.
LUTEFISK3D_EXPORT QString ToStringHex(unsigned value);
/// Convert a byte buffer to a string.
LUTEFISK3D_EXPORT void BufferToString(QString& dest, const void* data, unsigned size);
/// Convert a string to a byte buffer.
LUTEFISK3D_EXPORT void StringToBuffer(std::vector<unsigned char>& dest, const QString& source);
/// Convert a C string to a byte buffer.
LUTEFISK3D_EXPORT void StringToBuffer(std::vector<unsigned char>& dest, const char* source);
/// Return an index to a string list corresponding to the given string, or a default value if not found. The string list must be empty-terminated.
LUTEFISK3D_EXPORT unsigned GetStringListIndex(const QString& value, const QString* strings, unsigned defaultIndex, bool caseSensitive = false);
/// Return an index to a string list corresponding to the given C string, or a default value if not found. The string list must be empty-terminated.
LUTEFISK3D_EXPORT unsigned GetStringListIndex(const char* value, const QString* strings, unsigned defaultIndex, bool caseSensitive = false);
/// Return an index to a C string list corresponding to the given C string, or a default value if not found. The string list must be empty-terminated.
LUTEFISK3D_EXPORT unsigned GetStringListIndex(const QString & value, const char * const *strings, unsigned defaultIndex, bool caseSensitive = false);
/// Convert a memory size into a formatted size string, of the style "1.5 Mb".
LUTEFISK3D_EXPORT QString GetFileSizeString(uint64_t memorySize);
}
