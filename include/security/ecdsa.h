#ifndef ECDSA_H
#define ECDSA_H

#include <openssl/ec.h> 

EC_KEY* generate_keypair(void);

int sign_data(
    EC_KEY *private_key,
    const char *data,
    unsigned char *signature,
    unsigned int *signature_len
);
int verify_signature(
    EC_KEY *public_key,
    const char *data,
    const unsigned char *signature,\
    unsigned int signature_len
);
void free_keypair(EC_KEY *key);

#endif