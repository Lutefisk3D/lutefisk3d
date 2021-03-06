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

#include "Lutefisk3D/Math/Matrix2.h"

#include <cstdio>
#include <QtCore/QString>

namespace Urho3D
{

const Matrix2 Matrix2::ZERO(
    0.0f, 0.0f,
    0.0f, 0.0f);

const Matrix2 Matrix2::IDENTITY;

Matrix2 Matrix2::Inverse() const
{
    float det = m00_ * m11_ -
                m01_ * m10_;

    float invDet = 1.0f / det;

    return Matrix2(
        m11_, -m01_,
        -m10_, m00_
    ) * invDet;
}

QString Matrix2::ToString() const
{
    return QString("%1 %2 %3 %4").arg(m00_).arg(m01_).arg(m10_).arg(m11_);
}
}
