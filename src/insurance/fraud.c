#include "../../include/core/blockchain.h"
#include "../../include/insurance/fraud.h"
#include "../../include/core/mempool.h"
#include "../../include/models/account.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int count_recent_claims(const char *member_id)
{
    long now = time(NULL);
    long window_start = now - FRAUD_FREQUENCY_WINDOW_SECONDS;
    int count = 0;

    for (int i = 0; i < mempool.count; i++) {
        MempoolEntry *e = &mempool.entries[i];
        if (strcmp(e->sender_address, member_id) == 0 &&
            strcmp(e->transaction_type, "CLAIM_SUBMISSION") == 0 &&
            e->submitted_at >= window_start) {
            count++;
        }
    }

    for (int b = 0; b < blockchain.height; b++) {
        Block *block = &blockchain.blocks[b];
        for (int t = 0; t < block->transaction_count; t++) {
            Transaction *tx = &block->transactions[t];
            if (strcmp(tx->sender_address, member_id) == 0 &&
                strcmp(tx->transaction_type, "CLAIM_SUBMISSION") == 0 &&
                tx->timestamp >= window_start) {
                count++;
            }
        }
    }

    return count;
}

static int transaction_id_exists(const char *transaction_id)
{
    for (int i = 0; i < mempool.count; i++) {
        if (strcmp(mempool.entries[i].transaction_id, transaction_id) == 0) {
            return 1;
        }
    }

    for (int b = 0; b < blockchain.height; b++) {
        Block *block = &blockchain.blocks[b];
        for (int t = 0; t < block->transaction_count; t++) {
            if (strcmp(block->transactions[t].transaction_id, transaction_id) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

FraudReason check_fraud_heuristics(const char *member_id, const char *provider_address,
                                     double claim_amount, const char *transaction_id)
{
    if (transaction_id_exists(transaction_id)) {
        return FRAUD_DUPLICATE_TRANSACTION;
    }

    if (count_recent_claims(member_id) > FRAUD_MAX_CLAIMS_PER_WINDOW) {
        return FRAUD_HIGH_FREQUENCY;
    }

    Account *provider = account_find(provider_address);
    if (provider != NULL && provider->claim_settlement_count > 0) {
        if (claim_amount > provider->claim_average * FRAUD_AMOUNT_MULTIPLIER) {
            return FRAUD_ABNORMAL_AMOUNT;
        }
    }

    return FRAUD_NONE;
}

static const char* fraud_reason_to_string(FraudReason reason)
{
    switch (reason) {
        case FRAUD_NONE:                  return "NONE";
        case FRAUD_HIGH_FREQUENCY:        return "HIGH_FREQUENCY_CLAIMS";
        case FRAUD_ABNORMAL_AMOUNT:       return "ABNORMAL_CLAIM_AMOUNT";
        case FRAUD_DUPLICATE_TRANSACTION: return "DUPLICATE_TRANSACTION";
        default:                          return "UNKNOWN";
    }
}

void fraud_review()
{
    int found = 0;

    printf("===== Fraud Review: SUSPICIOUS transactions =====\n\n");

    for (int i = 0; i < mempool.count; i++) {
        MempoolEntry *e = &mempool.entries[i];
        if (e->status != TX_SUSPICIOUS) {
            continue;
        }
        found++;
        printf("[%d] id:       %s\n", found, e->transaction_id);
        printf("    sender:   %s\n", e->sender_address);
        printf("    receiver: %s\n", e->receiver_address);
        printf("    amount:   %.8f\n", e->amount);
        printf("    type:     %s\n", e->transaction_type);
        printf("    reason:   %s\n\n", fraud_reason_to_string((FraudReason)e->flag_reason));
    }

    if (found == 0) {
        printf("No suspicious transactions currently flagged.\n");
    }

    printf("===== End of fraud review (%d flagged) =====\n", found);
}

void approve_suspicious(const char *transaction_id)
{
    for (int i = 0; i < mempool.count; i++) {
        if (strcmp(mempool.entries[i].transaction_id, transaction_id) == 0) {
            if (mempool.entries[i].status != TX_SUSPICIOUS) {
                printf("approve_suspicious: %s is not currently SUSPICIOUS (status unchanged).\n",
                       transaction_id);
                return;
            }
            mempool.entries[i].status = TX_PENDING;
            printf("Transaction %s approved and returned to PENDING.\n", transaction_id);
            return;
        }
    }
    printf("approve_suspicious: no mempool entry found with id %s.\n", transaction_id);
}

void reject_suspicious(const char *transaction_id)
{
    MempoolEntry *target = NULL;
    for (int i = 0; i < mempool.count; i++) {
        if (strcmp(mempool.entries[i].transaction_id, transaction_id) == 0) {
            target = &mempool.entries[i];
            break;
        }
    }

    if (target == NULL) {
        printf("reject_suspicious: no mempool entry found with id %s.\n", transaction_id);
        return;
    }
    if (target->status != TX_SUSPICIOUS) {
        printf("reject_suspicious: %s is not currently SUSPICIOUS (not removed).\n", transaction_id);
        return;
    }

    mempool_remove(transaction_id);
    printf("Transaction %s rejected and discarded.\n", transaction_id);
}