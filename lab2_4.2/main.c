/**
 * ----------------------------------------------------------------------------
 * main.c
 * Author: Carl Larsson
 * Description: Print accelerometer etc on LCD screen
 * Date: 2023-09-06
 * ----------------------------------------------------------------------------
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/adc.h"
#include "grlib/grlib.h"

#include "utils/uartstdio.c"
#include "drivers/buttons.h"
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
// Check if two integers are of the same length (if they are of the same power of 10)
int same_length(uint32_t a, uint32_t b)
{
    // Need to handle special case of when one is 0, since 1 = 1*10^1 and 0 is undefined, resulting in 0 and 1 to 9 not being considered the same length, even though they are
    if((a<10) && (b<10))
    {
        return 1;
    }

    // Reduce by a power of 10^1 each time, until one (or both) is 0
    while((a>0) && (b>0))
    {
        a = a/10;
        b = b/10;
    }

    // If both a 0, then they are of the same length (except the special case of 0 and 1 to 9)
    if((a == 0) && (b == 0))
    {
        return 1;
    }

    return 0;
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
    tContext context;

    // see https://www.ti.com/lit/ug/spmu300e/spmu300e.pdf?ts=1693897900634&ref_url=https%253A%252F%252Fwww.startpage.com%252F page 269
    uint32_t background_color = ClrBlueViolet;
    uint32_t background_color_text = ClrSeashell;
    // ui32Value is the 24-bit RGB color.  The least-significant byte is the
    // blue channel, the next byte is the green channel, and the third byte is the
    // red channel.

    char itoa_buf [10];

    uint32_t acc_x = 0;
    uint32_t acc_y = 0;
    uint32_t acc_z = 0;

    uint32_t joy_x = 0;
    uint32_t joy_y = 0;

    uint32_t microphone_value = 0;

    uint32_t num_samples = 0;

    uint32_t total_val_acc_x = 0;
    uint32_t total_val_acc_y = 0;
    uint32_t total_val_acc_z = 0;
    uint32_t total_val_joy_x = 0;
    uint32_t total_val_joy_y = 0;
    uint32_t total_val_micro = 0;

    uint32_t print_acc_x = 0;
    uint32_t print_acc_y = 0;
    uint32_t print_acc_z = 0;
    uint32_t print_joy_x = 0;
    uint32_t print_joy_y = 0;
    uint32_t print_micro = 0;

    // Run from the PLL at 40 MHz (needs to be 2*15MHz for SSIConfigSetExpClk(); to work).
    systemClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 40000000);

    // Configure the device pins.
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
    // Sets text background color.
    GrContextBackgroundSet(&context, background_color_text);
    //-----------------------------------------------------------------------------
    // Accelerometer (gyroscope?)
    // Enable the ADC0 module.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))
    {
    }
    // Accelerometer (gyroscope?) is on: X (PE1), Y (PE2), Z (PE0).
    // But should be on: X (PE0), Y (PE1), Z (PE2).
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    // Accelerometer (gyroscope) X-axis (PE1)
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1);

    // Enables trigger from ADC on the GPIO pin.
    // Enable the first sample sequencer to capture the value of the channel when
    // the processor trigger occurs.
    // Channel 2 is X-axis, channel 1 is Y-axis, channel 3 is Z-axis.
    // However, channel 3 should be X-axis, channel 2 should be Y-axis and channel 1 should be Z-axis.
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH2);
    ADCSequenceEnable(ADC0_BASE, 0);
    //-----------------------------------------------------------------------------
    // Potentiometer (joystick) and Microphone
    // Enable the ADC1 module.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC1))
    {
    }
    // Microphone (PE3), joystick horizontal (PE4), joystick vertical (PE5).
    // However, should be Microphone (PE5), joystick horizontal (PE4), joystick vertical (PE3).
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    //  Joystick vertical (PE3).
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    // Enables trigger from ADC on the GPIO pin.
    // Enable the first sample sequencer to capture the value of the channel when
    // the processor trigger occurs.
    // Channel 9 is microphone, channel 0 is joystick horizontal, channel 8 is joystick vertical.
    // However, should be channel 8 is microphone, channel 9 is joystick horizontal, channel 0 is joystick vertical.
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH9);
    ADCSequenceEnable(ADC1_BASE, 0);
    //-----------------------------------------------------------------------------

    while (1)
    {
        // Used for taking average over 10 samples.
        num_samples++;

        //-----------------------------------------------------------------------------
        // Accelerometer (Gyroscope?) X-axis
        // Switch to PE1 (Accelerometer X-axis).
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1);
        // Switch ADC0 to channel 2 for Accelerometer (gyroscope?) X-axis.
        ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH2);
        // Wait for Accelerometer (gyroscope) X-axis (PE1) value.
        ADCProcessorTrigger(ADC0_BASE, 0);
        while(!ADCIntStatus(ADC0_BASE, 0, false))
        {
        }
        ADCSequenceDataGet(ADC0_BASE, 0, &acc_x);
        // Used for average value.
        total_val_acc_x = total_val_acc_x + acc_x;
        //-----------------------------------------------------------------------------
        // Accelerometer (Gyroscope?) Y-axis
        // Switch to PE2 (Accelerometer Y-axis).
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_2);
        // Switch ADC0 to channel 1 for Accelerometer (gyroscope?) Y-axis.
        ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH1);
        // Wait for Accelerometer (gyroscope) Y-axis (PE2) value.
        ADCProcessorTrigger(ADC0_BASE, 0);
        while (!ADCIntStatus(ADC0_BASE, 0, false))
        {
        }
        ADCSequenceDataGet(ADC0_BASE, 0, &acc_y);
        // Used for average value.
        total_val_acc_y = total_val_acc_y + acc_y;
        //-----------------------------------------------------------------------------
        // Accelerometer (Gyroscope?) Z-axis
        // Switch to PE0 (Accelerometer Z-axis).
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);
        // Switch ADC0 to channel 3 for Accelerometer (gyroscope?) Z-axis.
        ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH3);
        // Wait for Accelerometer (gyroscope) Z-axis (PE0) value.
        ADCProcessorTrigger(ADC0_BASE, 0);
        while (!ADCIntStatus(ADC0_BASE, 0, false))
        {
        }
        ADCSequenceDataGet(ADC0_BASE, 0, &acc_z);
        // Used for average value.
        total_val_acc_z = total_val_acc_z + acc_z;
        //-----------------------------------------------------------------------------

        //-----------------------------------------------------------------------------
        // Joystick horizontal
        // Switch to PE4 (Joystick horizontal).
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);
        // Switch ADC1 to channel 0 for joystick horizontal.
        ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH0);
        // Wait for joystick horizontal.
        ADCProcessorTrigger(ADC1_BASE, 0);
        while (!ADCIntStatus(ADC1_BASE, 0, false))
        {
        }
        ADCSequenceDataGet(ADC1_BASE, 0, &joy_x);
        // Used for average value.
        total_val_joy_x = total_val_joy_x + joy_x;
        //-----------------------------------------------------------------------------
        // Joystick vertical
        // Switch to PE5 (Joystick vertical).
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_5);
        // Switch ADC1 to channel 8 for joystick vertical.
        ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH8);
        // Wait for joystick vertical.
        ADCProcessorTrigger(ADC1_BASE, 0);
        while (!ADCIntStatus(ADC1_BASE, 0, false))
        {
        }
        ADCSequenceDataGet(ADC1_BASE, 0, &joy_y);
        // Used for average value.
        total_val_joy_y = total_val_joy_y + joy_y;
        //-----------------------------------------------------------------------------

        //-----------------------------------------------------------------------------
        // Microphone
        // Switch to PE3 (Microphone).
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
        // Switch ADC1 to channel 9 for microphone.
        ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH9);
        // Wait for microphone.
        ADCProcessorTrigger(ADC1_BASE, 0);
        while (!ADCIntStatus(ADC1_BASE, 0, false))
        {
        }
        ADCSequenceDataGet(ADC1_BASE, 0, &microphone_value);
        // Used for average value.
        total_val_micro = total_val_micro + microphone_value;
        //-----------------------------------------------------------------------------

        // Take average over 10 samples.
        if(num_samples >= 200)
        {
            //-----------------------------------------------------------------------------
            // Accelerometer X-axis
            print_acc_x = total_val_acc_x / num_samples;
            GrStringDrawCentered(&context, "Accelerometer X:", -1, 50, 8, 1);
            GrStringDrawCentered(&context, "    ", -1, 110, 8, 1);
            GrStringDrawCentered(&context, itoa(print_acc_x, itoa_buf, 10), -1, 110, 8, 1);
            total_val_acc_x = 0;
            //-----------------------------------------------------------------------------
            // Accelerometer Y-axis
            print_acc_y = total_val_acc_y / num_samples;
            GrStringDrawCentered(&context, "Accelerometer Y:", -1, 50, 18, 1);
            GrStringDrawCentered(&context, "    ", -1, 110, 18, 1);
            GrStringDrawCentered(&context, itoa(print_acc_y, itoa_buf, 10), -1, 110, 18, 1);
            total_val_acc_y = 0;
            //-----------------------------------------------------------------------------
            // Accelerometer Z-axis
            print_acc_z = total_val_acc_z / num_samples;
            GrStringDrawCentered(&context, "Accelerometer Z:", -1, 50, 28, 1);
            GrStringDrawCentered(&context, "    ", -1, 110, 28, 1);
            GrStringDrawCentered(&context, itoa(print_acc_z, itoa_buf, 10), -1, 110, 28, 1);
            total_val_acc_z = 0;
            //-----------------------------------------------------------------------------
            // Joystick horizontal
            print_joy_x = total_val_joy_x / num_samples;
            GrStringDrawCentered(&context, "joystick      X:", -1, 50, 38, 1);
            GrStringDrawCentered(&context, "    ", -1, 110, 38, 1);
            GrStringDrawCentered(&context, itoa(print_joy_x, itoa_buf, 10), -1, 110, 38, 1);
            total_val_joy_x = 0;
            //-----------------------------------------------------------------------------
            // Joystick vertical
            print_joy_y = total_val_joy_y / num_samples;
            GrStringDrawCentered(&context, "joystick      Y:", -1, 50, 48, 1);
            GrStringDrawCentered(&context, "    ", -1, 110, 48, 1);
            GrStringDrawCentered(&context, itoa(print_joy_y, itoa_buf, 10), -1, 110, 48, 1);
            total_val_joy_y = 0;
            //-----------------------------------------------------------------------------
            // Microphone
            print_micro = total_val_micro / num_samples;
            GrStringDrawCentered(&context, "Microphone     :", -1, 50, 58, 1);
            GrStringDrawCentered(&context, "    ", -1, 110, 58, 1);
            GrStringDrawCentered(&context, itoa(print_micro, itoa_buf, 10), -1, 110, 58, 1);
            total_val_micro = 0;
            //-----------------------------------------------------------------------------

            num_samples = 0;
        }
    }
}
//=============================================================================
