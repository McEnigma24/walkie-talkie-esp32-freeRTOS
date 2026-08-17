#include "event_group.h"

EventGroupHandle_t xCreatedEventGroup;

esp_err_t init_event_group(void)
{
    xCreatedEventGroup = xEventGroupCreate();
    if (xCreatedEventGroup == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    xEventGroupSetBits(xCreatedEventGroup, EVENT_RECEIVE); // RECEIVE as a default

    return ESP_OK;
}

static bool isCHOSENmodeStillActive(EventBits_t choice)
{
    EventBits_t uxBits = xEventGroupGetBits(xCreatedEventGroup);
    return (uxBits & choice) != 0;
}

bool isReceiveModeStillActive(void) // in loop
{
    return isCHOSENmodeStillActive( EVENT_RECEIVE );
}

bool isTransmitModeStillActive(void) // in loop
{
    return isCHOSENmodeStillActive( EVENT_TRANSMIT );
}

static bool isCHOSENmodeStillActiveFromISR(EventBits_t choice) // ISR
{
    EventBits_t uxBits = xEventGroupGetBitsFromISR(xCreatedEventGroup);
    return (uxBits & choice) != 0;
}

bool isReceiveModeStillActiveFromISR(void) // ISR
{
    return isCHOSENmodeStillActiveFromISR( EVENT_RECEIVE );
}

bool isTransmitModeStillActiveFromISR(void) // ISR
{
    return isCHOSENmodeStillActiveFromISR( EVENT_TRANSMIT );
}

static void blockWaitForCHOSENmode(EventBits_t choice)
{
    (void)xEventGroupWaitBits(
        xCreatedEventGroup,
        choice,            // The bits within the event group to wait for
        pdFALSE,           // should be cleared before returning ?
        pdFALSE,           // Don't wait for both bits, either bit will do
        portMAX_DELAY      // wait Infinitely
    );
}

void blockWaitForReceiveMode(void)
{
    blockWaitForCHOSENmode( EVENT_RECEIVE );
}

void blockWaitForTransmitMode(void)
{
    blockWaitForCHOSENmode( EVENT_TRANSMIT );
}

static EventBits_t oppositeMode(EventBits_t choice)
{
    return (choice == EVENT_TRANSMIT) ? EVENT_RECEIVE : EVENT_TRANSMIT;
}

static void switchToCHOSENmode(EventBits_t choice)
{
    // Kasujemy przed ustawieniem, zeby zaden task nie zobaczyl obu bitow naraz
    xEventGroupClearBits(xCreatedEventGroup, oppositeMode(choice));
    xEventGroupSetBits(xCreatedEventGroup, choice);
}

void switchToTransmitMode(void)
{
    switchToCHOSENmode( EVENT_TRANSMIT );
}

void switchToReceiveMode(void)
{
    switchToCHOSENmode( EVENT_RECEIVE );
}

void toggleMode(void)
{
    if (isTransmitModeStillActive())
    {
        switchToReceiveMode();
        return;
    }

    switchToTransmitMode();
}

static void switchToCHOSENmodeFromISR(EventBits_t choice) // ISR
{
    BaseType_t xHigherPriorityTaskWoken, xResult;

    /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
    xHigherPriorityTaskWoken = pdFALSE;

    // Uwaga: z ISR kasowanie jest odroczone do timer daemona, wiec przez chwile
    // moga byc widoczne oba bity. W kontekscie taska uzywaj switchToCHOSENmode().
    (void)xEventGroupClearBitsFromISR(xCreatedEventGroup, oppositeMode(choice));

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

void switchToTransmitModeFromISR(void) // ISR
{
    switchToCHOSENmodeFromISR ( EVENT_TRANSMIT );
}

void switchToReceiveModeFromISR(void) // ISR
{
    switchToCHOSENmodeFromISR ( EVENT_RECEIVE );
}

void toggleModeFromISR(void) // ISR
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
