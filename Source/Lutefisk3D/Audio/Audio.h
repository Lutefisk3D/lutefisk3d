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

#include "Lutefisk3D/Audio/AudioDefs.h"
#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "jlsignal/SignalBase.h"
namespace Urho3D
{

class AudioImpl;
class Sound;
class SoundListener;
class SoundSource;

/// %Audio subsystem.
class LUTEFISK3D_EXPORT Audio : public Object, public jl::SignalObserver
{
    URHO3D_OBJECT(Audio,Object)
    friend void SDLAudioCallback(void *userdata, uint8_t* stream, int len);
public:
    /// Construct.
    Audio(Context* context);
    /// Destruct. Terminate the audio thread and free the audio buffer.
    virtual ~Audio();

    bool SetMode(int bufferLengthMSec, int mixRate, bool stereo, bool interpolation = true);
    void Update(float timeStep);
    bool Play();
    void Stop();
    void SetMasterGain(const QString& type, float gain);
    void PauseSoundType(const QString& type);
    void ResumeSoundType(const QString& type);
    void ResumeAll();
    void SetListener(SoundListener* listener);
    void StopSound(Sound* sound);

    /// Return byte size of one sample.
    unsigned GetSampleSize() const { return sampleSize_; }
    /// Return mixing rate.
    int GetMixRate() const { return mixRate_; }
    /// Return whether output is interpolated.
    bool GetInterpolation() const { return interpolation_; }
    /// Return whether output is stereo.
    bool IsStereo() const { return stereo_; }
    /// Return whether audio is being output.
    bool IsPlaying() const { return playing_; }
    /// Return whether an audio stream has been reserved.
    bool IsInitialized() const { return deviceID_ != 0; }
    float GetMasterGain(const QString& type) const;
    bool IsSoundTypePaused(const QString& type) const;
    SoundListener* GetListener() const;
    /// Return all sound sources.
    const std::vector<SoundSource*>& GetSoundSources() const { return soundSources_; }
    /// Return whether the specified master gain has been defined.
    bool HasMasterGain(const QString& type) const { return masterGain_.contains(type); }
    void AddSoundSource(SoundSource* soundSource);
    void RemoveSoundSource(SoundSource* soundSource);
    /// Return audio thread mutex.
    Mutex& GetMutex() { return audioMutex_; }
    float GetSoundSourceMasterGain(StringHash typeHash) const;
    /// Final multiplier for for audio byte conversion
    static const int SAMPLE_SIZE_MUL = 1;
private:
    void MixOutput(void *dest, unsigned samples);
    void Release();
    void UpdateInternal(float timeStep);

    SharedArrayPtr<int> clipBuffer_; ///< Clipping buffer for mixing.
    Mutex audioMutex_;  ///< Audio thread mutex.
    unsigned deviceID_; ///< SDL audio device ID.
    unsigned sampleSize_; ///< Sample size.
    unsigned fragmentSize_; ///< Clip buffer size in samples.
    int mixRate_;       ///< Mixing rate.
    bool interpolation_;///< Mixing interpolation flag.
    bool stereo_;       ///< Stereo flag.
    bool playing_;      ///< Playing flag.
    HashMap<StringHash, float> masterGain_;///< Master gain by sound source type.
    HashSet<StringHash> pausedSoundTypes_;///< Paused sound types.
    std::vector<SoundSource*> soundSources_; ///< Sound sources. TODO: consider std::set ?
    WeakPtr<SoundListener> listener_; ///< Sound listener.
};

/// Register Audio library objects.
void LUTEFISK3D_EXPORT RegisterAudioLibrary(Context* context);

}
