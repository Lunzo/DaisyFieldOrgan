#include "daisy_field.h"
#include "daisysp.h"
#include <string>
#include "organ_voice.h"

using namespace daisy;

// Declare a local daisy_field for hardware access
DaisyField hw;
MidiHandler midi;

const int NUM_DRAWBARS = 8;

/**
 * Helpful utility to write a debug message to the OLED display.
 * Should be ~6 rows at chosen font size?
 */
void displayDebugMessage(int row, std::string message) {
    // pad with spaces in case new message is shorter than old one on this row.
    message.resize(25, ' ');    
    hw.display.SetCursor(0, 10 * row);
    hw.display.WriteString(&message[0], Font_7x10, true);
    hw.display.Update();
}

uint8_t drawbar_level_to_led_brightness[] = {
    0,
    100,
    140,    
    180,
    220,
    255
};

/* Metaphor from Hammond Organ.
We'll use the LED buttons to control and display the levels of the partials */
struct drawbar {    
    // TODO - find out why it starts at 1 instead of 0
    //  I'm guessing there's a "button press" right at startup.
    
    void init() {
        level = 0;
    }
    void increase() {
        if (level == 10) { return; }
        level++;
    }
    void decrease() {
        if (level == 0) { return; }
        level--;
    }
    uint8_t brightness_bottom() {
        if (level >= 5) {
            return drawbar_level_to_led_brightness[5];
        }
        return drawbar_level_to_led_brightness[level];
    }
    uint8_t brightness_top() {
        if (level < 5) {
            return 0;
        }
        return drawbar_level_to_led_brightness[level - 5];
    }
    int level = 0;
};

drawbar drawbars[NUM_DRAWBARS];
int drawbar_leds_top[] = {
    DaisyField::LED_KEY_A1,
    DaisyField::LED_KEY_A2,
    DaisyField::LED_KEY_A3,
    DaisyField::LED_KEY_A4,
    DaisyField::LED_KEY_A5,
    DaisyField::LED_KEY_A6,
    DaisyField::LED_KEY_A7,
    DaisyField::LED_KEY_A8
};

int drawbar_leds_bottom[] = {
    DaisyField::LED_KEY_B1,
    DaisyField::LED_KEY_B2,
    DaisyField::LED_KEY_B3,
    DaisyField::LED_KEY_B4,
    DaisyField::LED_KEY_B5,
    DaisyField::LED_KEY_B6,
    DaisyField::LED_KEY_B7,
    DaisyField::LED_KEY_B8
};

static VoiceManager<24> voice_handler;

// This runs at a fixed rate, to prepare audio samples
void audioCallback(float *in, float *out, size_t size)
{
    hw.ProcessAnalogControls();
    hw.UpdateDigitalControls();
    if(hw.GetSwitch(hw.SW_1)->FallingEdge()) {
        voice_handler.freeAllVoices();
    }
    
    // TODO move out of audio callback?
    // Update drawbar levels
    std::string init_debug = "";
    
    for (size_t i = 0; i < 16; i++) {        
        // TODO may want to initially respond to KeyboardRisingEdge
        //  then after a certain amount of delay keep responding if held down
        if (hw.KeyboardFallingEdge(i)) {
            init_debug += std::to_string(i);
            size_t db_ind = i;
            if (i >= NUM_DRAWBARS) {
                db_ind = i - NUM_DRAWBARS;
                drawbars[db_ind].increase();
            } else {
                drawbars[db_ind].decrease();
            }
            
            // show status change on display
            //std::string str = "Button " + std::to_string(i);
            std::string str = "Drawbar " + std::to_string(db_ind) + " = " + std::to_string(drawbars[db_ind].level) + "   ";
            displayDebugMessage(0, str);
            displayDebugMessage(1, init_debug);
        }        
    }

    // Audio is interleaved stereo by default
    for (size_t i = 0; i < size; i+=2)
    {
        float sum = 0.f;
        sum = voice_handler.process() * 0.5f;
        out[i] = sum; // left
        out[i+1] = sum; // right
    }
}

/**
 * Update the status LEDs on the field.
 */
void updateLeds() {
    hw.led_driver_.SetAllTo((uint8_t) 0);
    hw.led_driver_.SetLed(DaisyField::LED_SW_1, (uint8_t) 255);
    hw.led_driver_.SetLed(DaisyField::LED_SW_2, (uint8_t) 255);
    for (int db_ind = 0; db_ind < NUM_DRAWBARS; db_ind++) {
        hw.led_driver_.SetLed(drawbar_leds_top[db_ind], drawbars[db_ind].brightness_top());
        hw.led_driver_.SetLed(drawbar_leds_bottom[db_ind], drawbars[db_ind].brightness_bottom());
    }
    hw.display.Update(); // I have no idea why but this is needed to make LEDs go.
    hw.led_driver_.SwapBuffersAndTransmit();
}


/**
 * Process a single MIDI message.
 */
void processMidiMessage(MidiEvent m) {
    displayDebugMessage(1, "Voices free: " + std::to_string(voice_handler.numFreeVoices()));
    switch(m.type) {
        case NoteOn: {
            NoteOnEvent p = m.AsNoteOn();
			// Note Off can come in as Note On w/ 0 Velocity
            if(p.velocity <= 0.001f) {
                voice_handler.onNoteOff(p.note, p.velocity);
            }
            else {
                voice_handler.onNoteOn(p.note, p.velocity);
            }
        }
        break;
        case NoteOff: {
            NoteOnEvent p = m.AsNoteOn();
            voice_handler.onNoteOff(p.note, p.velocity);
        }
        break;
        default: break; // TODO other MidiMessageTypes, esp PitchBend.
            // https://github.com/electro-smith/libDaisy/blob/aaa17b041a4eb0c00a583be6b383af8c02797682/src/hid/midi.h#L20
    }    
}


int main(void) {
    hw.Init();
    float samplerate = hw.SampleRate();
    midi.Init(MidiHandler::INPUT_MODE_UART1, MidiHandler::OUTPUT_MODE_NONE);
    voice_handler.init(samplerate);

    midi.StartReceive();
    hw.StartAdc();
    hw.StartAudio(audioCallback);    

    for (int i = 0; i < NUM_DRAWBARS; i++) {
        drawbars[i].init();
    }

    while(1) {
        midi.Listen();        
        while(midi.HasEvents()) {
            processMidiMessage(midi.PopEvent());
        }

        updateLeds();
    }
}
