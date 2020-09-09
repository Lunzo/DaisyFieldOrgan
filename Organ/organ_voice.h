#ifndef ORGAN_VOICE_H
#define ORGAN_VOICE_H

#include "daisy_field.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

class OrganVoice {
  public:
    OrganVoice() {}
    ~OrganVoice() {}
    void init(float samplerate) {
        active_ = false;
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        env_.Init(samplerate);
        env_.SetSustainLevel(1.0f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);        
    }

    float process() {
        if(active_) {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if(!env_.IsRunning()) {
                active_ = false;
            }
            sig = osc_.Process();            
            return sig * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void onNoteOn(float note, float velocity) {
        note_     = note;
        velocity_ = velocity;
        osc_.SetFreq(mtof(note_));
        active_   = true;
        env_gate_ = true;
    }

    void onNoteOff() {
        env_gate_ = false;
    }

    inline bool  isActive() const { return active_; }
    inline float getNote() const { return note_; }

  private:
    Oscillator osc_;
    Adsr       env_;
    float      note_, velocity_;
    bool       active_;
    bool       env_gate_;
};


template <size_t max_voices>
class VoiceManager {
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void init(float samplerate) {
        for(size_t i = 0; i < max_voices; i++) {
            voices[i].init(samplerate);
        }
    }

    float process() {
        float sum;
        sum = 0.f;
        for(size_t i = 0; i < max_voices; i++)
        {
            sum += voices[i].process();
        }
        return sum;
    }

    void onNoteOn(float notenumber, float velocity) {
        OrganVoice *v = findFreeVoice();
        if(v == NULL) {
            return; // TODO probably want to "steal" oldest voice.
        }
        v->onNoteOn(notenumber, velocity);
    }

    void onNoteOff(float notenumber, float velocity)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            OrganVoice *v = &voices[i];
            if(v->isActive() && v->getNote() == notenumber)
            {
                v->onNoteOff();
            }
        }
    }

    void freeAllVoices()
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].onNoteOff();
        }
    }

    // TODO example of global parameters e.g. drawbar or effect settings.
    // void SetCutoff(float all_val)
    // {
        // for(size_t i = 0; i < max_voices; i++)
        // {
            // voices[i].SetCutoff(all_val);
        // }
    // }

    int numFreeVoices() {
        int result = 0;
        for (size_t i = 0; i < max_voices; i++) {
            if (!voices[i].isActive()) {
                result++;
            }            
        }
        return result;
    }

  private:
    OrganVoice  voices[max_voices];
    OrganVoice *findFreeVoice()
    {
        OrganVoice *v = NULL;
        for(size_t i = 0; i < max_voices; i++)
        {
            if(!voices[i].isActive())
            {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};



#endif