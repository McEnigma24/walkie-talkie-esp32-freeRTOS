#ifndef EVENT_GROUP_H
#define EVENT_GROUP_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern EventGroupHandle_t xCreatedEventGroup;

#define EVENT_RECEIVE  ( 1 << 0 )
#define EVENT_TRANSMIT ( 1 << 1 )

esp_err_t init_event_group(void);

bool isReceiveModeStillActive(void);  // in loop
bool isTransmitModeStillActive(void); // in loop

bool isReceiveModeStillActiveFromISR(void);  // ISR
bool isTransmitModeStillActiveFromISR(void); // ISR

void blockWaitForReceiveMode(void);
void blockWaitForTransmitMode(void);

void switchToTransmitModeFromISR(void); // ISR
void switchToReceiveModeFromISR(void);  // ISR
void toggleModeFromISR(void);           // ISR

#endif
