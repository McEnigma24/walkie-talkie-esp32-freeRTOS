#include "crypto.h"

#include <stdio.h>
#include <string.h>
#include "psa/crypto.h"
#include "esp_log.h"
#include "event_group.h"
#include "streams.h"

static const char *TAG = "PSA_MICRO_PACKET";

// ======================= KLUCZ GŁÓWNY AES ======================= //
// Wybierz długość klucza: 128, 192 albo 256 bitów.
#define AES_KEY_BITS  256
#define AES_KEY_BYTES (AES_KEY_BITS / 8)

// Wpisz tu swoje bajty klucza. Musi ich być dokładnie AES_KEY_BYTES:
//   AES-128 -> 16 bajtów, AES-192 -> 24 bajty, AES-256 -> 32 bajty.
// Oba radia (TX i RX) muszą mieć identyczny klucz.
static const uint8_t AES_MASTER_KEY[] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69, 0x78,
    0x87, 0x96, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0
};
_Static_assert(sizeof(AES_MASTER_KEY) == AES_KEY_BYTES, "Liczba bajtow AES_MASTER_KEY nie zgadza sie z AES_KEY_BITS");

static psa_key_id_t aes_key_id = 0;

// ======================= SALT -> 14B + 2B (SN) ======================= //

static uint8_t IV_key[16] = {
    0xAA, 0xBB, 0xCC, 0xDD,
    0xEE, 0xFF, 0x11, 0x22,
    0x33, 0x44, 0x55, 0x66,
    0x77, 0x88, 0x00, 0x00
};

static uint16_t LOCAL_SN = 0;

// Zwraca numer wpisany do IV - ten sam musi trafić do nagłówka pakietu,
// inaczej odbiornik zbuduje inne IV i odszyfruje szum.
static uint16_t add_SN(void)
{
    uint16_t used_SN = LOCAL_SN++;

    // Doklejamy 2 bajty sequence number na końcu (Little Endian)
    IV_key[14] = (used_SN >> 8) & 0xFF;
    IV_key[15] = used_SN & 0xFF;

    return used_SN;
}

static void sync_SN(uint16_t packet_SN)
{
    // if(LOCAL_SN != packet_SN)
    // {
    //     // ESP_LOGE(TAG, "Sequence Numbers are OUT OF SYNC   local: %d  packet: %d", (int)LOCAL_SN, (int)packet_SN);
    //     LOCAL_SN = packet_SN;
    // }
    LOCAL_SN = packet_SN;

    // Doklejamy 2 bajty sequence number na końcu (Little Endian)
    IV_key[14] = (LOCAL_SN >> 8) & 0xFF;
    IV_key[15] = LOCAL_SN & 0xFF;

    LOCAL_SN++;
}





// Functions //

// Zwraca esp_err_t, bo main.c opakowuje to w ESP_ERROR_CHECK - dla tego makra
// sukcesem jest wylacznie ESP_OK (0), a nie "prawda".
esp_err_t init_crypto(void)
{
    // Uruchamia backend PSA (RNG, sterowniki, magazyn kluczy).
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_crypto_init: %d", (int)status);
        return ESP_FAIL;
    }

    // "Metryczka" klucza - PSA egzekwuje ją przy każdym użyciu.
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    // Ten sam klucz służy do nadawania i odbioru (walkie-talkie w obie strony).
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);

    // Klucz przypięty do CTR - nie da się go użyć np. w GCM.
    psa_set_key_algorithm(&attr, PSA_ALG_CTR);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    // PSA wywnioskowałoby długość z liczby bajtów, ale podanie jej wprost
    // sprawia, że import odrzuci klucz o innym rozmiarze niż zadeklarowany.
    psa_set_key_bits(&attr, AES_KEY_BITS);

    // Wgrywa surowe bajty do PSA i zwraca uchwyt. Od tej chwili posługujemy
    // się tylko uchwytem - materiał klucza zostaje schowany w PSA.
    status = psa_import_key(&attr, AES_MASTER_KEY, AES_KEY_BYTES, &aes_key_id);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_import_key: %d", (int)status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Zaimportowano klucz AES-%d (id=%u)", AES_KEY_BITS, (unsigned)aes_key_id);
    return ESP_OK;
}

// // Kasuje klucz z pamięci PSA (np. przy wyłączaniu radia).
// static void crypto_deinit(void)
// {
//     if (aes_key_id != 0)
//     {
//         psa_destroy_key(aes_key_id);
//         aes_key_id = 0;
//     }
// }

bool en_crypto_audio_packet(uint8_t* IN_raw_audio, radio_packet_t* OUT_packet_to_send)
{
    psa_status_t status;
    if (aes_key_id == 0)
    {
        ESP_LOGE(TAG, "Brak klucza - najpierw wywolaj init_crypto()");
        return false;
    }
    if (nullptr == IN_raw_audio || nullptr == OUT_packet_to_send)
    {
        ESP_LOGE(TAG, "invalid input");
        return false;
    }

    OUT_packet_to_send->sequence_number = add_SN(); // increments counter

    // --- Używamy "Multi-part" jako Single-part, by podać WŁASNE IV ---
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

    // 1. Setup operacji szyfrowania
    status = psa_cipher_encrypt_setup(&operation, aes_key_id, PSA_ALG_CTR);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_cipher_encrypt_setup: %d", (int)status);
        return false;
    }

    // 2. WSTRZYKUJEMY WŁASNE IV (Magia, której nie ma w psa_cipher_encrypt)
    status = psa_cipher_set_iv(&operation, IV_key, sizeof(IV_key));
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_cipher_set_iv: %d", (int)status);
        psa_cipher_abort(&operation);
        return false;
    }

    size_t length_out = 0;

    // 3. Szyfrujemy nasze 30 bajtów bezpośrednio do structa pakietu
    status = psa_cipher_update(&operation,
                               IN_raw_audio,                        CRYPTO_PAYLOAD_BYTE_SIZE,
                               OUT_packet_to_send->encrypted_audio, CRYPTO_PAYLOAD_BYTE_SIZE,
                               &length_out);
    if (status != PSA_SUCCESS || length_out != CRYPTO_PAYLOAD_BYTE_SIZE)
    {
        ESP_LOGE(TAG, "psa_cipher_update: %d (out %u B)", (int)status, (unsigned)length_out);
        psa_cipher_abort(&operation);
        return false;
    }

    // 4. Koniec operacji. W przypadku CTR 'finish' nic nie dopisuje, ale zwalnia RAM na ESP32!
    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, NULL, 0, &finish_len);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_cipher_finish: %d", (int)status);
        psa_cipher_abort(&operation);
        return false;
    }

    // Gotowe! Pakiet ma równe 32 bajty: [2 bajty SEQ][30 bajtów SZYFROGRAMU]
    return true;
}

bool decode_radio_packet(radio_packet_t* IN_received_packet, uint8_t* OUT_raw_audio)
{
    psa_status_t status;
    if (aes_key_id == 0)
    {
        ESP_LOGE(TAG, "Brak klucza - najpierw wywolaj init_crypto()");
        return false;
    }
    if (nullptr == IN_received_packet || nullptr == OUT_raw_audio)
    {
        ESP_LOGE(TAG, "invalid input");
        return false;
    }

    sync_SN(IN_received_packet->sequence_number);

    // --- Używamy "Multi-part" jako Single-part, by podać WŁASNE IV ---
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

    // 1. Setup operacji deszyfrowania
    status = psa_cipher_decrypt_setup(&operation, aes_key_id, PSA_ALG_CTR);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_cipher_decrypt_setup: %d", (int)status);
        return false;
    }

    // 2. WSTRZYKUJEMY WŁASNE IV (Magia, której nie ma w psa_cipher_decrypt)
    status = psa_cipher_set_iv(&operation, IV_key, sizeof(IV_key));
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_cipher_set_iv: %d", (int)status);
        psa_cipher_abort(&operation);
        return false;
    }

    size_t length_out = 0;

    // 3. Deszyfrujemy
    status = psa_cipher_update(&operation,
                               IN_received_packet->encrypted_audio, CRYPTO_PAYLOAD_BYTE_SIZE,
                               OUT_raw_audio,                       CRYPTO_PAYLOAD_BYTE_SIZE,
                               &length_out);
    if (status != PSA_SUCCESS || length_out != CRYPTO_PAYLOAD_BYTE_SIZE)
    {
        ESP_LOGE(TAG, "psa_cipher_update: %d (out %u B)", (int)status, (unsigned)length_out);
        psa_cipher_abort(&operation);
        return false;
    }

    // 4. Koniec operacji. W przypadku CTR 'finish' nic nie dopisuje, ale zwalnia RAM na ESP32!
    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, NULL, 0, &finish_len);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_cipher_finish: %d", (int)status);
        psa_cipher_abort(&operation);
        return false;
    }

    return true;
}





// MIC -> EN_CRYPTO -> nRF Transmit //

void TASK_crypto_en_crypto(void *arg)
{
    (void)arg;
    uint8_t RAW_AUDIO_INPUT[CRYPTO_PAYLOAD_BYTE_SIZE];
    radio_packet_t packet_to_fill;
    size_t filled = 0;

    while(1)
    {
        blockWaitForTransmitMode();

        filled = 0; // nowa sesja - nie doklejamy ogona z poprzedniej

        while(isTransmitModeStillActive())
        {
            // Mikrofon dostarcza ramki po 128 B, a pakiet ma 30 B. Reszty z dzielenia
            // nie wolno wyrzucic - dokladamy ja do nastepnego pakietu, inaczej co ramke
            // gubimy 8 B audio (czyli 6% dzwieku).
            filled += xStreamBufferReceive(
                mic_to_en_crypto_stream,
                RAW_AUDIO_INPUT + filled,
                CRYPTO_PAYLOAD_BYTE_SIZE - filled,
                common_timeout
            );

            if(filled < CRYPTO_PAYLOAD_BYTE_SIZE)
            {
                continue;
            }
            filled = 0;

            if(! en_crypto_audio_packet(RAW_AUDIO_INPUT, &packet_to_fill))
            {
                ESP_LOGE(TAG, "Failed to encode data");
                continue;
            }

            // now, we have a encrypted packet -> packet_to_fill
            //
            // we send its content to another stream buffer to be sent by nRF

            xStreamBufferSend(
                en_crypto_to_nRF_transmit_stream,
                &packet_to_fill,
                sizeof(packet_to_fill),
                common_timeout
            );
        }
    }
}



// nRF Receive -> DE_CRYPTO -> SPEAKER //

void TASK_crypto_de_crypto(void *arg)
{
    (void)arg;
    uint8_t RAW_AUDIO_OUTPUT[CRYPTO_PAYLOAD_BYTE_SIZE];
    radio_packet_t received_packet;

    while(1)
    {
        blockWaitForReceiveMode();

        while(isReceiveModeStillActive())
        {
            size_t got = xStreamBufferReceive(
                nRF_receive_to_de_crypto_stream,
                &received_packet,
                sizeof(received_packet),
                common_timeout
            );

            if(got > 0)
            {
                // we got something //

                if(got != sizeof(received_packet))
                {
                    ESP_LOGE(TAG, "Received from StreamBuffer with incorrect size -> %u != 32", (unsigned)got);
                    continue;
                }

                if(! decode_radio_packet(&received_packet, RAW_AUDIO_OUTPUT))
                {
                    ESP_LOGE(TAG, "Failed to decode data");
                    continue;
                }

                // now, we have a decrypted audio
                //
                // we put it on the speaker

                xStreamBufferSend(
                    de_crypto_to_speaker_stream,
                    RAW_AUDIO_OUTPUT,
                    CRYPTO_PAYLOAD_BYTE_SIZE,
                    common_timeout
                );
            }
        }
    }
}
