#include "../../include/security/sha256.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>


void sha256(const char *input, char output[SHA256_HEX_SIZE]){
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(
        (const unsigned char *)input,
        strlen(input),
        hash
    );

    for (int i= 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(
            output + (i * 2),
            "%02x",
            hash[i]
        );
    }

    output[64] = '\0';
}
