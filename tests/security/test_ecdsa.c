#include <stdio.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>

#include "../../include/security/ecdsa.h"

int main(void)
{
    EC_KEY *key = generate_keypair();

    unsigned char signature[256];
    unsigned int sig_len = 0;

    const char *message =
        "Premium Payment 100 AHT";

    sign_data(
        key,
        message,
        signature,
        &sig_len
    );

    int valid = verify_signature(
        key,
        message,
        signature,
        sig_len
    );

    printf(
        "ECDSA TEST\n"
        "Signature valid: %s\n",
        valid == 1 ? "YES" : "NO"
    );

    free_keypair(key);

    return 0;
}