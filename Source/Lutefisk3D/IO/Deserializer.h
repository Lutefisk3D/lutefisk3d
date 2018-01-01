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

#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Container/HashMap.h"
#include <vector>

class QStringList;

namespace Urho3D
{
enum VariantType : uint8_t;
class Variant;
using VariantMap = HashMap<StringHash, Variant>;
struct ResourceRefList;
struct ResourceRef;
class Color;
class IntRect;
class IntVector2;
class IntVector3;
class Quaternion;
class Rect;
class BoundingBox;
class Matrix3;
class Matrix3x4;
class Matrix4;
class Vector2;
class Vector3;
class Vector4;
/// Abstract stream for reading.
class LUTEFISK3D_EXPORT Deserializer
{
public:
    /// Construct with zero size.
    Deserializer();
    /// Construct with defined size.
    Deserializer(unsigned size);
    /// Destruct.
    virtual ~Deserializer();

    /// Read bytes from the stream. Return number of bytes actually read.
    virtual unsigned Read(void* dest, unsigned size) = 0;
    /// Set position from the beginning of the stream.
    virtual unsigned Seek(unsigned position) = 0;
    /// Return name of the stream.
    virtual const QString& GetName() const;
    /// Return a checksum if applicable.
    virtual unsigned GetChecksum();
    /// Return whether the end of stream has been reached.
    virtual bool IsEof() const { return position_ >= size_; }
    /// Set position relative to current position. Return actual new position.
    unsigned SeekRelative(int delta);
    /// Return current position.
    unsigned GetPosition() const { return position_; }
    /// Return current position.
    unsigned Tell() const { return position_; }
    /// Return size.
    unsigned GetSize() const { return size_; }

    /// Read a 64-bit integer.
    long long ReadInt64();
    /// Read a 32-bit integer.
    int ReadInt();
    /// Read a 16-bit integer.
    short ReadShort();
    /// Read an 8-bit integer.
    signed char ReadByte();
    /// Read a 64-bit unsigned integer.
    uint64_t ReadUInt64();
    /// Read a 32-bit unsigned integer.
    unsigned ReadUInt();
    /// Read a 16-bit unsigned integer.
    unsigned short ReadUShort();
    /// Read an 8-bit unsigned integer.
    unsigned char ReadUByte();
    /// Read a bool.
    bool ReadBool();
    /// Read a float.
    float ReadFloat();
    /// Read a double.
    double ReadDouble();
    /// Read an IntRect.
    IntRect ReadIntRect();
    /// Read an IntVector2.
    IntVector2 ReadIntVector2();
    /// Read an IntVector3.
    IntVector3 ReadIntVector3();
    /// Read a Rect.
    Rect ReadRect();
    /// Read a Vector2.
    Vector2 ReadVector2();
    /// Read a Vector3.
    Vector3 ReadVector3();
    /// Read a Vector3 packed into 3 x 16 bits with the specified maximum absolute range.
    Vector3 ReadPackedVector3(float maxAbsCoord);
    /// Read a Vector4.
    Vector4 ReadVector4();
    /// Read a quaternion.
    Quaternion ReadQuaternion();
    /// Read a quaternion with each component packed in 16 bits.
    Quaternion ReadPackedQuaternion();
    /// Read a Matrix3.
    Matrix3 ReadMatrix3();
    /// Read a Matrix3x4.
    Matrix3x4 ReadMatrix3x4();
    /// Read a Matrix4.
    Matrix4 ReadMatrix4();
    /// Read a color.
    Color ReadColor();
    /// Read a bounding box.
    BoundingBox ReadBoundingBox();
    /// Read a null-terminated string.
    QString ReadString();
    /// Read a four-letter file ID.
    QString ReadFileID();
    /// Read a 32-bit StringHash.
    StringHash ReadStringHash();
    /// Read a buffer with size encoded as VLE.
    std::vector<unsigned char> ReadBuffer();
    /// Read a resource reference.
    ResourceRef ReadResourceRef();
    /// Read a resource reference list.
    ResourceRefList ReadResourceRefList();
    /// Read a variant.
    Variant ReadVariant();
    /// Read a variant whose type is already known.
    Variant ReadVariant(VariantType type);
    /// Read a variant vector.
    std::vector<Variant> ReadVariantVector();
    /// Read a string vector.
    QStringList ReadStringVector();
    /// Read a variant map.
    VariantMap ReadVariantMap();
    /// Read a variable-length encoded unsigned integer, which can use 29 bits maximum.
    unsigned ReadVLE();
    /// Read a 24-bit network object ID.
    unsigned ReadNetID();
    /// Read a text line.
    QString ReadLine();

protected:
    /// Stream position.
    unsigned position_;
    /// Stream size.
    unsigned size_;
};

}
