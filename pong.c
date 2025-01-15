#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#include <ctype.h>

#include <c64.h>
#include <cbm.h>

#define WIDTH 40
#define HEIGHT 24
#define BALL_SIZE 1
#define PADDLE_WIDTH 1
#define PADDLE_HEIGHT 4

#define BALL_SHAPE 15
#define LEFT_PADDLE_SHAPE 118
#define RIGHT_PADDLE_SHAPE 117
#define BACKGROUND_SHAPE ' '

#define BORDER_COLOR ((char*)0xD020)
#define BACKGROUND_COLOR ((char*)0xD021)

#define JOY1 ((char*)0xDC01)
#define JOY2 ((char*)0xDC00)
#define JOY_UP 1
#define JOY_DOWN 2
#define JOY_LEFT 4
#define JOY_RIGHT 8
#define JOY_BUTTON 16
#define IS_DOWN( value, mask ) (!(value&mask))

int music_on = 1;
int sound_on = 1;
int debug_on = 0;
int vertical_blank = 0;
int update_speed = 10;
int update_counter = 0;

int left_score = 0;
int right_score = 0;

// Positions and velocities
int ball_x, ball_y;
int ball_dx = 1, ball_dy = 1; // Ball speed/direction
int old_ball_x = 0, old_ball_y = 0;

int left_paddle_y, right_paddle_y;
int old_left_paddle_y = 0, old_right_paddle_y = 0;

void initialize();
void draw();
void update();
void reset_ball();
void reset_screen();


// Define base addresses for SID registers
#define SID_MAIN_VOLUME 0xD418    // 54296, Main volume register
#define SID_FREQ_HIA    0xD400    // 54272, Channel A frequency high byte
#define SID_FREQ_LOA    0xD401    // 54273, Channel A frequency low byte
#define SID_CTRL        0xD404    // 54276, Channel A control settings (waveform, etc.)
#define SID_ENV1        0xD406    // 54278, Channel A envelope low byte

void playTriNote( int freq, int duration ) {
    int i, j, step;
    // Obtain pointers to the SID register addresses
    volatile char* mainVolume = (volatile char*)SID_MAIN_VOLUME;
    volatile char* freqLoA = (volatile char*)SID_FREQ_LOA;
    volatile char* freqHiA = (volatile char*)SID_FREQ_HIA;
    volatile char* env1 = (volatile char*)SID_ENV1;
    volatile char* sidCtrl = (volatile char*)SID_CTRL;

    // Set voice 1 frequency to C-4 (middle C)
    *freqLoA = freq&0xFF;   // Low byte of the frequency for C-4
    *freqHiA = (freq>>8)&0xFF;    // High byte of the frequency for C-4

    // Set voice 1 envelope sustain to max
    *env1 = 240;

    // Set waveform to triangle (16) and open the gate (+1)
    *sidCtrl = 17; // Triangle wave, gate open

    // Fade volume
    step = duration / 3;
    for( i = 7; i >= 0; i-- )
    {
        *mainVolume = i;
        for( j = 0; j < step; j++ ) {}
    }
}

void playPingSound() {
    int i, j;

    // Obtain pointers to the SID register addresses
    volatile char* mainVolume = (volatile char*)SID_MAIN_VOLUME;
    volatile char* freqLoA = (volatile char*)SID_FREQ_LOA;
    volatile char* freqHiA = (volatile char*)SID_FREQ_HIA;
    volatile char* env1 = (volatile char*)SID_ENV1;
    volatile char* sidCtrl = (volatile char*)SID_CTRL;

    if( !sound_on ) return;

    // Set voice 1 frequency to C-4 (middle C)
    *freqLoA = 104;   // Low byte of the frequency for C-4
    *freqHiA = 17;    // High byte of the frequency for C-4

    // Set voice 1 envelope sustain to max
    *env1 = 240;

    // Set waveform to triangle (16) and open the gate (+1)
    *sidCtrl = 17; // Triangle wave, gate open

    // Set main volume to max
    for( i = 15; i >= 0; i-- )
    {
        *mainVolume = i;
        for( j = 0; j < 10; j++ ) {}
    }
}

void playCrashSound() {
    int i, j;

    // Obtain pointers to the SID register addresses
    volatile char* mainVolume = (volatile char*)SID_MAIN_VOLUME;
    volatile char* freqLoA = (volatile char*)SID_FREQ_LOA;
    volatile char* freqHiA = (volatile char*)SID_FREQ_HIA;
    volatile char* env1 = (volatile char*)SID_ENV1;
    volatile char* sidCtrl = (volatile char*)SID_CTRL;

    if( !sound_on ) return;

    // Set voice 1 frequency to C-4 (middle C)
    *freqLoA = 104;   // Low byte of the frequency for C-4
    *freqHiA = 17;    // High byte of the frequency for C-4

    // Set voice 1 envelope sustain to max
    *env1 = 240;

    // Set waveform to triangle (16) and open the gate (+1)
    *sidCtrl = 129; // Noise

    // Set main volume to max
    for( i = 15; i >= 0; i-- )
    {
        *mainVolume = i;
        for( j = 0; j < 100; j++ ) {}
    }
}


// Define the number of notes and octaves
#define NUM_NOTES 12
#define NUM_OCTAVES 9

// Two-dimensional array representing note frequencies (Hz) for each octave
int noteFrequencies[NUM_NOTES][NUM_OCTAVES] = {
    {16, 33, 65, 131, 262, 523, 1047, 2093, 4186}, // C
    {17, 35, 69, 139, 277, 554, 1109, 2217, 4435}, // C#/Db
    {18, 37, 73, 147, 294, 587, 1175, 2350, 4699}, // D
    {19, 39, 78, 155, 311, 622, 1245, 2489, 4978}, // D#/Eb
    {20, 41, 82, 165, 329, 659, 1319, 2637, 5274}, // E
    {21, 44, 87, 175, 349, 698, 1397, 2794, 5588}, // F
    {23, 46, 92, 185, 370, 740, 1479, 2959, 5919}, // F#/Gb
    {24, 49, 98, 196, 392, 783, 1568, 3136, 6272}, // G
    {26, 51, 103, 207, 415, 831, 1661, 3322, 6645}, // G#/Ab
    {27, 55, 110, 220, 440, 880, 1760, 3520, 7040}, // A
    {29, 58, 116, 233, 466, 932, 1865, 3729, 7459}, // A#/Bb
    {31, 61, 123, 247, 494, 988, 1976, 3951, 7902}  // B
};

// Array of note names for easy reference
const char* noteNames[NUM_NOTES] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

int strcmp( const char* str1, const char* str2 )
{
    while( *str1 && *str2)
    {
        if (*str1 != *str2 )  {
            return -1;
        }
        ++str1;
        ++str2;
    }
    return 0;
}

// Function to get frequency from note name and octave
int getNoteFrequency(const char* noteName, int octave) {
    int noteIndex = -1;
    int i;
    if (octave < 0 || octave >= NUM_OCTAVES) {
        return -1; // Return an error code for invalid input
    }

    // Find the index of the note name in noteNames array
    for (i = 0; i < NUM_NOTES; ++i) {
        if (strcmp(noteName, noteNames[i]) == 0) {
            noteIndex = i;
            break;
        }
    }

    // If the note name is not found, return an error code
    if (noteIndex == -1) {
        return -1; 
    }

    // Return the frequency for the given note and octave
    return noteFrequencies[noteIndex][octave];
}

// Function to play a note for a specific duration
void playNote(const char* noteName, int octave, int duration) {
    int frequency = getNoteFrequency(noteName, octave);
    
    if (frequency != -1) {
        //printf("Playing %s%d at %d Hz\n", noteName, octave, frequency);
        playTriNote(frequency,duration);
    }
}

void playTwinkleTwinkleLittleStar() {
    static int i = 0;
    const char* notes[] = {"C", "C", "G", "G", "A", "A", "G","F", "F", "E", "E", "D", "D", "C","G", "G", "F", "F", "E", "E", "D"};
    int octaves[] = {4, 4, 4, 4, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    int durations[] = {500, 500, 500, 500, 500, 500, 1000, 500, 500, 500, 500, 500, 500, 1000, 500, 500, 500, 500, 500, 500, 1000}; // Durations in milliseconds
    playNote(notes[i], octaves[i], durations[i]);
    i++;
    if( i >= sizeof(notes) )
    {
        i = 0;
    }
}

void drawMainMenu()
{
    gotoxy(0, 10);
    printf("Q - Quit\n");
    printf("D - Debug %s\n", debug_on?"On ":"Off" );
    printf("M - Music %s\n", music_on?"On ":"Off" );
    printf("S - Sound Effects %s\n", sound_on?"On ":"Off");
    printf("Space / Joystick button - Start Game\n");
}

int main() {
    int running = 1;

    BORDER_COLOR[0] = COLOR_BLUE;
    BACKGROUND_COLOR[0] = COLOR_BLUE;

    srand(0); // Seed random number generator

    clrscr();
    drawMainMenu();
    
    while( running )
    {
        unsigned char ch;


        {
            int paused = 1;
            while( running && paused )
            {
                if(music_on) { playTwinkleTwinkleLittleStar(); }
                while ((ch = cbm_k_getin()) != 0)
                {
                    if( ch == 'q' )
                    {
                        running = 0;
                    }
                    else if( ch == ' ' )
                    {
                        update_speed = 10;
                        left_score = 0;
                        right_score = 0;
                        paused = 0;
                    }
                    else if( ch == 'm' || ch == 'M' )
                    {
                        music_on = !music_on;
                        drawMainMenu();
                    }
                    else if( ch == 's' || ch == 'S' )
                    {
                        sound_on = !sound_on;
                        drawMainMenu();
                    }
                    else if( ch == 'd' || ch == 'D' )
                    {
                        debug_on = !debug_on;
                        drawMainMenu();
                    }
                }
                /// Check joysticks
                {
                    char joy1 = *JOY1;
                    char joy2 = *JOY2;
                    if( IS_DOWN( joy1, JOY_BUTTON ) || IS_DOWN( joy2, JOY_BUTTON ) )
                    {
                        update_speed = 10;
                        left_score = 0;
                        right_score = 0;
                        paused = 0;
                    }

                }
            }
        }

        if( running )
        {
            initialize();

            while (left_score < 5 && right_score < 5)
            {
                if( debug_on ) { BORDER_COLOR[0] = COLOR_BLUE; }

                old_left_paddle_y = left_paddle_y;
                old_right_paddle_y = right_paddle_y;
                while ((ch = cbm_k_getin()) != 0) {
                    if ((ch == 'w' || ch == 'W') && left_paddle_y > 2) {
                        left_paddle_y--;
                    } else if ((ch == 's' || ch == 'S') && left_paddle_y < HEIGHT - PADDLE_HEIGHT - 1) {
                        left_paddle_y++;
                    } else if ((ch == 'i' || ch == 'I') && right_paddle_y > 2) {
                        right_paddle_y--;
                    } else if ((ch == 'k' || ch == 'K') && right_paddle_y < HEIGHT - PADDLE_HEIGHT - 1) {
                        right_paddle_y++;
                    } else if ((ch == 'd' || ch == 'D') ) {
                        debug_on = !debug_on;
                    }
                }

                /// Check joysticks
                {
                    char joy1 = *JOY1;
                    char joy2 = *JOY2;
                    if( IS_DOWN( joy1, JOY_UP ) && left_paddle_y > 2 ) {
                        left_paddle_y--;
                    }
                    else if ( IS_DOWN( joy1, JOY_DOWN ) && left_paddle_y < HEIGHT - PADDLE_HEIGHT - 1 ) {
                        left_paddle_y++;
                    }
                    if( IS_DOWN( joy2, JOY_UP ) && right_paddle_y > 2 ) {
                        right_paddle_y--;
                    }
                    else if ( IS_DOWN( joy2, JOY_DOWN ) && right_paddle_y < HEIGHT - PADDLE_HEIGHT - 1 ) {
                        right_paddle_y++;
                    }
                }

                ++update_counter;
                if( update_counter > (update_speed>>1) )
                {
                    update_counter = 0;
                    if(debug_on) { BORDER_COLOR[0] = COLOR_GREEN; }
                    update();
                }

                if( debug_on ) { BORDER_COLOR[0] = COLOR_LIGHTBLUE; }
                draw();

                /// wait for vertical blank
                if( debug_on ) { BORDER_COLOR[0] = COLOR_BLACK; }
                waitvsync();
            }

            clrscr();
            gotoxy(0, 6);
            printf("\nGame Over!\n");
            if (left_score == 5)
                printf("Left Player Wins!\n");
            else
                printf("Right Player Wins!\n");

            drawMainMenu();
        }

    }

    return 0;
}

void initialize() {
    reset_ball();
    
    left_paddle_y = HEIGHT / 2 - PADDLE_HEIGHT / 2;
    right_paddle_y = HEIGHT / 2 - PADDLE_HEIGHT / 2;

    reset_screen();
    gotoxy(1, 0);
    printf("C64 Pong by JL'25.  Keys: W, S, I, K");
    gotoxy(0, HEIGHT);
    printf("Left Score: %d", left_score);
    gotoxy(WIDTH-16, HEIGHT);
    printf("Right Score: %d", right_score);
}

void reset_screen()
{
    int x;
    int y;
    char* screen;
    screen = (void*)0x400;
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            if (x == ball_x && y == ball_y) {
                screen[x] = BALL_SHAPE;
            } else if ((x == 1 && y >= left_paddle_y && y < left_paddle_y + PADDLE_HEIGHT)) {
                screen[x] = LEFT_PADDLE_SHAPE;
            } else if ((x == WIDTH - 2 && y >= right_paddle_y && y < right_paddle_y + PADDLE_HEIGHT)) {
                screen[x] = RIGHT_PADDLE_SHAPE;
            } else if (y == 1) {
                screen[x] = 121;
            } else if (y == HEIGHT - 1) {
                screen[x] = 120;
            } else if (x == 0 || x == WIDTH - 1) {
                screen[x] = ' ';
            }
            else
            {
                screen[x] = BACKGROUND_SHAPE;
            }
        }
        screen += WIDTH;
    }
}

void draw() {
    char* screen;
    screen = (void*)0x400;

    screen[old_ball_y*40+old_ball_x] = ' ';
    screen[ball_y*40+ball_x] = BALL_SHAPE;

    if( old_left_paddle_y < left_paddle_y)
    {
        screen[(old_left_paddle_y)*40+1] = BACKGROUND_SHAPE;
        screen[(left_paddle_y+PADDLE_HEIGHT-1)*40+1] = LEFT_PADDLE_SHAPE;
    }
    else if( old_left_paddle_y > left_paddle_y)
    {
        screen[(old_left_paddle_y+PADDLE_HEIGHT-1)*40+1] = BACKGROUND_SHAPE;
        screen[(left_paddle_y)*40+1] = LEFT_PADDLE_SHAPE;
    }

    if( old_right_paddle_y < right_paddle_y)
    {
        screen[(old_right_paddle_y)*40+WIDTH - 2] = BACKGROUND_SHAPE;
        screen[(right_paddle_y+PADDLE_HEIGHT-1)*40+WIDTH - 2] = RIGHT_PADDLE_SHAPE;
    }
    else if( old_right_paddle_y > right_paddle_y)
    {
        screen[(old_right_paddle_y+PADDLE_HEIGHT-1)*40+WIDTH - 2] = BACKGROUND_SHAPE;
        screen[(right_paddle_y)*40+WIDTH - 2] = RIGHT_PADDLE_SHAPE;
    }
}

void updateScore()
{
    gotoxy(12, HEIGHT);
    printf("%d", left_score);
    gotoxy(WIDTH-3, HEIGHT);
    printf("%d", right_score);
}

void update() {
    int new_ball_x;
    int new_ball_y;
    old_ball_x = ball_x;
    old_ball_y = ball_y;
    new_ball_x = ball_x + ball_dx;
    new_ball_y = ball_y + ball_dy;

    // Ball collision with top and bottom
    if (new_ball_y <= 2 || new_ball_y >= HEIGHT - 2) {
        ball_dy *= -1;
        playPingSound();
    }

    // Ball collision with paddles
    if ((new_ball_x == 2 && new_ball_y >= left_paddle_y-1 && new_ball_y < left_paddle_y + PADDLE_HEIGHT+1) ||
        (new_ball_x == WIDTH - 3 && new_ball_y >= right_paddle_y-1 && new_ball_y < right_paddle_y + PADDLE_HEIGHT+1)) {
        ball_dx *= -1;
        if( update_speed > 0 )  {
            update_speed--;
        }
        playPingSound();
    }

    // Update ball position
    if (new_ball_x >= 0 && new_ball_x < WIDTH) {
        ball_x = new_ball_x;
        ball_y = new_ball_y;
    } else {
        // Score update and reset ball
        if (new_ball_x <= 0) {
            right_score++;
            reset_ball();
            updateScore();
            playCrashSound();
        } else if (new_ball_x >= WIDTH - 1) {
            left_score++;
            reset_ball();
            updateScore();
            playCrashSound();
        }
    }
}

void reset_ball() {
    int dir;
    ball_x = WIDTH / 2;
    ball_y = HEIGHT / 2;

    // Randomize initial direction
    dir = rand() % 2 ? 1 : -1;
    ball_dx = dir;
    ball_dy = (rand() % 2) * 2 - 1; // Randomly up or down
}
