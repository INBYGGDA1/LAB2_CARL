/**
 * ----------------------------------------------------------------------------
 * main.c
 * Author: Carl Larsson
 * Description: asteroid destroyer game
 * Date: 2023-09-15
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
    uint32_t ship_color = ClrWhite;
    uint32_t laser_color = ClrRed;
    uint32_t asteroid_color = ClrDimGray;
    uint32_t background_color_text = ClrRed;
    // ui32Value is the 24-bit RGB color.  The least-significant byte is the
    // blue channel, the next byte is the green channel, and the third byte is the
    // red channel.
    //-----------------------------------------------------------------------------
    // Player ship
    tRectangle ship_rectangle;
    int16_t ship_size = 9;
    int16_t ship_speed = 4;
    //-----------------------------------------------------------------------------
    tRectangle laser_rectangle;
    int16_t laser_height = 9;
    int16_t laser_width = 3;
    int16_t laser_speed = 5;
    int16_t laser_active = 0;
    //-----------------------------------------------------------------------------
    // Asteroid
    tRectangle asteroid_rectangle;
    int16_t asteroid_size = 9;
    int16_t asteroid_speed = 5;
    // Matrix for all the asteroid XMin and YMin coordinates, last value is 1 if the asteroid has not been destroyed
    int16_t asteroid_matrix [24][2] = {{-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10},
                                       {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10},
                                       {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}};
    //-----------------------------------------------------------------------------

    uint32_t joystick_val_hor;
    int32_t button_value;

    int16_t i;

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
    GrContextForegroundSet(&context, ship_color);
    // Sets text background color behind text.
    GrContextBackgroundSet(&context, background_color_text);
    //-----------------------------------------------------------------------------
    // HORIZONTAL
    // Enable the ADC1 module.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC1))
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
    //-----------------------------------------------------------------------------
    // Booster button
    // Enable the GPIO port that is used for the on-board LED.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
    // Check if the peripheral access is enabled.
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL))
    {
        ;
    }
    // button is on (PL2)
    GPIOPinTypeGPIOInput(GPIO_PORTL_BASE, GPIO_PIN_2);
    //-----------------------------------------------------------------------------

    // Infinite loop
    while(1)
    {
        // Clears/redraws the screen.
        CF128x128x16_ST7735SClear(background_color);
        // Draw player at bottom mid
        ship_rectangle.i16XMin = 60;
        ship_rectangle.i16YMin = 122 - ship_size;
        ship_rectangle.i16XMax = ship_rectangle.i16XMin + ship_size;
        ship_rectangle.i16YMax = 122;
        GrContextForegroundSet(&context, ship_color);
        GrRectFill(&context, &ship_rectangle);

        for(i=0 ; i<24 ; i++)
        {
            // X, random start x-value
            asteroid_matrix[i][0] = roundf(((128.0-asteroid_size) / 32767.0) * rand());
            // Y, give a random - y-value to make them not appear all at the same time
            asteroid_matrix[i][1] = -roundf((1000.0 / 32767.0) * rand());
            // Set all asteroids as not destroyed

            // We do not need to draw the asteroids yet since they are not within the screen yet
        }

        // No laser has been shot
        laser_active = 0;
        // These are set to avoid values from last loop persisting and having an effect on the new loop
        ADCSequenceDataGet(ADC1_BASE, 0, &joystick_val_hor);
        joystick_val_hor = 50;

        // Loop for one round
        while(1)
        {
            //-----------------------------------------------------------------------------
            // HORIZONTAL
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
            // Read button value
            // Button is on PL2
            button_value = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_2);
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Ship controls
            //-----------------------------------------------------------------------------
            // Move right
            if ((joystick_val_hor > 60) && (ship_rectangle.i16XMax < 128))
            {
                // Character has moved, remove old location
                GrContextForegroundSet(&context, background_color);
                GrRectFill(&context, &ship_rectangle);
                // Update position
                ship_rectangle.i16XMin = ship_rectangle.i16XMin + ship_speed;
                ship_rectangle.i16XMax = ship_rectangle.i16XMin + ship_size;
                // Character has moved, remove old location
                GrContextForegroundSet(&context, ship_color);
                GrRectFill(&context, &ship_rectangle);
            }
            //-----------------------------------------------------------------------------
            // Move left
            else if ((joystick_val_hor < 40) && (ship_rectangle.i16XMin > 0))
            {
                // Character has moved, remove old location
                GrContextForegroundSet(&context, background_color);
                GrRectFill(&context, &ship_rectangle);
                // Update position
                ship_rectangle.i16XMin = ship_rectangle.i16XMin - ship_speed;
                ship_rectangle.i16XMax = ship_rectangle.i16XMin + ship_size;
                // Character has moved, remove old location
                GrContextForegroundSet(&context, ship_color);
                GrRectFill(&context, &ship_rectangle);
            }
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Laser
            //-----------------------------------------------------------------------------
            // Shoot laser
            // Button PL2 is on pin 2 which is bit 2 which in binary is 00000100 which is 4 in decimal
            if ((button_value != 4) && (laser_active == 0))
            {
                // Laser spawns in the middle front of the ship
                laser_rectangle.i16XMin = ship_rectangle.i16XMin + floor(ship_size / 2.0);
                laser_rectangle.i16YMin = ship_rectangle.i16YMin - (laser_height+1);
                laser_rectangle.i16XMax = laser_rectangle.i16XMin + laser_width;
                laser_rectangle.i16YMax = laser_rectangle.i16YMin + laser_height;
                // Draw laser
                GrContextForegroundSet(&context, laser_color);
                GrRectFill(&context, &laser_rectangle);
                // Redraw ship since laser spawns on ship and overwrites part of ship
                GrContextForegroundSet(&context, ship_color);
                GrRectFill(&context, &ship_rectangle);

                // A laser has been shot and is active
                laser_active = 1;
            }
            //-----------------------------------------------------------------------------
            // Updates laser position
            else if (laser_active == 1)
            {
                // First remove old laser position
                GrContextForegroundSet(&context, background_color);
                GrRectFill(&context, &laser_rectangle);
                // Update laser position, only moves in y-axis
                laser_rectangle.i16YMin = laser_rectangle.i16YMin - laser_speed;
                laser_rectangle.i16YMax = laser_rectangle.i16YMin + laser_height;
                // Draw laser
                GrContextForegroundSet(&context, laser_color);
                GrRectFill(&context, &laser_rectangle);
            }
            //-----------------------------------------------------------------------------
            // If laser goes outside screen, it despawns
            if(laser_rectangle.i16YMax < 0)
            {
                // The laser is no longer active
                laser_active = 0;
            }
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Asteroids
            //-----------------------------------------------------------------------------
            // Loop through the asteroid matrix
            for(i=0 ; i<24 ; i++)
            {
                //-----------------------------------------------------------------------------
                // Update position
                // First check if asteroid coordinates are on screen, otherwise we don't need to remove old position from screen
                if((asteroid_matrix[i][1] + asteroid_size) > (0-asteroid_size))
                {
                    // First remove old position
                    asteroid_rectangle.i16XMin = asteroid_matrix[i][0];
                    asteroid_rectangle.i16YMin = asteroid_matrix[i][1];
                    asteroid_rectangle.i16XMax = asteroid_rectangle.i16XMin + asteroid_size;
                    asteroid_rectangle.i16YMax = asteroid_rectangle.i16YMin + asteroid_size;
                    // Redraw old position with background color (erasing old position)
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &asteroid_rectangle);
                }

                // Update position, it only moves in y-axis
                asteroid_matrix[i][1] = asteroid_matrix[i][1] + asteroid_speed;
                asteroid_rectangle.i16YMin = asteroid_matrix[i][1];
                asteroid_rectangle.i16YMax = asteroid_rectangle.i16YMin + asteroid_size;

                // We only need to draw new position if it is on screen
                if((asteroid_matrix[i][1] + asteroid_size) > (0-asteroid_size))
                {
                    // Draw new position
                    GrContextForegroundSet(&context, asteroid_color);
                    GrRectFill(&context, &asteroid_rectangle);
                }
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Asteroid hits ship
                if(GrRectOverlapCheck(&asteroid_rectangle, &ship_rectangle))
                {
                    // Despawn laser
                    laser_active = 0;

                    // Set the color for pixels drawn
                    GrContextForegroundSet(&context, ship_color);
                    // Sets text background color behind text.
                    GrContextBackgroundSet(&context, background_color_text);
                    GrStringDrawCentered(&context, "Defeat", -1, 64, 80, 1);

                    // This function provides a means of generating a constant length
                    // delay.  The function delay (in cycles) = 3 * parameter.  Delay
                    // 0.125 seconds.
                    MAP_SysCtlDelay(systemClock / 2);

                    // Exit the inner while loop that makes up one round
                    goto game_lost;
                }
                //-----------------------------------------------------------------------------

                //-----------------------------------------------------------------------------
                // Respawn
                // If asteroid disappears down on screen
                if((asteroid_matrix[i][1] > 128))
                {
                    // Respawn asteroid
                    // X, random start x-value
                    asteroid_matrix[i][0] = roundf(((128.0-asteroid_size) / 32767.0) * rand());
                    // Y, give a random - y-value to make them not appear all at the same time
                    asteroid_matrix[i][1] = -roundf((1000.0 / 32767.0) * rand());
                }
                // If laser is active and hits an asteroid
                if((laser_active == 1) && (GrRectOverlapCheck(&asteroid_rectangle, &laser_rectangle)))
                {
                    // Despawn asteroid
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &asteroid_rectangle);

                    // Respawn asteroid
                    // X, random start x-value
                    asteroid_matrix[i][0] = roundf(((128.0 - asteroid_size) / 32767.0) * rand());
                    // Y, give a random - y-value to make them not appear all at the same time
                    asteroid_matrix[i][1] = -roundf((1000.0 / 32767.0) * rand());

                    // Despawn laser
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &laser_rectangle);
                    laser_active = 0;
                }
                //-----------------------------------------------------------------------------

            }
            //-----------------------------------------------------------------------------

            // According to the documentation, GrFlush is important to use when drawing pixels, since it ensures any buffered pixels are drawn
            GrFlush(&context);

            // This function provides a means of generating a constant length
            // delay.  The function delay (in cycles) = 3 * parameter.  Delay
            // 0.125 seconds.
            MAP_SysCtlDelay(systemClock / 80);
        }
        // goto statement that breaks the inner while loop for one round when you loose
        game_lost:
        ;
    }
}
//=============================================================================
