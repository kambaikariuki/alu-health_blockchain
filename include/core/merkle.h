#ifndef MERKLE_H
#define MERKLE_H

#define HASH_SIZE 65

#include "../transactions/transaction.h"

void compute_merkle_root(const Transaction* txs, int Tx_count, char output[HASH_SIZE]);
void hash_transaction(const Transaction *tx, char output[HASH_SIZE]);

#endif