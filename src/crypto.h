#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdio.h>
#include <string.h>
#include "psa/crypto.h"
#include "esp_log.h"

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

#define PAYLOAD_DATA_BYTE_SIZE ( 30 )

// Struktura dokładnie odwzorowująca to, co leci w eter
typedef struct __attribute__((packed))
{
    uint16_t sequence_number;
    uint8_t  encrypted_audio[PAYLOAD_DATA_BYTE_SIZE];
} radio_packet_t;

static uint16_t LOCAL_SN = 0;

static void add_SN()
{
    // Doklejamy 2 bajty sequence number na końcu (Little Endian)
    IV_key[14] = (LOCAL_SN >> 8) & 0xFF;
    IV_key[15] = LOCAL_SN & 0xFF;

    LOCAL_SN++;
}

static void sync_SN(uint16_t packet_SN)
{
    if(LOCAL_SN != packet_SN)
    {
        ESP_LOGE(TAG, "Sequence Numbers are OUT OF SYNC   local: %d  packet: %d", (int)LOCAL_SN, (int)packet_SN);
        LOCAL_SN = packet_SN;
    }

    // Doklejamy 2 bajty sequence number na końcu (Little Endian)
    IV_key[14] = (LOCAL_SN >> 8) & 0xFF;
    IV_key[15] = LOCAL_SN & 0xFF;

    LOCAL_SN++;
}



// Functions //

static bool crypto_init(void)
{
    // Uruchamia backend PSA (RNG, sterowniki, magazyn kluczy).
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_crypto_init: %d", (int)status);
        return false;
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
        return false;
    }

    ESP_LOGI(TAG, "Zaimportowano klucz AES-%d (id=%u)", AES_KEY_BITS, (unsigned)aes_key_id);
    return true;
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

static bool encode_audio_packet(uint8_t* IN_raw_audio, radio_packet_t* OUT_packet_to_send)
{
    psa_status_t status;
    if (aes_key_id == 0)
    {
        ESP_LOGE(TAG, "Brak klucza - najpierw wywolaj crypto_init()");
        return false;
    }
    if (nullptr == IN_raw_audio || nullptr == OUT_packet_to_send)
    {
        ESP_LOGE(TAG, "invalid input");
        return false;
    }

    add_SN(); // increments counter



    // --- Używamy "Multi-part" jako Single-part, by podać WŁASNE IV ---
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

    // 1. Setup operacji szyfrowania
    status = psa_cipher_encrypt_setup(&operation, aes_key_id, PSA_ALG_CTR);

    // 2. WSTRZYKUJEMY WŁASNE IV (Magia, której nie ma w psa_cipher_encrypt)
    status = psa_cipher_set_iv(&operation, IV_key, sizeof(IV_key));

    size_t length_out = 0;

    // 3. Szyfrujemy nasze 30 bajtów bezpośrednio do structa pakietu
    status = psa_cipher_update(&operation,
                               IN_raw_audio,                    PAYLOAD_DATA_BYTE_SIZE,
                               packet_to_send->encrypted_audio, PAYLOAD_DATA_BYTE_SIZE,
                               &length_out);

    // 4. Koniec operacji. W przypadku CTR 'finish' nic nie dopisuje, ale zwalnia RAM na ESP32!
    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, NULL, 0, &finish_len);

    // Gotowe! Możesz wysłać 'packet_to_send' w eter.
    // Ma równe 32 bajty: [2 bajty SEQ][30 bajtów CZYSTEGO SZYFROGRAMU]
    // ESP_NOW_SEND(packet_to_send);

    return true;
}

static bool decode_radio_packet(radio_packet_t* IN_received_packet, uint8_t* OUT_raw_audio)
{
    psa_status_t status;
    if (aes_key_id == 0)
    {
        ESP_LOGE(TAG, "Brak klucza - najpierw wywolaj crypto_init()");
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

    // 1. Setup operacji szyfrowania
    status = psa_cipher_decrypt_setup(&operation, aes_key_id, PSA_ALG_CTR);

    // 2. WSTRZYKUJEMY WŁASNE IV (Magia, której nie ma w psa_cipher_encrypt)
    status = psa_cipher_set_iv(&operation, IV_key, sizeof(IV_key));

    size_t length_out = 0;

    // 3. Deszyfrujemy
    status = psa_cipher_update(&operation,
                               IN_received_packet->encrypted_audio, PAYLOAD_DATA_BYTE_SIZE,
                               OUT_raw_audio,                       PAYLOAD_DATA_BYTE_SIZE,
                               &length_out);

    // 4. Koniec operacji. W przypadku CTR 'finish' nic nie dopisuje, ale zwalnia RAM na ESP32!
    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, NULL, 0, &finish_len);

    return true;
}


#endif