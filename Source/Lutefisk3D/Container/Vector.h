//
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

#include "../Container/VectorBase.h"
#include "SmallVector.h"
#include <cassert>
#include <vector>
#include <algorithm>
namespace Urho3D
{

/// %Vector template class.

template <typename T>
class PODVector : public std::vector<T> {
public:
    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;
    PODVector() {}
    PODVector(int sz) : std::vector<T>(sz) {}
    constexpr bool contains(const T &v) const { return find(v)!=this->cend();}
    iterator find(const T &v) { return std::find(this->begin(),this->end(),v);}
    const_iterator find(const T &v) const { return std::find(this->cbegin(),this->cend(),v);}
    /// Erase an element if found.
    bool remove(const T& value)
    {
        iterator i = find(value);
        if (i == this->end())
            return false;
        this->erase(i);
        return true;
    }
};
template<typename T>
using Vector = PODVector<T> ;
template <typename T,int N>
class PODVectorN : public lls::SmallVector<T,N> {
public:
    typedef typename lls::SmallVector<T,N>::iterator iterator;
    typedef typename lls::SmallVector<T,N>::const_iterator const_iterator;
    PODVectorN() {}
    PODVectorN(int sz) : lls::SmallVector<T,N>(sz) {}
    constexpr bool contains(const T &v) const { return find(v)!=this->end();}
    iterator find(const T &v) { return std::find(this->begin(),this->end(),v);}
    const_iterator find(const T &v) const { return std::find(this->begin(),this->end(),v);}
    /// Erase an element if found.
    bool remove(const T& value)
    {
        iterator i = find(value);
        if (i == this->end())
            return false;
        this->erase(i);
        return true;
    }
};
}
