#include "../../include/core/merkle.h"
#include "../../include/security/sha256.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_LEVEL 1024

void hash_transaction(const Transaction *tx, char output[HASH_SIZE])
{
    char buffer[512];

    snprintf(buffer, sizeof(buffer),
        "%s|%s|%lf|%s|%ld|%ld|%s|",
        tx->sender_address,
        tx->receiver_address,
        tx->amount,
        tx->transaction_type,
        tx->timestamp,
        tx->sender_nonce,
        tx->transaction_id
    );

    sha256(buffer, output);

}

static void hash_pair(const char *a, const char *b, char output[HASH_SIZE])
{
    char combined[HASH_SIZE * 2 + 1];

    strcpy(combined, a);
    strcat(combined, b);

    sha256(combined, output);
}

void compute_merkle_root(const Transaction *txs, int tx_count, char output[HASH_SIZE])
{
    if (tx_count <= 0) {
        memset(output, 0, HASH_SIZE);
        return;
    }
    if (tx_count > MAX_LEVEL) {
        
        memset(output, 0, HASH_SIZE);
        return;
    }

    char *current[MAX_LEVEL];
    char *next[MAX_LEVEL];

    for (int i = 0; i < tx_count; i++) {
        current[i] = malloc(HASH_SIZE);
        hash_transaction(&txs[i], current[i]);
    }

    int current_count = tx_count;

    while (current_count > 1) {
        int next_count = 0;

        for (int i = 0; i < current_count; i += 2) {
            next[next_count] = malloc(HASH_SIZE);

            if (i + 1 < current_count) {
                hash_pair(current[i], current[i + 1], next[next_count]);
            } else {
                hash_pair(current[i], current[i], next[next_count]);
            }
            next_count++;
        }

        for (int i = 0; i < current_count; i++) {
            free(current[i]);
        }
        for (int i = 0; i < next_count; i++) {
            current[i] = next[i];
        }

        current_count = next_count;
    }

    memcpy(output, current[0], HASH_SIZE);
    free(current[0]);
}