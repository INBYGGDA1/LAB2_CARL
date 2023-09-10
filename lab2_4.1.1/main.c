/**
 * ----------------------------------------------------------------------------
 * main.c
 * Author: Carl Larsson
 * Description: Pong game
 * Date: 2023-09-10
 * ----------------------------------------------------------------------------
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/adc.h"
#include "grlib/grlib.h"

#include "utils/uartstdio.c"
#include "drivers/buttons.h"
#include "drivers/pinout.h"
#include "drivers/CF128x128x16_ST7735S.h"
#include "circular_queue.h"
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
    uint32_t pixel_color = ClrWhite;
    uint32_t background_color_text = ClrRed;
    // ui32Value is the 24-bit RGB color.  The least-significant byte is the
    // blue channel, the next byte is the green channel, and the third byte is the
    // red channel.
    //-----------------------------------------------------------------------------
    // Ball
    tRectangle ball_rectangle;
    int16_t ball_size = 5;
    // ball direction works on a unit circle degrees logic, where origin is the ball, so 180 would mean the ball is traveling straight to the left
    int16_t ball_direction = 180;
    //-----------------------------------------------------------------------------
    // Rackets
    int16_t racket_height = 32;
    int16_t racket_width = 4;
    int16_t racket_speed = 8;
    int16_t control_left = 1;
    int16_t control_right = 0;
    //-----------------------------------------------------------------------------
    // Left racket
    tRectangle left_racket;
    //-----------------------------------------------------------------------------
    // Right racket
    tRectangle right_racket;
    //-----------------------------------------------------------------------------
    // Walls
    int16_t wall_height = 4;
    int16_t wall_width = 128;
    //-----------------------------------------------------------------------------
    // Upper wall
    tRectangle upper_wall;
    //-----------------------------------------------------------------------------
    // Lower wall
    tRectangle lower_wall;
    //-----------------------------------------------------------------------------
    // Points
    int16_t left_points = 0;
    int16_t right_points = 0;
    //-----------------------------------------------------------------------------

    uint32_t joystick_val_ver;
    uint32_t joystick_val_hor;

    char itoa_buf [10];

    // Configure the device pins (Ethernet and USB).
    PinoutSet(false, false);

    //-----------------------------------------------------------------------------
    // LCD
    // Initialize the base LCD driver.
    CF128x128x16_ST7735SInit(systemClock);
    // Clears/redraws the screen.
    CF128x128x16_ST7735SClear(background_color);
    // Initialize the grlib library.
    GrContextInit(&context, &g_sCF128x128x16_ST7735S);
    // Sets text font.
    GrContextFontSet(&context, &g_sFontFixed6x8);
    // Set the color for pixels drawn
    GrContextForegroundSet(&context, pixel_color);
    // Sets text background color behind text.
    GrContextBackgroundSet(&context, background_color_text);
    //-----------------------------------------------------------------------------
    // VERTICAL
    // Enable the ADC0 module.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))
    {
    }
    // Enable port E, joystick vertical is on PE4.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    // Use joystick vertical (PE4) for interacting with LCD
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);

    // Enables trigger from ADC on the GPIO pin PE4 (joystick vertical)
    // Enable the first sample sequencer to capture the value of channel 0 (PE4) (Should be PE3?) when
    // the processor trigger occurs.
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH0);
    ADCSequenceEnable(ADC0_BASE, 0);
    //-----------------------------------------------------------------------------
    // HORIZONTAL
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
        left_points = 0;
        right_points = 0;
        // Loop for one game
        while((left_points < 10) && (right_points < 10))
        {
            // Clears/redraws the screen.
            CF128x128x16_ST7735SClear(background_color);
            // Set pixel color to draw with
            GrContextForegroundSet(&context, pixel_color);

            // Draw upper wall
            upper_wall.i16XMin = 0;
            upper_wall.i16YMin = 0;
            upper_wall.i16XMax = wall_width;
            upper_wall.i16YMax = wall_height;
            GrRectFill(&context, &upper_wall);

            // Draw lower wall
            lower_wall.i16XMin = 0;
            lower_wall.i16YMin = 128-wall_height;
            lower_wall.i16XMax = wall_width;
            lower_wall.i16YMax = 128;
            GrRectFill(&context, &lower_wall);

            // Draw left racket, start in middle
            left_racket.i16XMin = 4;
            left_racket.i16YMin = 48;
            left_racket.i16XMax = left_racket.i16XMin + racket_width;
            left_racket.i16YMax = left_racket.i16YMin + racket_height;
            GrRectFill(&context, &left_racket);

            // Draw right racket, start in middle
            right_racket.i16XMin = 128-(racket_width+4);
            right_racket.i16YMin = 48;
            right_racket.i16XMax = 128-4;
            right_racket.i16YMax = right_racket.i16YMin + racket_height;
            GrRectFill(&context, &right_racket);


            // Starting position, in the middle
            ball_rectangle.i16XMin = 62;
            ball_rectangle.i16YMin = 62;
            ball_rectangle.i16XMax = ball_rectangle.i16XMin + ball_size;
            ball_rectangle.i16YMax = ball_rectangle.i16YMin + ball_size;
            GrRectFill(&context, &ball_rectangle);
            // Ball initially moves left
            ball_direction = 180;
            // Initially left racket has control
            // Disable right
            control_right = 0;
            // Enable left
            control_left = 1;

            // Loop for one round
            while(1)
            {
                //-----------------------------------------------------------------------------
                // VERTICAL
                // Wait for joystick trigger and then get value.
                GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);
                ADCProcessorTrigger(ADC0_BASE, 0);
                while (!ADCIntStatus(ADC0_BASE, 0, false))
                {
                }
                ADCSequenceDataGet(ADC0_BASE, 0, &joystick_val_ver);

                joystick_val_ver = roundf((100.0 / 4095.0) * joystick_val_ver);
                //-----------------------------------------------------------------------------
                // HORIZONTAL
                // Wait for joystick trigger and then get value.
                GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
                ADCProcessorTrigger(ADC1_BASE, 0);
                while (!ADCIntStatus(ADC1_BASE, 0, false))
                {
                }
                ADCSequenceDataGet(ADC1_BASE, 0, &joystick_val_hor);

                joystick_val_hor = roundf((100.0 / 4095.0) * joystick_val_hor);
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Ball movement
                //-----------------------------------------------------------------------------
                // First remove old ball position from LCD screen by coloring over it with background color
                GrContextForegroundSet(&context, background_color);
                GrRectFill(&context, &ball_rectangle);
                // Move straight right (east)
                if(ball_direction == 0)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin + ball_size;
                }
                // Move north east
                else if (ball_direction == 45)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin + ball_size;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin - ball_size;
                }
                // Move north west
                else if (ball_direction == 135)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin - ball_size;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin - ball_size;
                }
                // Move straight left (west)
                else if (ball_direction == 180)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin - ball_size;
                }
                // Move south west
                else if (ball_direction == 225)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin - ball_size;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin + ball_size;
                }
                // Move south east
                else if (ball_direction == 315)
                {
                    ball_rectangle.i16XMin = ball_rectangle.i16XMin + ball_size;
                    ball_rectangle.i16YMin = ball_rectangle.i16YMin + ball_size;

                }
                // Update ball position and draw it
                ball_rectangle.i16XMax = ball_rectangle.i16XMin + ball_size;
                ball_rectangle.i16YMax = ball_rectangle.i16YMin + ball_size;
                GrContextForegroundSet(&context, pixel_color);
                GrRectFill(&context, &ball_rectangle);
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Control of racket
                //-----------------------------------------------------------------------------
                // Switch control to left racket
                if(joystick_val_hor < 20)
                {
                    // Disable right
                    control_right = 0;
                    // Enable left
                    control_left = 1;
                }
                // Switch control to right racket
                else if(joystick_val_hor > 80)
                {
                    // Disable left
                    control_left = 0;
                    // Enable right
                    control_right = 1;
                }

                //-----------------------------------------------------------------------------
                // Racket movement
                //-----------------------------------------------------------------------------
                if(control_left == 1)
                {
                    // Move racket up, unless it is at top (Y goes from 0 at top to 128 at bottom)
                    if((joystick_val_ver > 70) && (left_racket.i16YMin > wall_height))
                    {
                        // Remove old position of left racket by drawing over old position with background color
                        GrContextForegroundSet(&context, background_color);
                        GrRectFill(&context, &left_racket);
                        left_racket.i16YMin = left_racket.i16YMin - racket_speed;
                        left_racket.i16YMax = left_racket.i16YMin + racket_height;
                    }
                    // Move racket down, unless it is at bottom (Y goes from 0 at top to 128 at bottom)
                    else if((joystick_val_ver < 30) && (left_racket.i16YMax < (128-wall_height)))
                    {
                        // Remove old position of left racket by drawing over old position with background color
                        GrContextForegroundSet(&context, background_color);
                        GrRectFill(&context, &left_racket);
                        left_racket.i16YMin = left_racket.i16YMin + racket_speed;
                        left_racket.i16YMax = left_racket.i16YMin + racket_height;
                    }
                }
                else if(control_right == 1)
                {
                    // Move racket up, unless it is at top (Y goes from 0 at top to 128 at bottom)
                    if((joystick_val_ver > 70)  && (right_racket.i16YMin > wall_height))
                    {
                        // Remove old position of right racket by drawing over old position with background color
                        GrContextForegroundSet(&context, background_color);
                        GrRectFill(&context, &right_racket);
                        right_racket.i16YMin = right_racket.i16YMin - racket_speed;
                        right_racket.i16YMax = right_racket.i16YMin + racket_height;
                    }
                    // Move racket down, unless it is at bottom (Y goes from 0 at top to 128 at bottom)
                    else if((joystick_val_ver < 30) && (right_racket.i16YMax < (128-wall_height)))
                    {
                        // Remove old position of right racket by drawing over old position with background color
                        GrContextForegroundSet(&context, background_color);
                        GrRectFill(&context, &right_racket);
                        right_racket.i16YMin = right_racket.i16YMin + racket_speed;
                        right_racket.i16YMax = right_racket.i16YMin + racket_height;
                    }

                }
                // Redraw/update left racket
                GrContextForegroundSet(&context, pixel_color);
                GrRectFill(&context, &left_racket);
                // Redraw/update right racket
                GrContextForegroundSet(&context, pixel_color);
                GrRectFill(&context, &right_racket);

                //-----------------------------------------------------------------------------
                // Racket ball logic
                //-----------------------------------------------------------------------------
                // If ball hits left racket
                if(GrRectOverlapCheck(&left_racket, &ball_rectangle))
                {
                    // If ball hits top part of racket
                    if (ball_rectangle.i16YMin > left_racket.i16YMin + roundf((2.0/3.0)*racket_height))
                    {
                        // Set direction to north east
                        //ball_direction = 45;
                        ball_direction = 315;
                    }
                    // If ball hits lower part of racket
                    else if (ball_rectangle.i16YMin < left_racket.i16YMin + roundf((1.0/3.0)*racket_height))
                    {
                        // Set direction to south east
                        //ball_direction = 315;
                        ball_direction = 45;
                    }
                    // If ball hits middle of racket
                    else
                    {
                        // Set direction to straight right (east)
                        ball_direction = 0;
                    }
                }
                //-----------------------------------------------------------------------------
                // If ball hits right racket
                if (GrRectOverlapCheck(&right_racket, &ball_rectangle))
                {
                    // If ball hits top part of racket
                    if (ball_rectangle.i16YMin > left_racket.i16YMin + roundf((2.0/3.0)*racket_height))
                    {
                        // Set direction to north west
                        //ball_direction = 135;
                        ball_direction = 225;
                    }
                    // If ball hits lower part of racket
                    else if (ball_rectangle.i16YMin < left_racket.i16YMin + roundf((1.0/3.0)*racket_height))
                    {
                        // Set direction to south west
                        //ball_direction = 225;
                        ball_direction = 135;
                    }
                    // If ball hits middle of racket
                    else
                    {
                        // Set direction to straight left (west)
                        ball_direction = 180;
                    }
                }
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Wall ball logic
                //-----------------------------------------------------------------------------
                // Upper (Top) wall
                if(GrRectOverlapCheck(&upper_wall, &ball_rectangle))
                {
                    // If ball is moving north east and then hits upper wall, then it should bounce and change direction to south east
                    if(ball_direction == 45)
                    {
                        ball_direction = 315;
                    }
                    // Else if ball is moving north west and then hits upper wall, then it should bounce and change direction to south west
                    else
                    {
                        ball_direction = 225;
                    }
                }
                //-----------------------------------------------------------------------------
                // Lower (Bottom) wall
                if(GrRectOverlapCheck(&lower_wall, &ball_rectangle))
                {
                    // If ball is moving south east and then hits lower wall, then it should bounce and change direction to north east
                    if(ball_direction == 315)
                    {
                        ball_direction = 45;
                    }
                    // Else if ball is moving south west and then hits lower wall, then it should bounce and chance direction to north west
                    else
                    {
                        ball_direction = 135;
                    }
                }
                // Redrawn wall incase ball has hit it
                GrContextForegroundSet(&context, pixel_color);
                GrRectFill(&context, &upper_wall);
                // Redrawn wall incase ball has hit it
                GrContextForegroundSet(&context, pixel_color);
                GrRectFill(&context, &lower_wall);
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Goal logic
                //-----------------------------------------------------------------------------
                // Goal on left
                if(ball_rectangle.i16XMax < 0)
                {
                    left_points++;
                    GrStringDrawCentered(&context, itoa(left_points, itoa_buf, 10), -1, 44, 64, 1);
                    GrStringDrawCentered(&context, itoa(right_points, itoa_buf, 10), -1, 84, 64, 1);

                    // This function provides a means of generating a constant length
                    // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                    // 0.125 seconds.
                    MAP_SysCtlDelay(systemClock / 2);

                    break;
                }
                // Goal on right
                else if(ball_rectangle.i16XMax > 128)
                {
                    right_points++;
                    GrStringDrawCentered(&context, itoa(left_points, itoa_buf, 10), -1, 44, 64, 1);
                    GrStringDrawCentered(&context, itoa(right_points, itoa_buf, 10), -1, 84, 64, 1);

                    // This function provides a means of generating a constant length
                    // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                    // 0.125 seconds.
                    MAP_SysCtlDelay(systemClock / 2);

                    break;
                }

                // According to the documentation, GrFlush is important to use when drawing pixels, since it ensures any buffered pixels are drawn
                GrFlush(&context);

                // This function provides a means of generating a constant length
                // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                // 0.125 seconds.
                MAP_SysCtlDelay(systemClock / 40);
            }
        }
    }
}
//=============================================================================
