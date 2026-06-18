#ifndef MERKLE_H
#define MERKLE_H

#include <transaction.h>

char* compute_merkle_root(Transaction* txs, int Tx_count);
char* hash_transaction(Transaction tx);

#endif