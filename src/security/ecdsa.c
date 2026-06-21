#include "../../include/security/ecdsa.h"

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>

EC_KEY* generate_keypair(void){
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

    if (!key) {
        return NULL;
    }
    
    if (EC_KEY_generate_key(key) != 1){
        EC_KEY_free(key);
        return NULL;
    } 

    return key;
}

int sign_data(
    EC_KEY *private_key,
    const char *data,
    unsigned char *signature,
    unsigned int *signature_len
)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(
        (const unsigned char*)data,
        strlen(data),
        hash
    );

    if (ECDSA_sign(
        0,
        hash,
        SHA256_DIGEST_LENGTH,
        signature,
        signature_len,
        private_key
    ) != 1)
    {
        return 0;
    }

    return 1;
}

int verify_signature(
    EC_KEY *public_key,
    const char *data,
    const unsigned char *signature,
    unsigned int signature_len
)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(
        (const unsigned char*)data,
        strlen(data),
        hash
    );

    return ECDSA_verify(
        0,
        hash,
        SHA256_DIGEST_LENGTH,
        signature,
        signature_len,
        public_key
    );
}

void free_keypair(EC_KEY *key)
{
    if (key){
        EC_KEY_free(key);
    }
}