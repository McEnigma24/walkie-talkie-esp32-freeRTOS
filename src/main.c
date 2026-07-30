#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "blinker.h"
#include "speaker.h"
#include "mic.h"
#include "ptt.h"
#include "nRF.h"

#include "psa/crypto.h"


// static int16_t tx_chunk[BUFFER_SAMPLES];

// static void run_transmit_mode(gpio_output_t *blinker)
// {
//     printf("Tryb TX (emisja) - mow do mikrofonu\n");
//     gpio_output_toggle(blinker);
//     ESP_ERROR_CHECK(speaker_stream_begin());

//     while (ptt_is_transmitting())
//     {
//         ESP_ERROR_CHECK(mic_read_samples(tx_chunk, BUFFER_SAMPLES));
//         ESP_ERROR_CHECK(speaker_stream_write(tx_chunk, BUFFER_SAMPLES));
//     }

//     ESP_ERROR_CHECK(speaker_stream_end());
//     gpio_output_toggle(blinker);
//     printf("Tryb RX (odbior)\n");
// }






// #define TRANSMITTER
#define RECEIVER


#if defined(TRANSMITTER) && defined(RECEIVER)
    #error "Configuration error: TRANSMITTER and RECEIVER cannot be defined at the same time."
#endif


#define MIC_DATA_TYPE int16_t
#define MIC_UNALIGNED_BUFFER_SIZE ( 50'000 )
#define MIC_BUFFER_SIZE ( (((MIC_UNALIGNED_BUFFER_SIZE / TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT) + (TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT - 1))) * (TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT) )
#define MIC_BUFFER_BYTE_SIZE ( MIC_BUFFER_SIZE * sizeof(MIC_DATA_TYPE) )
static MIC_DATA_TYPE MIC_BUFFER[MIC_BUFFER_SIZE]; // 10'000 * sizeof(int16_t) == ~20 kB, w .bss

// AES-CTR na PSA Crypto (Mbed TLS 4.x). CTR jest trybem strumieniowym:
// szyfruje dowolna dlugosc bez paddingu, a szyfrowanie == deszyfrowanie.
// Uwaga: dla danego klucza kazda wiadomosc MUSI miec unikalne IV (nonce).
static bool aes_ctr_stream_init(mbedtls_svc_key_id_t *key_id,
                                psa_cipher_operation_t *op,
                                const uint8_t *key, size_t key_len,
                                uint8_t *iv_out, size_t iv_size)
{
    // "Metryczka" klucza - opisuje CO to za klucz i DO CZEGO wolno go uzyc.
    // PSA egzekwuje te ustawienia: proba uzycia klucza niezgodnie z metryczka konczy sie bledem.
    // PSA_KEY_ATTRIBUTES_INIT to bezpieczny stan poczatkowy (wszystkie pola wyzerowane).
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    // usage flags = jakie operacje sa dozwolone tym kluczem (maska bitowa, mozna laczyc '|'):
    //   PSA_KEY_USAGE_ENCRYPT - wolno szyfrowac (strona TX),
    //   PSA_KEY_USAGE_DECRYPT - wolno deszyfrowac (strona RX).
    // Tu wlaczamy oba, bo ten sam klucz wspoldzielimy do TX i RX (walkie-talkie w obie strony).
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);

    // algorithm policy = z jakim algorytmem ten klucz moze byc uzyty.
    //   PSA_ALG_CTR - AES w trybie licznikowym (Counter). Tryb strumieniowy: brak paddingu,
    //   dowolna dlugosc danych, enc == dec. Klucz "przypiety" do CTR nie da sie np. uzyc w GCM.
    psa_set_key_algorithm(&attr, PSA_ALG_CTR);

    // key type = rodzina algorytmu / typ materialu klucza.
    //   PSA_KEY_TYPE_AES - surowe bajty klucza AES. Dlugosc (128/192/256 bit) PSA wywnioskuje
    //   automatycznie z key_len przekazanego do psa_import_key (16/24/32 bajty).
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    // Wgrywa surowe bajty klucza do PSA i zwraca jego uchwyt (key_id).
    // Od teraz poslugujemy sie tylko uchwytem - sam material klucza jest schowany w PSA.
    psa_status_t st = psa_import_key(&attr, key, key_len, key_id);
    if (st != PSA_SUCCESS) { printf("psa_import_key: %ld\n", (long)st); return false; }

    // Reset obiektu operacji do stanu poczatkowego przed konfiguracja (wymagane przez PSA).
    *op = (psa_cipher_operation_t) PSA_CIPHER_OPERATION_INIT;

    // Konfiguruje operacje jako SZYFROWANIE danym kluczem i algorytmem CTR.
    // (Po stronie RX uzyjesz analogicznie psa_cipher_decrypt_setup.)
    st = psa_cipher_encrypt_setup(op, *key_id, PSA_ALG_CTR);
    if (st != PSA_SUCCESS) { printf("encrypt_setup: %ld\n", (long)st); return false; }

    // Losuje IV (nonce) - dla AES to 16 bajtow (rozmiar bloku). iv_len = ile bajtow faktycznie zapisano.
    // IV NIE jest tajne, ale MUSI byc unikalne dla danego klucza -> wyslij je do odbiornika
    // (np. w pierwszym pakiecie), a RX ustawi je przez psa_cipher_set_iv przed deszyfrowaniem.
    size_t iv_len = 0;
    st = psa_cipher_generate_iv(op, iv_out, iv_size, &iv_len);
    if (st != PSA_SUCCESS) { printf("generate_iv: %ld\n", (long)st); return false; }

    return true;
}

void app_main(void)
{
    printf("Walkie-talkie %d Hz - PTT na GPIO%d\n", SAMPLE_RATE, PTT_GPIO);

    gpio_output_t blinker = gpio_output_init(BLINK_GPIO, true);
    gpio_output_blink(&blinker, 3, 500, 500);

    ESP_ERROR_CHECK(speaker_init());
    ESP_ERROR_CHECK(nRF_init());

    #ifdef TRANSMITTER
        // ESP_ERROR_CHECK(mic_init());
        ESP_ERROR_CHECK(mic_init_cont());
        ESP_ERROR_CHECK(mic_stream_init());

        ESP_ERROR_CHECK(ptt_init());
    #endif

    #ifdef RECEIVER
        // ESP_ERROR_CHECK(speaker_stream_begin());
        // int mic_buffer_idx = 0;
    #endif

    // uint8_t tmp_single_packet_buffer[32];
    // int64_t TPUT_last_tick_before_full_1s = esp_timer_get_time();



    // Inicjalizacja podsystemu PSA Crypto - MUSI byc wywolane raz przed jakąkolwiek operacja PSA.
    // Zwraca PSA_SUCCESS (0) przy powodzeniu; opakowujemy w ESP_ERROR_CHECK, by ubic boot przy bledzie.
    ESP_ERROR_CHECK(psa_crypto_init() == PSA_SUCCESS ? ESP_OK : ESP_FAIL);

    // 16 bajtow = klucz AES-128. TODO: podmien na realny, wspoldzielony klucz (nie same zera!).
    static const uint8_t aes_key[16] = {0};

    // Uchwyt klucza w PSA - MBEDTLS_SVC_KEY_ID_INIT to "pusty/niewazny" identyfikator na start.
    mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;

    // Obiekt trzymajacy stan trwajacego szyfrowania (m.in. licznik CTR) miedzy kolejnymi chunkami.
    psa_cipher_operation_t enc = PSA_CIPHER_OPERATION_INIT;

    // Bufor na wygenerowane IV (16 B dla AES). Do wyslania odbiornikowi.
    uint8_t iv[16];

    if (aes_ctr_stream_init(&key_id, &enc, aes_key, sizeof(aes_key), iv, sizeof(iv)))
    {
        // Kazdy przychodzacy chunk audio szyfrujesz w locie tym samym obiektem 'enc'.
        // W CTR wyjscie ma dokladnie tyle bajtow co wejscie (out_len == wejscie), bez paddingu.
        uint8_t plain[32] = {0};   // dane jawne (tu placeholder; docelowo probki audio)
        uint8_t cipher[32];        // bufor na szyfrogram; musi byc >= rozmiar wejscia
        size_t out_len = 0;        // ile bajtow szyfrogramu faktycznie zapisano
        // psa_cipher_update: przetwarza kolejna porcje strumienia, kontynuujac licznik CTR.
        // Woluj to wielokrotnie - po jednym razie na kazdy chunk audio.
        psa_status_t st = psa_cipher_update(&enc, plain, sizeof(plain),
                                            cipher, sizeof(cipher), &out_len);
        printf("psa_cipher_update: %ld, out=%u\n", (long)st, (unsigned)out_len);

        // Sprzatanie po zakonczeniu strumienia:
        //   psa_cipher_abort  - zwalnia stan operacji (bezpieczne tez po bledzie),
        //   psa_destroy_key   - usuwa klucz z PSA i czysci jego bajty z pamieci.
        psa_cipher_abort(&enc);
        psa_destroy_key(key_id);
    }

    /*
        #define BENCHMARK(x, numbers) \
        { \
        \
        \
        }

        int64_t next_us = esp_timer_get_time();
        int64_t next_us = esp_timer_get_time();
        int64_t next_us = esp_timer_get_time();


    */




    return;

    // xTaskCreate(cont_mic_stream_task, "mic", 4096, NULL, 5, NULL);
    // xTaskCreate(nRF_stream_task,      "nrf", 4096, NULL, 5, NULL);

    /*

    while (1)
    {
        #ifdef TRANSMITTER
        {
            if (ptt_is_transmitting())
            {
                // ptt_force_stop(); // turning to non-transmit mode
                nRF_send_data((uint8_t*)tmp_single_packet_buffer, 32);

                // gpio_output_blink(&blinker, 1, 50, 20);
            }
        }
        #endif

        #ifdef RECEIVER
        {
            if (Nrf24_dataReady(&dev))
            {
                Nrf24_getData(&dev, tmp_single_packet_buffer);
            }



            // static uint32_t TPUT_volume = 0;
            // static uint32_t TPUT_count = 0;

            // TPUT_volume += 32;
            // TPUT_count++;

            // int64_t now_us = esp_timer_get_time();
            // int64_t diff_us = ( now_us - TPUT_last_tick_before_full_1s );
            // if(1'000'000 < diff_us)
            // {
            //     // printf("TPUT_volume  %ld\n", TPUT_volume);
            //     // printf("TPUT_count  %ld\n", TPUT_count);
            //     printf("TPUT_volume per second %fB\n", TPUT_volume / ((float)diff_us / 1'000'000));
            //     printf("TPUT_count  per second %f\n", TPUT_count / ((float)diff_us / 1'000'000));

            //     TPUT_last_tick_before_full_1s = now_us;

            //     TPUT_volume = 0;
            //     TPUT_count = 0;
            // }

            // transmittig all stored data //
            // nRF_send_data((uint8_t*)MIC_BUFFER, MIC_BUFFER_BYTE_SIZE);

            // uint8_t ending_preamble[32];
            // for(int i=0; i<32; i+=2)
            // {
            //     ending_preamble[i] = 0;
            //     ending_preamble[i + 1] = 0xff;
            // }
            // nRF_send_data((uint8_t*)ending_preamble, 32);

            // gpio_output_toggle(&blinker);
            // mic_read_samples(MIC_BUFFER, MIC_BUFFER_SIZE);
            // gpio_output_toggle(&blinker);


                    // TX:
                    for(int i=0; i<32; i++)
                    {
                        buf[i] = i;
                    }

                    // Nrf24_send(&dev, buf);
                    Nrf24_sendNoAck(&dev, buf);
                    bool status = Nrf24_isSend(&dev, 1000);
                    printf("Sending data - %d \n", status);

                    vTaskDelay(pdMS_TO_TICKS(20));


            // play_tone();

            // // RX:
            // if (Nrf24_dataReady(&dev))
            // {
            //     uint8_t pkt[32];

            //     Nrf24_getData(&dev, pkt); // odbierz do bufora tymczasowego, NIE prosto do MIC_BUFFER

            //     bool pattern_holds = true;
            //     for(int i=0; i<32; i+=2)
            //     {
            //         if(! ((pkt[i] == 0) && (pkt[(i + 1)] == 0xff)))
            //         {
            //             pattern_holds = false;
            //             break;
            //         }
            //     }

            //     if(pattern_holds)
    //         //     {
    //         //         printf("END MARKER detected\n");
    //         //         speaker_stream_write(MIC_BUFFER, mic_buffer_idx); // graj tyle, ile faktycznie odebrano (bez wartownika)
    //         //         mic_buffer_idx = 0;                               // gotowi na kolejna wiadomosc
    //         //     }
    //         //     else if(mic_buffer_idx + TRANSMISSION_PAYLOAD_LENGTH / sizeof(MIC_DATA_TYPE) <= MIC_BUFFER_SIZE)
    //         //     {
    //         //         memcpy(&MIC_BUFFER[mic_buffer_idx], pkt, TRANSMISSION_PAYLOAD_LENGTH);   // dopisz audio
    //         //         mic_buffer_idx += TRANSMISSION_PAYLOAD_LENGTH / sizeof(MIC_DATA_TYPE);   // 16
    //         //     }
    //         //     // else: bufor pelny -> pomijaj audio az do wartownika
    //         // }
    //     }
    //     #endif
    // }

    */

    ESP_ERROR_CHECK(speaker_stream_end());


    // printf("\n");
    // for(int i=0; i<32; i++)
    // {
    //     if((i % 4) == 0) printf("Payload data ");

    //     char sting_bufor[12];
    //     snprintf(sting_bufor, sizeof(sting_bufor), "%d", i);
    //     printf(" [%s]:0x%02x,", sting_bufor, buf[i]);

    //     if((i % 4) == 3) printf("\n");
    // }

    // while (1)
    // {
    //     if (ptt_is_transmitting())
    //     {
    //         run_transmit_mode(&blinker);
    //     }

    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }
}
