#include "../../include/models/utxo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UTXO_OK                  0
#define UTXO_OUT_OF_MEMORY       1
#define UTXO_DUPLICATE_ID        2
#define UTXO_NOT_FOUND           3
#define UTXO_ALREADY_SPENT       4

UTXOSet utxo_set;

void init_utxo_set()
{
    utxo_set.outputs = malloc(sizeof(UTXO) * UTXO_INITIAL_CAPACITY);
    if (utxo_set.outputs == NULL) {
        fprintf(stderr, "FATAL: failed to allocate initial UTXO set\n");
        exit(EXIT_FAILURE);
    }
    utxo_set.count = 0;
    utxo_set.capacity = UTXO_INITIAL_CAPACITY;
}

UTXO* utxo_find(const char *output_id)
{
    for (int i = 0; i < utxo_set.count; i++) {
        if (strcmp(utxo_set.outputs[i].output_id, output_id) == 0) {
            return &utxo_set.outputs[i];
        }
    }
    return NULL;
}

int utxo_create(const char *output_id, const char *owner_address, double amount,
                 const char *source_transaction_id)
{
    if (utxo_find(output_id) != NULL) {
        fprintf(stderr, "utxo_create: rejected, duplicate output_id %s\n", output_id);
        return UTXO_DUPLICATE_ID;
    }

    if (utxo_set.count >= utxo_set.capacity) {
        int new_capacity = utxo_set.capacity * 2;
        UTXO *resized = realloc(utxo_set.outputs, sizeof(UTXO) * new_capacity);
        if (resized == NULL) {
            fprintf(stderr, "utxo_create: out of memory growing UTXO set\n");
            return UTXO_OUT_OF_MEMORY;
        }
        utxo_set.outputs = resized;
        utxo_set.capacity = new_capacity;
    }

    UTXO *out = &utxo_set.outputs[utxo_set.count];
    strncpy(out->output_id, output_id, UTXO_OUTPUT_ID_LEN - 1);
    out->output_id[UTXO_OUTPUT_ID_LEN - 1] = '\0';
    strncpy(out->owner_address, owner_address, UTXO_ADDRESS_LEN - 1);
    out->owner_address[UTXO_ADDRESS_LEN - 1] = '\0';
    out->amount = amount;
    out->status = UTXO_UNSPENT;
    strncpy(out->source_transaction_id, source_transaction_id, UTXO_OUTPUT_ID_LEN - 1);
    out->source_transaction_id[UTXO_OUTPUT_ID_LEN - 1] = '\0';

    utxo_set.count++;
    return UTXO_OK;
}

int utxo_validate(const char *output_id)
{
    UTXO *out = utxo_find(output_id);
    if (out == NULL) {
        return 0;
    }
    return out->status == UTXO_UNSPENT;
}

int utxo_consume(const char *output_id)
{
    UTXO *out = utxo_find(output_id);
    if (out == NULL) {
        fprintf(stderr, "utxo_consume: rejected, output_id %s does not exist\n", output_id);
        return UTXO_NOT_FOUND;
    }
    if (out->status == UTXO_SPENT) {
        fprintf(stderr, "utxo_consume: rejected, output_id %s already spent (double-spend attempt)\n",
                output_id);
        return UTXO_ALREADY_SPENT;
    }

    out->status = UTXO_SPENT;
    return UTXO_OK;
}

double utxo_get_balance(const char *owner_address)
{
    double total = 0.0;
    for (int i = 0; i < utxo_set.count; i++) {
        if (utxo_set.outputs[i].status == UTXO_UNSPENT &&
            strcmp(utxo_set.outputs[i].owner_address, owner_address) == 0) {
            total += utxo_set.outputs[i].amount;
        }
    }
    return total;
}

void free_utxo_set()
{
    free(utxo_set.outputs);
    utxo_set.outputs = NULL;
    utxo_set.count = 0;
    utxo_set.capacity = 0;
}

static const char* status_to_string(UtxoStatus status)
{
    switch (status) {
        case UTXO_UNSPENT: return "UNSPENT";
        case UTXO_SPENT:   return "SPENT";
        default:           return "UNKNOWN";
    }
}

void view_utxo_set()
{
    if (utxo_set.count <= 0) {
        printf("UTXO set is empty.\n");
        return;
    }

    printf("===== UTXO Set (%d output%s) =====\n\n",
           utxo_set.count, utxo_set.count == 1 ? "" : "s");

    for (int i = 0; i < utxo_set.count; i++) {
        UTXO *u = &utxo_set.outputs[i];
        printf("[%d] output_id: %s\n", i, u->output_id);
        printf("    owner:     %s\n", u->owner_address);
        printf("    amount:    %.8f\n", u->amount);
        printf("    status:    %s\n", status_to_string(u->status));
        printf("    source_tx: %s\n\n", u->source_transaction_id);
    }

    printf("===== End of UTXO set =====\n");
}