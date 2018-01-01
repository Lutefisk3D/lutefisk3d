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

#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Audio/SoundStream.h"
#include <deque>
#include <utility>

namespace Urho3D
{

/// %Sound stream that supports manual buffering of data from the main thread.
class LUTEFISK3D_EXPORT BufferedSoundStream : public SoundStream
{
public:
    BufferedSoundStream();
    ~BufferedSoundStream() override;

    /// Produce sound data into destination. Return number of bytes produced. Called by SoundSource from the mixing
    /// thread.
    unsigned GetData(signed char *dest, unsigned numBytes) override;

    /// Buffer sound data. Makes a copy of it.
    void AddData(void* data, unsigned numBytes);
    /// Buffer sound data by taking ownership of it.
    void AddData(SharedArrayPtr<signed char> data, unsigned numBytes);
    /// Buffer sound data by taking ownership of it.
    void AddData(SharedArrayPtr<signed short> data, unsigned numBytes);
    /// Remove all buffered audio data.
    void Clear();

    /// Return amount of buffered (unplayed) sound data in bytes.
    unsigned GetBufferNumBytes() const;
    /// Return length of buffered (unplayed) sound data in seconds.
    float GetBufferLength() const;

private:
    /// Buffers and their sizes.
    std::deque<std::pair<SharedArrayPtr<int8_t>, unsigned> > buffers_;
    /// Byte position in the frontmost buffer.
    unsigned position_;
    /// Mutex for buffer data.
    mutable Mutex bufferMutex_;
};

}
