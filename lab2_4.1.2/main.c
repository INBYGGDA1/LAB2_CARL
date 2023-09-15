/**
 * ----------------------------------------------------------------------------
 * main.c
 * Author: Carl Larsson
 * Description: breakout game
 * Date: 2023-09-10
 * ----------------------------------------------------------------------------
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/adc.h"
#include "grlib/grlib.h"

#include "utils/uartstdio.c"
#include "drivers/pinout.h"
#include "drivers/CF128x128x16_ST7735S.h"
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//=============================================================================
// The error routine that is called if the driver library
// encounters an error.
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
    while(1);
}
#endif
//=============================================================================
// Configure the UART.
void ConfigureUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTStdioConfig(0, 115200, 16000000);
}
//=============================================================================
// A utility function to reverse a string
void reverse_string(char str[], int length)
{
    char temp;
    int start = 0;
    int end = length - 1;

    // Switch place on everything until we meet in the middle
    while (start < end)
    {
        temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        end--;
        start++;
    }
}
//=============================================================================
// Implementation of itoa()
char* itoa(int num, char* str, int base)
{
    int i = 0;
    bool isNegative = false;

    /* Handle 0 explicitly, otherwise empty string is
     * printed for 0 */
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled
    // only with base 10. Otherwise numbers are
    // considered unsigned.
    if (num < 0 && base == 10) {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    reverse_string(str, i);

    return str;
}
//=============================================================================
// Main Function
int main(void)
{
    ConfigureUART();

    uint32_t systemClock;
    // Run from the PLL at 40 MHz (needs to be 2*15MHz for SSIConfigSetExpClk(); to work).
    systemClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 40000000);

    tContext context;
    //-----------------------------------------------------------------------------
    // LCD Colors
    // see https://www.ti.com/lit/ug/spmu300e/spmu300e.pdf?ts=1693897900634&ref_url=https%253A%252F%252Fwww.startpage.com%252F page 269
    uint32_t background_color = ClrBlack;
    uint32_t racket_ball_color = ClrWhite;
    uint32_t top_row = ClrYellow;
    uint32_t mid_row = ClrLime;
    uint32_t bottom_row = ClrBlueViolet;
    uint32_t background_color_text = ClrRed;
    // ui32Value is the 24-bit RGB color.  The least-significant byte is the
    // blue channel, the next byte is the green channel, and the third byte is the
    // red channel.
    //-----------------------------------------------------------------------------
    // Ball
    tRectangle ball_rectangle;
    int16_t ball_size = 5;
    int16_t ball_speed = 1;
    int16_t num_balls = 5;
    // ball direction works on a unit circle degrees logic, where origin is the ball, so 90 would mean the ball is traveling straight up
    int16_t ball_direction;
    //-----------------------------------------------------------------------------
    // Racket
    int16_t racket_height = 4;
    int16_t racket_width = 20;
    int16_t racket_speed = 2;
    tRectangle bottom_racket;
    //-----------------------------------------------------------------------------
    // Bricks
    int16_t brick_width = 15;
    int16_t brick_height = 5;
    int16_t num_bricks = 24;
    tRectangle brick_rectangle;
    // Matrix for all the brick XMin and YMin coordinates, last value is 1 if the brick has not been destroyed
    int16_t brick_matrix [3][8][3] = {{{1, 15, 1}, {17, 15, 1}, {33, 15, 1}, {49, 15, 1}, {65, 15, 1}, {81, 15, 1}, {97, 15, 1}, {113, 15, 1}},
                                   {{1, 21, 1}, {17, 21, 1}, {33, 21, 1}, {49, 21, 1}, {65, 21, 1}, {81, 21, 1}, {97, 21, 1}, {113, 21, 1}},
                                   {{1, 27, 1}, {17, 27, 1}, {33, 27, 1}, {49, 27, 1}, {65, 27, 1}, {81, 27, 1}, {97, 27, 1}, {113, 27, 1}}};
    //-----------------------------------------------------------------------------

    int16_t i;
    int16_t j;

    uint32_t joystick_val_hor;

    char itoa_buf [10];

    // Configure the device pins (Ethernet and USB).
    PinoutSet(false, false);

    //-----------------------------------------------------------------------------
    // LCD
    //-----------------------------------------------------------------------------
    // Initialize the base LCD driver.
    CF128x128x16_ST7735SInit(systemClock);
    // Clears/redraws the screen.
    CF128x128x16_ST7735SClear(background_color);
    // Initialize the grlib library.
    GrContextInit(&context, &g_sCF128x128x16_ST7735S);
    // Sets text font.
    GrContextFontSet(&context, &g_sFontFixed6x8);
    // Set the color for pixels drawn
    GrContextForegroundSet(&context, racket_ball_color);
    // Sets text background color behind text.
    GrContextBackgroundSet(&context, background_color_text);
    //-----------------------------------------------------------------------------
    // HORIZONTAL
    //-----------------------------------------------------------------------------
    // Enable the ADC1 module.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC1))
    {
    }
    // Enable port E, joystick horizontal is on PE3.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    // Use joystick honrizontal (PE3) for interacting with LCD
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    // Enables trigger from ADC on the GPIO pin PE3 (joystick horizontal)
    // Enable the first sample sequencer to capture the value of channel 9 (PE3) (Should be PE4?) when
    // the processor trigger occurs.
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH9);
    ADCSequenceEnable(ADC1_BASE, 0);

    // Infinite loop
    while(1)
    {
        num_balls = 5;
        num_bricks = 24;
        // Clears/redraws the screen.
        CF128x128x16_ST7735SClear(background_color);

        // Draw racket, start in middle
        bottom_racket.i16XMin = 54;
        bottom_racket.i16YMin = 128-racket_height;
        bottom_racket.i16XMax = bottom_racket.i16XMin + racket_width;
        bottom_racket.i16YMax = 128;
        // Set the color for pixels drawn
        GrContextForegroundSet(&context, racket_ball_color);
        GrRectFill(&context, &bottom_racket);


        // Create and draw all bricks
        for(i = 0; i < 3; i++)
        {
            // First row color
            if(i == 0)
            {
                GrContextForegroundSet(&context, top_row);
            }
            // Second row color
            else if (i == 1)
            {
                GrContextForegroundSet(&context, mid_row);
            }
            // Third row color
            else
            {
                GrContextForegroundSet(&context, bottom_row);
            }
            for(j = 0; j < 8; j++)
            {
                // Set last value back to 1 to mark them as not destroyed again
                brick_matrix[i][j][2] = 1;
                brick_rectangle.i16XMin = brick_matrix[i][j][0];
                brick_rectangle.i16YMin = brick_matrix[i][j][1];
                brick_rectangle.i16XMax = brick_rectangle.i16XMin + brick_width;
                brick_rectangle.i16YMax = brick_rectangle.i16YMin + brick_height;
                GrRectFill(&context, &brick_rectangle);
            }
        }

        // Loop for one game
        while((num_balls > 0) && (num_bricks > 0))
        {
            // Starting position is slightly above the middle of the screen on the Y-axis, but random on X-axis
            ball_rectangle.i16XMin = roundf((128.0 / 32767.0) * rand());
            ball_rectangle.i16YMin = 50;
            ball_rectangle.i16XMax = ball_rectangle.i16XMin + ball_size;
            ball_rectangle.i16YMax = ball_rectangle.i16YMin + ball_size;
            // Set the color for pixels drawn
            GrContextForegroundSet(&context, racket_ball_color);
            GrRectFill(&context, &ball_rectangle);
            // Ball initially moves south west or south east
            if(rand() > roundf(32767.0/2.0))
            {
                ball_direction = 225;
            }
            else
            {
                ball_direction = 315;
            }

            // Loop for not missing ball
            while(1)
            {
                //-----------------------------------------------------------------------------
                // HORIZONTAL
                //-----------------------------------------------------------------------------
                // Wait for joystick trigger and then get value.
                GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
                ADCProcessorTrigger(ADC1_BASE, 0);
                while (!ADCIntStatus(ADC1_BASE, 0, false))
                {
                }
                ADCSequenceDataGet(ADC1_BASE, 0, &joystick_val_hor);

                // Convert joystick values from a 0 to 4095 range down to 0 to 100 range (percentage)
                joystick_val_hor = roundf((100.0 / 4095.0) * joystick_val_hor);
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Ball movement
                //-----------------------------------------------------------------------------
                // First remove old ball position from LCD screen by coloring over it with background color
                GrContextForegroundSet(&context, background_color);
                GrRectFill(&context, &ball_rectangle);
                // Move straight up (north)
                if(ball_direction == 90)
                {
                    // Y goes from 0 at top, to 128 at bottom, thus we reduce to go up
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin - ball_speed;
                }
                // Move north east
                else if (ball_direction == 45)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin + ball_speed;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin - ball_speed;
                }
                // Move north west
                else if (ball_direction == 135)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin - ball_speed;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin - ball_speed;
                }
                // Move straight down (south)
                else if (ball_direction == 180)
                {
                    // Y goes from 0 at top, to 128 at bottom, thus we increase to go down
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin + ball_speed;
                }
                // Move south west
                else if (ball_direction == 225)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin - ball_speed;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin + ball_speed;
                }
                // Move south east
                else if (ball_direction == 315)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin + ball_speed;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin + ball_speed;

                }
                // Update ball position
                ball_rectangle.i16XMax = ball_rectangle.i16XMin + ball_size;
                ball_rectangle.i16YMax = ball_rectangle.i16YMin + ball_size;
                // Set the color for pixels drawn
                GrContextForegroundSet(&context, racket_ball_color);
                // Draw new position
                GrRectFill(&context, &ball_rectangle);
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Racket movement
                //-----------------------------------------------------------------------------
                // Move racket left, unless it is at left corner
                if ((joystick_val_hor < 30) && (bottom_racket.i16XMin > 0))
                {
                    // Remove old position of left racket by drawing over old position with background color
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &bottom_racket);
                    bottom_racket.i16XMin = bottom_racket.i16XMin - racket_speed;
                    bottom_racket.i16XMax = bottom_racket.i16XMin + racket_width;
                }
                // Move racket right, unless it is at right corner
                else if ((joystick_val_hor > 70) && (bottom_racket.i16XMax < 128))
                {
                    // Remove old position of left racket by drawing over old position with background color
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &bottom_racket);
                    bottom_racket.i16XMin = bottom_racket.i16XMin + racket_speed;
                    bottom_racket.i16XMax = bottom_racket.i16XMin + racket_width;
                }
                // Redraw/update racket
                // Set the color for pixels drawn
                GrContextForegroundSet(&context, racket_ball_color);
                GrRectFill(&context, &bottom_racket);

                //-----------------------------------------------------------------------------
                // Racket ball logic
                //-----------------------------------------------------------------------------
                // If ball hits racket
                if(GrRectOverlapCheck(&bottom_racket, &ball_rectangle))
                {
                    // If ball hits left part of racket
                    if (ball_rectangle.i16XMin < (bottom_racket.i16XMin + roundf((1.0/2.0)*racket_width)))
                    {
                        // Set direction to north west
                        ball_direction = 135;
                    }
                    // If ball hits right part of racket
                    else
                    {
                        // Set direction to north east
                        ball_direction = 45;
                    }
                }
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Wall (wall is considered left, top and right part of the screen) ball logic
                //-----------------------------------------------------------------------------
                // Left part of screen
                if(ball_rectangle.i16XMin <= 0)
                {
                    // If ball is moving north west and then hits left side of screen, then it should bounce and change direction to north east
                    if(ball_direction == 135)
                    {
                        ball_direction = 45;
                    }
                    // Else if ball is moving south west and then hits left part of the screen, then it should bounce and change direction to south east
                    else
                    {
                        ball_direction = 315;
                    }
                }
                //-----------------------------------------------------------------------------
                // Top part of screen
                // Y goes from 0 at top to 128 at bottom
                if(ball_rectangle.i16YMin <= 0)
                {
                    // If ball is moving north east and then hits top part of screen, then it should bounce and change direction to south east
                    if(ball_direction == 45)
                    {
                        ball_direction = 315;
                    }
                    // Else if ball is moving north west and then hits top part of screen, then it should bounce and chance direction to south west
                    else
                    {
                        ball_direction = 225;
                    }
                }
                //-----------------------------------------------------------------------------
                // Right part of screen
                if(ball_rectangle.i16XMax >= 128)
                {
                    // If ball is moving north east and then hits right side of screen, then it should bounce and change direction to north west
                    if(ball_direction == 45)
                    {
                        ball_direction = 135;
                    }
                    else
                    {
                        ball_direction = 225;
                    }
                }
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Brick logic
                //-----------------------------------------------------------------------------
                // Check all bricks that haven't been destroyed
                for (i = 0; i < 3; i++)
                {
                    for (j = 0; j < 8; j++)
                    {
                        // If brick hasn't been destroyed
                        if(brick_matrix[i][j][2] == 1)
                        {
                            brick_rectangle.i16XMin = brick_matrix[i][j][0];
                            brick_rectangle.i16YMin = brick_matrix[i][j][1];
                            brick_rectangle.i16XMax = brick_rectangle.i16XMin + brick_width;
                            brick_rectangle.i16YMax = brick_rectangle.i16YMin + brick_height;
                            // Check if brick is hit by ball
                            if(GrRectOverlapCheck(&ball_rectangle, &brick_rectangle))
                            {
                                // Set brick to destroyed
                                brick_matrix[i][j][2] = 0;
                                // Clear hit brick
                                GrContextForegroundSet(&context, background_color);
                                GrRectFill(&context, &brick_rectangle);
                                num_bricks--;

                                //-----------------------------------------------------------------------------
                                // Fix ball bounce on brick
                                //-----------------------------------------------------------------------------
                                if (ball_direction == 45)
                                {
                                    ball_direction = 315;
                                }
                                else if(ball_direction == 135)
                                {
                                    ball_direction = 225;
                                }
                                else if(ball_direction == 225)
                                {
                                    ball_direction = 135;
                                }
                                else if(ball_direction == 315)
                                {
                                    ball_direction = 45;
                                }
                                //-----------------------------------------------------------------------------
                                // Redraw ball
                                GrContextForegroundSet(&context, racket_ball_color);
                                GrRectFill(&context, &ball_rectangle);
                            }
                        }
                    }
                }
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Win/loose logic
                //-----------------------------------------------------------------------------
                // If ball goes down (you miss with racket)
                if(ball_rectangle.i16YMax > 128)
                {
                    // Clear the missed ball
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &ball_rectangle);

                    num_balls--;
                    // Set the color for pixels drawn
                    GrContextForegroundSet(&context, racket_ball_color);
                    // Sets text background color behind text.
                    GrContextBackgroundSet(&context, background_color_text);
                    GrStringDrawCentered(&context, itoa(num_balls, itoa_buf, 10), -1, 64, 80, 1);

                    // This function provides a means of generating a constant length
                    // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                    // 0.125 seconds.
                    MAP_SysCtlDelay(systemClock / 4);

                    // Clear the text just written
                    GrContextForegroundSet(&context, background_color);
                    // Sets text background color behind text.
                    GrContextBackgroundSet(&context, background_color);
                    GrStringDrawCentered(&context, itoa(num_balls, itoa_buf, 10), -1, 64, 80, 1);

                    break;
                }
                // If all bricks have been destroyed
                else if(num_bricks <= 0)
                {
                    // Set the color for pixels drawn
                    GrContextForegroundSet(&context, racket_ball_color);
                    // Sets text background color behind text.
                    GrContextBackgroundSet(&context, background_color_text);
                    GrStringDrawCentered(&context, "Victory", -1, 64, 80, 1);

                    // This function provides a means of generating a constant length
                    // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                    // 0.125 seconds.
                    MAP_SysCtlDelay(systemClock / 2);

                    break;
                }
                //-----------------------------------------------------------------------------

                // According to the documentation, GrFlush is important to use when drawing pixels, since it ensures any buffered pixels are drawn
                GrFlush(&context);

                // This function provides a means of generating a constant length
                // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                // 0.125 seconds.
                MAP_SysCtlDelay(systemClock / 200);
            }
        }
    }
}
//=============================================================================
