#ifndef CLAIMS_H
#define CLAIMS_H

typedef struct {
    char claim_id[64];
    char member_id[64];
    double amount;
    char status[16];
    long timestamp;
} Claim;

void submit_claim();
void approve_claim();
void reject_claim();

#endif