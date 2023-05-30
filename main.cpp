#include "mbed.h"
#include "headers/song_def.h"
#include "headers/song.h"
#include "headers/NHD_0216HZ.h"
#include <string>
using namespace std;

#define JOYSEL D2
#define BUTTON_1 D3 
#define BUTTON_2 D4 
#define BUTTON_3 D5 
#define BUTTON_4 D6 

#define RED_LED D7
#define YELLOW_LED D8
#define GREEN_LED D12

#define RED 0b100
#define YELLOW 0b010
#define GREEN 0b001
#define SPEAKER D9

Song allsongs [] ={
    FUR_ELISE,
    CANNON_IN_D,
    MINUET_IN_G_MAJOR,
    TURKISH_MARCH,
    NOCTRUNE_IN_E_FLAT,
    WALTZ_NO2,
    SYMPHONY_NO5,
    EINE_KLEINE_NACHTAMUSIK
};

volatile bool has_started = false;
volatile int song_index = 0;
volatile bool is_playing = false;
volatile bool wait_for_new_song = false;
string lcd_text[2];
volatile int k;
volatile int temp_index;
volatile float Volume = 0.5;

//Define the threads
Thread t1;
Thread t2;

// Define ticker and timeout
Ticker timer; 
Timeout delay;

//Define the LCD display and the Serial
// CS = D10, MOSI = D11, SCK = D13
NHD_0216HZ lcd(SPI_CS, SPI_MOSI, SPI_SCK);

// Define other inputs and outputs

DigitalIn pause(JOYSEL);
DigitalIn get_input(BUTTON_1);
BusIn  song_number (BUTTON_2,BUTTON_3,BUTTON_4);
BusOut leds(GREEN_LED,YELLOW_LED,RED_LED);
PwmOut speaker(SPEAKER);

// Display the song name on the LCD and the RGB LEDs
void update_lcd_leds_thread() {
    bool yellow_blink = false;
	while (1) {
        lcd.clr_lcd();
        lcd.set_cursor(0, 0);
        lcd.printf(lcd_text[0].c_str());
        lcd.set_cursor(0, 1);
        lcd.printf(lcd_text[1].c_str());

        // playing: green
        // paused: yellow
        // no song: red
        // selection: blink yellow

        if (!has_started) {
            leds.write(RED);
        } else if (wait_for_new_song) {
            yellow_blink = !yellow_blink;
            leds.write(yellow_blink ? YELLOW : 0);
        } else if (is_playing) {
            leds.write(GREEN);
        } else {
            leds.write(YELLOW);
        }

       ThisThread::sleep_for(500ms);
	}
}

// Define the ticker ISR
void timer_ISR() {
    if (is_playing) {
        if (k < allsongs[song_index].length) {
            // Set the PWM duty cycle to zero, if there is a sound pause
            if (*(allsongs[song_index].note +k) == No) {
                speaker = 0;            
            } else { // Set the PWM period, which determines the note of the sound
                speaker.period(0.001*(*(allsongs[song_index].note + k)));   
                speaker = Volume;
            }    			
            
            k++;

            // Set the time for the next ticker interrupt
            timer.attach(&timer_ISR, ((*(allsongs[song_index].beat + k)/3)+(allsongs[song_index].tempo/2)));
        } else {
            // If the musical piece is finished, start again
            k = 0;                                                                 	
            speaker = 0;
        }
    } else {
        speaker = 0;
    }
}

// Define pause button handler
void pause_button_handler() {
    is_playing = !is_playing;
}

void no_response() {
    song_index = temp_index;
    lcd_text[0] = allsongs[song_index].name1;
    lcd_text[1] = allsongs[song_index].name2;
    wait_for_new_song = false;
    is_playing = true;
    timer.attach(&timer_ISR, 100ms);
}

// Define get input handler
void get_input_handler() {

    if (wait_for_new_song == false) {
        temp_index = song_number.mask() ^ song_number.read();
        printf("song_number is %d\n", temp_index);
        lcd_text[0] = "Do you want to ";
        lcd_text[1] = "play song " + to_string(temp_index);
        wait_for_new_song = true;
        printf("waiting_for_song is true\n");

        delay.attach(&no_response, 5s);
    } else {
        printf("waiting_for_song is true\n");
        // YOUR JOB TO UPDATE: we are waiting for a new song;
        // set up the timer and LCD as needed
        wait_for_new_song = false;
        is_playing = true;
        song_index = temp_index;
        timer.attach(&timer_ISR, 100ms); // Initialize the time ticker
    }
}

void polling_loop() {
    // variables you can use to track events
    bool pressed_get_input = false;
    bool pressed_pause = false;

	while (1) {
        if (get_input.read() == 0) { // 0 because it's PullUp
            has_started = true;
            printf("get_input.read() is 0\n");
            get_input_handler();
        }

        if (pause.read() == 0) { // 0 because it's PullUp
            pause_button_handler();
        }
		
        ThisThread::sleep_for(250ms);
	}
}

void exampleLCDWrite() {
    // lcd_text[0] = "";
    // lcd_text[1] = "";
    lcd_text[0] = "Hello ENGR 029";
    lcd_text[1] = "and Hello World! ";
    lcd.clr_lcd();
    lcd.set_cursor(0, 0);
    lcd.printf(lcd_text[0].c_str());
    lcd.set_cursor(0, 1);
    lcd.printf(lcd_text[1].c_str());
}


/*----------------------------------------------------------------------------
 MAIN function
 *----------------------------------------------------------------------------*/

int main() {
    // joystick shield
    pause.mode(PullUp);
    get_input.mode(PullUp);
    song_number.mode(PullUp);

	// Initialise and clear the LCD display
	lcd.init_lcd();
    lcd.clr_lcd();
  
    t1.start(update_lcd_leds_thread);
    t2.start(polling_loop);

    printf("Choose a song:\r\n");
    for (int i=0; i<8; i++) {
        printf("     %d %s %s %s %s\r\n", i, " - ", allsongs[i].name1.c_str(), " ", allsongs[i].name2.c_str());
    }
    printf("Using the Up, Down, and Left Buttons, insert the song number in binary form\r\n");
    printf("Then press the Right Button and release all the buttons\r\n");

    // Test LEDs and speaker
    // leds = RED;
    // speaker.period_ms(5);
    // speaker.write(0.5f);
	// Wait for timer interrupt
    while(1){
        // exampleLCDWrite();
        // polling_loop();
        // Test buttons
        //printf("Select: %d, Get Input: %d, song_number: %d\n",pause.read(),get_input.read(),~song_number.read() & 0x07);
        thread_sleep_for(500);

        // Uncomment below for operation w/ timer (WFI means wait for interrupt)
        __WFI();
	}
}