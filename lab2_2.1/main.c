/**
 * ----------------------------------------------------------------------------
 * main.c
 * Author: Carl Larsson
 * Description: User decide LED brightness with help of PWM and buttons
 * Date: 2023-09-02
 * ----------------------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/rom_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/sysctl.h"
#include "drivers/buttons.h"
#include "drivers/pinout.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
// Causes redefinition warnings
//#include "inc/tm4c129encpdt.h"
#include "utils/uartstdio.c"


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
// Configurations
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
// Main
int main(void)
{
    ConfigureUART();
    float pwm_word;
    uint32_t systemClock;
    unsigned char ucDelta, ucState;
    volatile uint8_t k=50;

    // Run from the PLL at 16000 Hz..
    systemClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 16000);
    // Use a local variable to store the PWM clock rate which will be: 16000Hz / 200 = 80 Hz. This variable will be used to set the PWM generator period.
    pwm_word = systemClock / 200;

    // Enable the GPIO port that is used for the PWM output.
    // Enable the GPIO port that is used for the on-board LED.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    // Set the PWM clock to be SysClk / 1.
    SysCtlPWMClockSet(SYSCTL_PWMDIV_1);
    // Reset and enable PWM peripheral?
    SysCtlPeripheralDisable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_PWM0);
    // The PWM peripheral must be enabled for use.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    // Configure the GPIO pin for PWM function (PF2)
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
    GPIOPinConfigure(GPIO_PF2_M0PWM2);

    // Configure PWM2 to count up/down without synchronization.
    // PWN_GEN_0 is connected to PWM 0 and 1, PWN_GEN_1 is connected to PWM 2 and 3, PWN_GEN_2 is connected to PWM 4 and 5, PWN_GEN_3 is connected to PWM 6 and 7
    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC | PWM_GEN_MODE_DBG_RUN);
    // Set the PWM period to 80Hz?
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, pwm_word);
    // Set PWM2 to a duty cycle of 50 %.
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, (pwm_word/100)*50);
    // Enable the PWM generator block.
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
    // Enable the PWM Out Bit 2 (PF2) as output signal.
    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);

    // Configure the device pins.
    PinoutSet(false, false);

    // Initialize the button driver.
    ButtonsInit();

    while(1)
    {
        // Poll the buttons.
        ucState = ButtonsPoll(&ucDelta, 0);

        // Decrement duty cycle by 1% units each time left button is pressed
        if(BUTTON_PRESSED(LEFT_BUTTON, ucState, ucDelta))
        {
            // Duty cycle can be minimum 0%
            if(k>0)
            {
                k = k-1;
            }
        }
        // Increment duty cycle by 1% units each time right button is pressed
        if(BUTTON_PRESSED(RIGHT_BUTTON, ucState, ucDelta))
        {
            // Duty cycle can be maximum 100%
            if(k<100)
            {
                k = k+1;
            }
        }

        // Duty cycle can be minimum 0%
        if(k<=0)
        {
            // Turn off the led using LEDWrite over UART
            // Put a 0 in the output buffer.
            //UARTCharPut(UART0_BASE, 0);
            // Enable the GPIO pin for the LED (PF2).
            // Set the direction as output, and
            // enable the GPIO pin for digital function.
            // Required to do to take control from the PWM and turn off and on the LED with GPIOPinWrite
            GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
            // Turn off LED
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);
        }
        // Duty cycle can be maximum 100%
        else if(k>=100)
        {
            // Turn off the led using LEDWrite over UART
            // Put a 0 in the output buffer.
            //UARTCharPut(UART0_BASE, 1);
            // Enable the GPIO pin for the LED (PF2).
            // Set the direction as output, and
            // enable the GPIO pin for digital function.
            // Required to do to take control from the PWM and turn off and on the LED with GPIOPinWrite
            GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
            // Turn on LED at max
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
        }
        else if((k >= 1) && (k <= 99))
        {
            // Configure the GPIO pin for PWM function (PF2)
            // Required to do again to transfer control of the pin back to the PWM
            GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
            // Update PWM2 to a duty cycle of 1*k %, based on user input.
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, (pwm_word/100)*k);
        }

        //Use UARTgets()? to let user interact with LED brightness?

        /*
        // Get the character(s) in the receive FIFO.
        // First check that there are characters available
        if (UARTCharsAvail(UART0_BASE))
        {
            UARTprintf("%d\n",UARTgetc());
            // If 0 is returned turn off the led
            if (UARTCharGet(UART0_BASE) == 0)
            {
                // Enable the GPIO pin for the LED (PF2).
                // Set the direction as output, and
                // enable the GPIO pin for digital function.
                // Required to do to take control from the PWM and turn off and on the LED with GPIOPinWrite
                GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
                // Turn on LED at max
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);
            }
            // If 1 is returned turn on the led
            else if (UARTCharGet(UART0_BASE) == 1)
            {
                // Enable the GPIO pin for the LED (PF2).
                // Set the direction as output, and
                // enable the GPIO pin for digital function.
                // Required to do to take control from the PWM and turn off and on the LED with GPIOPinWrite
                GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
                // Turn on LED at max
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
            }
        }
        */
    }
}
//=============================================================================
