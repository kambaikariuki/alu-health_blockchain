#ifndef ECDSA_H
#define ECDSA_H

int sign_data(const char* signature_out);
int verify_signature(const char* data, const char* signature, const char* public_key);

#endif