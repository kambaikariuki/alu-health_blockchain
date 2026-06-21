#include <stdio.h>
#include <string.h>

#include "../../include/core/merkle.h"
#include "../../include/transactions/transaction.h"

int main(void)
{
    Transaction txs[3];

    strcpy(txs[0].sender_address, "A");
    strcpy(txs[0].receiver_address, "B");
    txs[0].amount = 10;
    strcpy(txs[0].transaction_type, "PAY");
    txs[0].timestamp = 1;
    txs[0].sender_nonce = 1;
    strcpy(txs[0].transaction_id, "tx1");

    strcpy(txs[1].sender_address, "C");
    strcpy(txs[1].receiver_address, "D");
    txs[1].amount = 20;
    strcpy(txs[1].transaction_type, "PAY");
    txs[1].timestamp = 2;
    txs[1].sender_nonce = 1;
    strcpy(txs[1].transaction_id, "tx2");

    strcpy(txs[2].sender_address, "E");
    strcpy(txs[2].receiver_address, "F");
    txs[2].amount = 30;
    strcpy(txs[2].transaction_type, "PAY");
    txs[2].timestamp = 3;
    txs[2].sender_nonce = 1;
    strcpy(txs[2].transaction_id, "tx3");

    char root1[HASH_SIZE];
    char root2[HASH_SIZE];
     compute_merkle_root(txs, 3, root1);
    compute_merkle_root(txs, 3, root2);

    printf("Merkle Root 1: %s\n", root1);
    printf("Merkle Root 2: %s\n", root2);

    if (strcmp(root1, root2) == 0)
    {
        printf("TEST PASSED: deterministic Merkle root\n");
    }
    else
    {
        printf("TEST FAILED: mismatch in Merkle root\n");
    }

    char empty_root[HASH_SIZE];
    compute_merkle_root(txs, 0, empty_root);

    printf("Empty Merkle Root: %s\n", empty_root);

    return 0;
}