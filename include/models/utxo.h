#ifndef UTXO_H
#define UTXO_H

#define UTXO_OUTPUT_ID_LEN 64
#define UTXO_ADDRESS_LEN 64
#define UTXO_INITIAL_CAPACITY 16


typedef enum {
    UTXO_UNSPENT = 0,
    UTXO_SPENT,
} UtxoStatus;

typedef struct {
    char output_id[UTXO_OUTPUT_ID_LEN];
    char owner_address[UTXO_ADDRESS_LEN];
    double amount;
    UtxoStatus status;
    char source_transaction_id[UTXO_OUTPUT_ID_LEN];
} UTXO;


typedef struct {
    UTXO *outputs;     
    int count;
    int capacity;
} UTXOSet;
 
/* Global UTXO set, defined once in utxo.c */
extern UTXOSet utxo_set;
 
void init_utxo_set();
 
int utxo_create(const char *output_id, const char *owner_address, double amount,
                 const char *source_transaction_id);

int utxo_validate(const char *output_id);
 
int utxo_consume(const char *output_id);
UTXO* utxo_find(const char *output_id);
 
double utxo_get_balance(const char *owner_address);
 
void free_utxo_set();
void view_utxo_set();
 
#endif
 
