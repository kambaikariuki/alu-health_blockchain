#ifndef UTXO_H
#define UTXO_H

typedef struct {
    char utxo_id[64];
    char owner_address[64];
    double amount;
    int spent;
} UTXO;

void create_utxo(char* owner, double amount);
int spend_utxo(char* utxo_id);
int validate_utxo(char* address);

#endif