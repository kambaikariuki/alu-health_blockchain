#ifndef TRANSACTION_H
#define TRANSACTION_H

#define TX_ID_LEN 64

typedef struct {
    char transaction_id[TX_ID_LEN];
    char sender_address[64];
    char receiver_address[64];

    double amount;
    char transaction_type[32];

    long timestamp;
    long sender_nonce;

    char digital_signature[256];
} Transaction;

#endif