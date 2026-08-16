#ifndef EVENT_GROUP_H
#define EVENT_GROUP_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

EventGroupHandle_t xCreatedEventGroup;

#define EVENT_RECEIVE  ( 1 << 0 )
#define EVENT_TRANSMIT ( 1 << 1 )

static esp_err_t init_event_group(void)
{
    xCreatedEventGroup = xEventGroupCreate();

    xEventGroupSetBits(xCreatedEventGroup, EVENT_RECEIVE); // RECEIVE as a default

    return (xCreatedEventGroup != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool isCHOSENmodeStillActive(EventBits_t choice)
{
    EventBits_t uxBits = xEventGroupGetBits(xCreatedEventGroup);
    return uxBits & ( choice );
}

static bool isReceiveModeStillActive(void) // in loop
{
    return isCHOSENmodeStillActive( EVENT_RECEIVE );
}

static bool isTransmitModeStillActive(void) // in loop
{
    return isCHOSENmodeStillActive( EVENT_TRANSMIT );
}

static bool isCHOSENmodeStillActiveFromISR(EventBits_t choice) // ISR
{
    EventBits_t uxBits = xEventGroupGetBitsFromISR(xCreatedEventGroup);
    return uxBits & ( choice );
}

static bool isReceiveModeStillActiveFromISR(void) // ISR
{
    return isCHOSENmodeStillActiveFromISR( EVENT_RECEIVE );
}

static bool isTransmitModeStillActiveFromISR(void) // ISR
{
    return isCHOSENmodeStillActiveFromISR( EVENT_TRANSMIT );
}

static void blockWaitForCHOSENmode(EventBits_t choice)
{
    EventBits_t uxBits = xEventGroupWaitBits(
        xCreatedEventGroup,
        choice,            // The bits within the event group to wait for
        pdFALSE,           // should be cleared before returning ?
        pdFALSE,           // Don't wait for both bits, either bit will do
        portMAX_DELAY      // wait Infinitely
    );
}

static void blockWaitForReceiveMode(void)
{
    blockWaitForCHOSENmode( EVENT_RECEIVE );
}

static void blockWaitForTransmitMode(void)
{
    blockWaitForCHOSENmode( EVENT_TRANSMIT );
}

static void switchToCHOSENmodeFromISR(EventBits_t choice) // ISR
{
    BaseType_t xHigherPriorityTaskWoken, xResult;

    /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
    xHigherPriorityTaskWoken = pdFALSE;

    xResult = xEventGroupSetBitsFromISR(
                                xCreatedEventGroup,
                                choice,
                                &xHigherPriorityTaskWoken );

    /* Was the message posted successfully? */
    if( xResult != pdFAIL )
    {
        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
           switch should be requested. The macro used is port specific and will
           be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
           the documentation page for the port being used. */
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

static void switchToTransmitModeFromISR() // ISR
{
    switchToCHOSENmodeFromISR ( EVENT_TRANSMIT );
}

static void switchToReceiveModeFromISR() // ISR
{
    switchToCHOSENmodeFromISR ( EVENT_RECEIVE );
}

static void toggleModeFromISR(void) // ISR
{
    if(isReceiveModeStillActiveFromISR())
    {
        switchToTransmitModeFromISR();
        return;
    }

    if(isTransmitModeStillActiveFromISR())
    {
        switchToReceiveModeFromISR();
        return;
    }
}



#endif