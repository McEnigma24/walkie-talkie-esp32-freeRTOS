#ifndef PTT_H
#define PTT_H

#include <stdbool.h>
#include "esp_err.h"

// Jeden pin wystarczy: przycisk miedzy GPIO a GND (pull-up wewnetrzny).
// GPIO17 zostaw wolny pod NRF24 CE.
#define PTT_GPIO            16
#define PTT_DEBOUNCE_MS     50

/*
 * ptt.h — obsluga przycisku PTT (Push-To-Talk) dla ESP32 / ESP-IDF
 *
 * PODLACZENIE SPRZETOWE:
 *   - Przycisk laczy sie miedzy GPIO16 a GND (NIE do VCC).
 *   - Jedna nozka -> GPIO16, druga nozka -> GND.
 *   - Nie jest potrzebny zewnetrzny rezystor: uzywamy wewnetrznego
 *     pull-upa (GPIO_PULLUP_ENABLE).
 *
 * ZASADA DZIALANIA (logika active-low):
 *   - Przycisk ZWOLNIONY: pull-up trzyma GPIO16 w stanie WYSOKIM (1).
 *   - Przycisk WCISNIETY: GPIO16 zwarte do GND -> stan NISKI (0).
 *   - Dlatego w kodzie: pressed = (gpio_get_level(PTT_GPIO) == 0).
 *
 * PRZERWANIA I DEBOUNCE:
 *   - GPIO_INTR_ANYEDGE budzi zadanie przy kazdej zmianie stanu.
 *   - ISR oddaje semafor, ktory wybudza ptt_task.
 *   - Stan musi byc stabilny przez PTT_DEBOUNCE_MS (50 ms), aby
 *     odfiltrowac drgania stykow.
 *
 * TOGGLE:
 *   - Nadawanie przelacza sie przy PUSZCZENIU przycisku (jeden klik
 *     = zmiana ptt_transmitting na przeciwny stan).
 *   - Stan odczytujesz przez ptt_is_transmitting().
 *
 * UWAGA: GPIO17 pozostaje wolny (zarezerwowany pod NRF24 CE).
 */

esp_err_t ptt_init(void);
bool ptt_is_transmitting(void);
void ptt_force_stop(void);

#endif
