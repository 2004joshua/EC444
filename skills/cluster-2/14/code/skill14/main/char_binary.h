#ifndef CHAR_BINARY_H
#define CHAR_BINARY_H

#include <stdint.h>

#define CHAR_A  0b01110111
#define CHAR_B  0b01111100
#define CHAR_C  0b00111001
#define CHAR_D  0b01011110
#define CHAR_E  0b01111001
#define CHAR_F  0b01110001
#define CHAR_G  0b00111101
#define CHAR_H  0b01110110
#define CHAR_I  0b00110000
#define CHAR_J  0b00011110
#define CHAR_K  0b01110101
#define CHAR_L  0b00111000
#define CHAR_M  0b00010101
#define CHAR_N  0b00110111
#define CHAR_O  0b00111111
#define CHAR_P  0b01110011
#define CHAR_Q  0b01101011
#define CHAR_R  0b00110011
#define CHAR_S  0b01101101
#define CHAR_T  0b01111000
#define CHAR_U  0b00111110
#define CHAR_V  0b00111110
#define CHAR_W  0b00101010
#define CHAR_X  0b01110110
#define CHAR_Y  0b01101110
#define CHAR_Z  0b01011011

// Define binary values for numbers (0-9)
#define CHAR_0  0b00111111
#define CHAR_1  0b00000110
#define CHAR_2  0b01011011
#define CHAR_3  0b01001111
#define CHAR_4  0b01100110
#define CHAR_5  0b01101101
#define CHAR_6  0b01111101
#define CHAR_7  0b00000111
#define CHAR_8  0b01111111
#define CHAR_9  0b01101111

// Define binary values for other common characters
#define CHAR_SPACE      0b00000000
#define CHAR_DASH       0b01000000
#define CHAR_UNDERSCORE 0b00001000
#define CHAR_DOT        0b10000000

// Function to convert a char to its binary representation
static inline uint16_t char_to_display(char c) {
    switch (c) {
        case 'A': return CHAR_A;
        case 'B': return CHAR_B;
        case 'C': return CHAR_C;
        case 'D': return CHAR_D;
        case 'E': return CHAR_E;
        case 'F': return CHAR_F;
        case 'G': return CHAR_G;
        case 'H': return CHAR_H;
        case 'I': return CHAR_I;
        case 'J': return CHAR_J;
        case 'K': return CHAR_K;
        case 'L': return CHAR_L;
        case 'M': return CHAR_M;
        case 'N': return CHAR_N;
        case 'O': return CHAR_O;
        case 'P': return CHAR_P;
        case 'Q': return CHAR_Q;
        case 'R': return CHAR_R;
        case 'S': return CHAR_S;
        case 'T': return CHAR_T;
        case 'U': return CHAR_U;
        case 'V': return CHAR_V;
        case 'W': return CHAR_W;
        case 'X': return CHAR_X;
        case 'Y': return CHAR_Y;
        case 'Z': return CHAR_Z;
        case '0': return CHAR_0;
        case '1': return CHAR_1;
        case '2': return CHAR_2;
        case '3': return CHAR_3;
        case '4': return CHAR_4;
        case '5': return CHAR_5;
        case '6': return CHAR_6;
        case '7': return CHAR_7;
        case '8': return CHAR_8;
        case '9': return CHAR_9;
        case '-': return CHAR_DASH;
        case '_': return CHAR_UNDERSCORE;
        case '.': return CHAR_DOT;
        case ' ': return CHAR_SPACE;
        default: return CHAR_SPACE;  // Blank for unsupported characters
    }
}

#endif