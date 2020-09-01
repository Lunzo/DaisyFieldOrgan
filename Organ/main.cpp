#include "daisy_field.h"
#include "daisysp.h" // Uncomment this if you want to use the DSP library.
#include <string>

using namespace daisy;

// Declare a local daisy_field for hardware access
DaisyField hw;

const int NUM_DRAWBARS = 8;

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
    
    void Init() {
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
    int level;
};

drawbar drawbars[NUM_DRAWBARS];
int drawbar_leds_top[] = {
    DaisyField::LED_KEY_A8,
    DaisyField::LED_KEY_A7,
    DaisyField::LED_KEY_A6,
    DaisyField::LED_KEY_A5,
    DaisyField::LED_KEY_A4,
    DaisyField::LED_KEY_A3,
    DaisyField::LED_KEY_A2,
    DaisyField::LED_KEY_A1
};

int drawbar_leds_bottom[] = {
    DaisyField::LED_KEY_B8,
    DaisyField::LED_KEY_B7,
    DaisyField::LED_KEY_B6,
    DaisyField::LED_KEY_B5,
    DaisyField::LED_KEY_B4,
    DaisyField::LED_KEY_B3,
    DaisyField::LED_KEY_B2,
    DaisyField::LED_KEY_B1
};

// This runs at a fixed rate, to prepare audio samples
void callback(float *in, float *out, size_t size)
{
    hw.ProcessAnalogControls();
    hw.UpdateDigitalControls();
    // TODO - create voices and send to output (See KeyboardTest.cpp)
    
    // TODO move out of audio callback?
    // Update drawbar levels
    for (size_t i = 0; i < 16; i++) {       
        // TODO may want to initially respond to KeyboardRisingEdge
        //  then after a certain amount of delay keep responding if held down
        if (hw.KeyboardFallingEdge(i)) {
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
            char *      cstr = &str[0];
            hw.display.SetCursor(0, 0);
            hw.display.WriteString(cstr, Font_7x10, true);
            hw.display.Update();
        }        
    }

    // Audio is interleaved stereo by default
    for (size_t i = 0; i < size; i+=2)
    {
        out[i] = in[i]; // left
        out[i+1] = in[i+1]; // right
    }
}

void DisplayDebugMessage(int row, std::string message) {
    hw.display.SetCursor(0, 10 * row);
    hw.display.WriteString(&message[0], Font_7x10, true);
    hw.display.Update();
}

int main(void)
{
    hw.Init();
    hw.StartAdc();
    hw.StartAudio(callback);    
    while(1) 
    {        
        // Do Stuff Infinitely Here

        // Update LEDs
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
}
