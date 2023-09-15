/**
 * ----------------------------------------------------------------------------
 * main.c
 * Author: Carl Larsson
 * Description: Snake game
 * Date: 2023-09-02
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
// Check if food overlaps with snake
// Returns 1 if food overlaps with snake
// Returns 0 if food does not overlap with snake
int16_t check_rect_overlap_food(CircularQueue* q, int16_t size, tRectangle food_rectangle)
{
    tRectangle snake_part;
    // Temp is used to step through the list and check all elements
    int16_t temp = q->front;

    // Go trough the list and check for any overlap
    while (temp != q->rear)
    {
        snake_part.i16XMin = q->queue[temp].x;
        snake_part.i16YMin = q->queue[temp].y;
        snake_part.i16XMax = snake_part.i16XMin + size;
        snake_part.i16YMax = snake_part.i16YMin + size;
        if(GrRectOverlapCheck(&food_rectangle, &snake_part))
        {
            // Returns 1 if food overlaps with snake
            return 1;
        }
        temp = (temp + 1) % QUEUESIZE;
    }
    // Returns 0 if food does not overlap with snake
    return 0;
}
//=============================================================================
// Check if snake overlaps itself
// Returns 1 if snake overlaps with itself
// Returns 0 if snake does not overlap itself
int16_t check_rect_overlap_snake(CircularQueue* q, int16_t size)
{
    tRectangle snake_head;
    tRectangle snake_part;
    // Temp is used to step through the list and check all elements
    int16_t temp = q->front;

    // Head of the snake is the last element in the list (rear)
    snake_head.i16XMin = q->queue[q->rear].x;
    snake_head.i16YMin = q->queue[q->rear].y;
    snake_head.i16XMax = snake_head.i16XMin + size;
    snake_head.i16YMax = snake_head.i16YMin + size;

    // Go trough the list and check for any overlap
    while (temp != q->rear)
    {
        snake_part.i16XMin = q->queue[temp].x;
        snake_part.i16YMin = q->queue[temp].y;
        snake_part.i16XMax = snake_part.i16XMin + size;
        snake_part.i16YMax = snake_part.i16YMin + size;
        if(GrRectOverlapCheck(&snake_head, &snake_part))
        {
            // Returns 1 if snake overlaps with itself
            return 1;
        }
        temp = (temp + 1) % QUEUESIZE;
    }
    // Returns 0 if snake does not overlap itself
    return 0;
}
//=============================================================================
// Main Function
int main(void)
{
    ConfigureUART();

    uint32_t systemClock;
    tContext context;

    //-----------------------------------------------------------------------------
    // LCD Colors
    // see https://www.ti.com/lit/ug/spmu300e/spmu300e.pdf?ts=1693897900634&ref_url=https%253A%252F%252Fwww.startpage.com%252F page 269
    uint32_t background_color = ClrBlack;
    uint32_t snake_color = ClrLime;
    uint32_t food_color = ClrRed;
    uint32_t text_color = ClrWhite;
    uint32_t background_color_text = ClrRed;
    // ui32Value is the 24-bit RGB color.  The least-significant byte is the
    // blue channel, the next byte is the green channel, and the third byte is the
    // red channel.
    //-----------------------------------------------------------------------------
    // Snake
    tRectangle snake_body;
    tRectangle old_snake_body;
    Coordinat snake_rear;
    int16_t snake_body_size = 9;
    int16_t skip_dequeue = 0;
    // Can not initialize instantly, it causes a fault interrupt
    CircularQueue snake_queue;
    snake_queue.front = -1;
    snake_queue.rear = -1;
    //-----------------------------------------------------------------------------
    // Food
    tRectangle food_;
    int16_t food_size = 5;
    int16_t spawn_food = 1;
    int16_t num_food_eaten = 0;
    //-----------------------------------------------------------------------------

    uint32_t joystick_val_ver;
    uint32_t joystick_val_hor;

    // Run from the PLL at 40 MHz (needs to be 2*15MHz for SSIConfigSetExpClk(); to work).
    systemClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 40000000);

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
    GrContextForegroundSet(&context, snake_color);
    // Sets text background color.
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
    //-----------------------------------------------------------------------------

    // Infinite while loop
    while(1)
    {
        // Enable spawning of food
        spawn_food = 1;
        // Set number of food eaten to 0
        num_food_eaten = 0;

        // Clears/redraws the screen.
        CF128x128x16_ST7735SClear(background_color);
        // Starting position, in the middle
        snake_body.i16XMin = 60;
        snake_body.i16YMin = 60;
        snake_body.i16XMax = snake_body.i16XMin + snake_body_size;
        snake_body.i16YMax = snake_body.i16YMin + snake_body_size;
        GrRectFill(&context, &snake_body);
        enqueue(&snake_queue, snake_body.i16XMin, snake_body.i16YMin);

        // While loop for one round, as long as you live (doesn't cross yourself or you go out of bound)
        while (1)
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

            // Convert joystick values from a 0 to 4095 range down to 0 to 100 range (percentage)
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

            // Convert joystick values from a 0 to 4095 range down to 0 to 100 range (percentage)
            joystick_val_hor = roundf((100.0 / 4095.0) * joystick_val_hor);
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Movement
            //-----------------------------------------------------------------------------
            // UP
            if (joystick_val_ver > 70)
            {
                // Skip dequeue and don't redraw with background color to simulate longer snake after having eaten food
                if(skip_dequeue == 0)
                {
                    // Snake has moved, so we need to remove that position from circular queue (first element, which is the snake tail), redraw with background color
                    snake_rear = dequeue(&snake_queue);
                    old_snake_body.i16XMin = snake_rear.x;
                    old_snake_body.i16YMin = snake_rear.y;
                    old_snake_body.i16XMax = old_snake_body.i16XMin + snake_body_size;
                    old_snake_body.i16YMax = old_snake_body.i16YMin + snake_body_size;
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &old_snake_body);
                }
                // Update and draw new position, add new position to list (last, which is the snake head)
                snake_body.i16YMin = snake_body.i16YMin - (snake_body_size+2);
                enqueue(&snake_queue, snake_body.i16XMin, snake_body.i16YMin);
                skip_dequeue = 0;
            }
            //-----------------------------------------------------------------------------
            // Right
            else if (joystick_val_hor > 70)
            {
                // Skip dequeue and don't redraw with background color to simulate longer snake after having eaten food
                if(skip_dequeue == 0)
                {
                    // Snake has moved, so we need to remove that position from circular queue (first element, which is the snake tail), redraw with background color
                    snake_rear = dequeue(&snake_queue);
                    old_snake_body.i16XMin = snake_rear.x;
                    old_snake_body.i16YMin = snake_rear.y;
                    old_snake_body.i16XMax = old_snake_body.i16XMin + snake_body_size;
                    old_snake_body.i16YMax = old_snake_body.i16YMin + snake_body_size;
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &old_snake_body);
                }
                // Update and draw new position, add new position to list (last, which is the snake head)
                snake_body.i16XMin = snake_body.i16XMin + (snake_body_size+2);
                enqueue(&snake_queue, snake_body.i16XMin, snake_body.i16YMin);
                skip_dequeue = 0;
            }
            //-----------------------------------------------------------------------------
            // Down
            else if (joystick_val_ver < 30)
            {
                // Skip dequeue and don't redraw with background color to simulate longer snake after having eaten food
                if(skip_dequeue == 0)
                {
                    // Snake has moved, so we need to remove that position from circular queue (first element, which is the snake tail), redraw with background color
                    snake_rear = dequeue(&snake_queue);
                    old_snake_body.i16XMin = snake_rear.x;
                    old_snake_body.i16YMin = snake_rear.y;
                    old_snake_body.i16XMax = old_snake_body.i16XMin + snake_body_size;
                    old_snake_body.i16YMax = old_snake_body.i16YMin + snake_body_size;
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &old_snake_body);
                }
                // Update and draw new position, add new position to list (last, which is the snake head)
                snake_body.i16YMin = snake_body.i16YMin + (snake_body_size+2);
                enqueue(&snake_queue, snake_body.i16XMin, snake_body.i16YMin);
                skip_dequeue = 0;
            }
            //-----------------------------------------------------------------------------
            // Left
            else if (joystick_val_hor < 30)
            {
                // Skip dequeue and don't redraw with background color to simulate longer snake after having eaten food
                if(skip_dequeue == 0)
                {
                    // Snake has moved, so we need to remove that position from circular queue (first element, which is the snake tail), redraw with background color
                    snake_rear = dequeue(&snake_queue);
                    old_snake_body.i16XMin = snake_rear.x;
                    old_snake_body.i16YMin = snake_rear.y;
                    old_snake_body.i16XMax = old_snake_body.i16XMin + snake_body_size;
                    old_snake_body.i16YMax = old_snake_body.i16YMin + snake_body_size;
                    GrContextForegroundSet(&context, background_color);
                    GrRectFill(&context, &old_snake_body);
                }
                // Update and draw new position, add new position to list (last, which is the snake head)
                snake_body.i16XMin = snake_body.i16XMin - (snake_body_size+2);
                enqueue(&snake_queue, snake_body.i16XMin, snake_body.i16YMin);
                skip_dequeue = 0;
            }
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Snake
            snake_body.i16XMax = snake_body.i16XMin + snake_body_size;
            snake_body.i16YMax = snake_body.i16YMin + snake_body_size;
            // Set the color for pixels drawn
            GrContextForegroundSet(&context, snake_color);
            GrRectFill(&context, &snake_body);
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Food
            // Only have 1 food spawned at a time
            if (spawn_food == 1)
            {
                // To prevent food spawning on the edge of the screen
                do
                {
                    // Map the rand function into a 0 to 128 range (screen dimension)
                    food_.i16XMin = roundf((128.0 / 32767.0) * rand());
                    food_.i16YMin = roundf((128.0 / 32767.0) * rand());
                    food_.i16XMax = food_.i16XMin + food_size;
                    food_.i16YMax = food_.i16YMin + food_size;
                }while((food_.i16XMin < 6) || (food_.i16YMin < 6) || (food_.i16XMax > 122) || (food_.i16YMax > 122) || (check_rect_overlap_food(&snake_queue, snake_body_size, food_) == 1));
                // Set the color for pixels drawn
                GrContextForegroundSet(&context, food_color);
                GrRectFill(&context, &food_);
                // Disable spawning of food until it has been consumed
                spawn_food = 0;
            }
            //-----------------------------------------------------------------------------

            //-----------------------------------------------------------------------------
            // Game Logic
            //-----------------------------------------------------------------------------
            // Death
            // Out of bound, you die
            if ((snake_body.i16XMin < 0) ||
                    (snake_body.i16YMin < 0) ||
                    (snake_body.i16XMax > 128) ||
                    (snake_body.i16YMax > 128))
            {
                empty_queue(&snake_queue);
                break;
            }
            // Check if snake overlaps with itself
            if(check_rect_overlap_snake(&snake_queue, (snake_body_size-snake_body_size)) == 1)
            {
                empty_queue(&snake_queue);
                break;
            }
            //-----------------------------------------------------------------------------
            // Food
            if(GrRectOverlapCheck(&snake_body, &food_))
            {
                // Remove food from screen
                // Set the color for pixels drawn
                GrContextForegroundSet(&context, background_color);
                GrRectFill(&context, &food_);
                // Food has been consumed, enable spawning of food
                spawn_food = 1;
                num_food_eaten++;
                // Set the color for pixels drawn
                GrContextForegroundSet(&context, snake_color);
                GrRectFill(&context, &snake_body);
                // Skip next dequeue to simulate increased length of snake
                skip_dequeue = 1;
            }
            if(num_food_eaten >= QUEUESIZE)
            {
                // Set the color for pixels drawn
                GrContextForegroundSet(&context, text_color);
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
            MAP_SysCtlDelay(systemClock / 30);
        }
    }
}
//=============================================================================
