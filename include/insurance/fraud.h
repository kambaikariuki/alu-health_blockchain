#ifndef FRAUD_H
#define FRAUD_H

#define FRAUD_FREQUENCY_WINDOW_SECONDS (24L * 60L * 60L)  /* 24 hours */
#define FRAUD_MAX_CLAIMS_PER_WINDOW 3                      /* "more than 3"
                                                                means 4+ is
                                                                the trigger */
#define FRAUD_AMOUNT_MULTIPLIER 2.0   /* claim > 2x provider's historical
                                          average triggers the heuristic */

typedef enum {
    FRAUD_NONE = 0, 
    FRAUD_HIGH_FREQUENCY,
    FRAUD_ABNORMAL_AMOUNT,
    FRAUD_DUPLICATE_TRANSACTION,   
} FraudReason;

FraudReason check_fraud_heuristics(const char *member_id, const char *provider_address,
                                     double claim_amount, const char *transaction_id);

void approve_suspicious(const char *transaction_id);

/* Removes a SUSPICIOUS mempool entry entirely (discards it). No-op (with a
 * message) if the entry doesn't exist or isn't currently SUSPICIOUS. */
void reject_suspicious(const char *transaction_id);
void fraud_review();

#endif