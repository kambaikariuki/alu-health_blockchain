#include "../../include/models/account.h"
#include "../../include/insurance/fraud.h"
#include "../../include/insurance/insurance.h"
#include "../../include/insurance/insurance_tx.h"
#include "../../include/core/mempool.h"
#include "../../include/insurance/policy.h"
#include "../../include/insurance/policy_internal.h"

#include <stdio.h>
#include <string.h>

static int queue_entry(const char *transaction_id, const char *sender, const char *receiver,
                        double amount, const char *type, double fee, TxStatus status)
{
    MempoolEntry e;
    strncpy(e.transaction_id, transaction_id, sizeof(e.transaction_id) - 1);
    e.transaction_id[sizeof(e.transaction_id) - 1] = '\0';
    strncpy(e.sender_address, sender, sizeof(e.sender_address) - 1);
    e.sender_address[sizeof(e.sender_address) - 1] = '\0';
    strncpy(e.receiver_address, receiver, sizeof(e.receiver_address) - 1);
    e.receiver_address[sizeof(e.receiver_address) - 1] = '\0';
    e.amount = amount;
    strncpy(e.transaction_type, type, sizeof(e.transaction_type) - 1);
    e.transaction_type[sizeof(e.transaction_type) - 1] = '\0';
    e.fee = fee;
    e.status = status;
    e.flag_reason = 0;
    /* submitted_at is set by mempool_add itself, regardless of what's
       here - see mempool.c */

    return mempool_add(e) == 0;
}

/* ---------------------------------------------------------------------
 * i. Policy Enrollment
 * --------------------------------------------------------------------- */
int record_policy_enrollment(const char *member_id, const char *policy_id,
                              const char *coverage_plan)
{
    int policy_result = enroll_policy_internal(member_id, policy_id, coverage_plan);
    if (policy_result != POLICY_OK) {
        return 0;
    }

    char tx_id[TX_ID_LEN];
    snprintf(tx_id, sizeof(tx_id), "enroll-%s", policy_id);

    return queue_entry(tx_id, member_id, "POLICY_REGISTRY", 0.0,
                        "POLICY_ENROLLMENT", 0.0, TX_CONFIRMED);
}

/* ---------------------------------------------------------------------
 * ii. Premium Payment
 * --------------------------------------------------------------------- */
int record_premium_payment(const char *member_address, long member_nonce,
                            double amount, const char *transaction_id)
{
    int ok = process_premium_payment(member_address, member_nonce, amount, transaction_id);
    if (!ok) {
        return 0;
    }

    double reinsurance_cut = amount * REINSURANCE_CONTRIBUTION_RATE;

    int q1 = queue_entry(transaction_id, member_address, INSURANCE_POOL_ADDRESS,
                          amount, "PREMIUM_PAYMENT", amount, TX_CONFIRMED);

    char reinsurance_tx_id[TX_ID_LEN];
    snprintf(reinsurance_tx_id, sizeof(reinsurance_tx_id), "%s-reinsurance", transaction_id);
    int q2 = queue_entry(reinsurance_tx_id, INSURANCE_POOL_ADDRESS, REINSURANCE_POOL_ADDRESS,
                          reinsurance_cut, "REINSURANCE_CONTRIBUTION", reinsurance_cut, TX_CONFIRMED);

    /* NOTE: at this point process_premium_payment has already applied the
       real balance/UTXO effects regardless of whether q1/q2 succeeded -
       these mempool entries are an audit-trail record, not a gate. If
       either queue_entry call fails (e.g. duplicate transaction_id - very
       unlikely given transaction_id is caller-supplied and presumably
       unique), the effects still happened; only the on-chain record is
       missing. Flagging this as a known limitation of the audit-trail
       being non-atomic with the effect itself. */
    return q1 && q2;
}

/* ---------------------------------------------------------------------
 * iv. Healthcare Service Request
 * --------------------------------------------------------------------- */
int record_service_request(const char *member_id, const char *provider_id,
                            const char *service_description, const char *transaction_id)
{

    (void)service_description;

    return queue_entry(transaction_id, member_id, provider_id, 0.0,
                        "HEALTHCARE_SERVICE_REQUEST", 0.0, TX_CONFIRMED);
}

/* ---------------------------------------------------------------------
 * v. Pre-Authorization
 * --------------------------------------------------------------------- */
int record_preauthorization(const char *member_id, const char *provider_id,
                             double estimated_amount, int approved,
                             const char *transaction_id)
{
    const char *type = approved ? "PRE_AUTHORIZATION_APPROVAL" : "PRE_AUTHORIZATION_REQUEST";

    return queue_entry(transaction_id, member_id, provider_id, estimated_amount,
                        type, 0.0, TX_CONFIRMED);
}

/* ---------------------------------------------------------------------
 * vi. Claim Submission
 * --------------------------------------------------------------------- */
int submit_claim(const char *member_id, const char *policy_id, const char *provider_address,
                  double claim_amount, const char *transaction_id)
{
    int policy_check = check_and_update_policy_expiry(policy_id);
    if (policy_check == POLICY_NOT_FOUND) {
        fprintf(stderr, "submit_claim: rejected, policy %s does not exist\n", policy_id);
        return 0;
    }
    if (policy_check == POLICY_EXPIRED) {
        fprintf(stderr, "submit_claim: rejected, policy %s is EXPIRED\n", policy_id);
        return 0;
    }

    FraudReason fraud = check_fraud_heuristics(member_id, provider_address,
                                                 claim_amount, transaction_id);

    TxStatus status = (fraud == FRAUD_NONE) ? TX_PENDING : TX_SUSPICIOUS;

    MempoolEntry e;
    strncpy(e.transaction_id, transaction_id, sizeof(e.transaction_id) - 1);
    e.transaction_id[sizeof(e.transaction_id) - 1] = '\0';
    strncpy(e.sender_address, member_id, sizeof(e.sender_address) - 1);
    e.sender_address[sizeof(e.sender_address) - 1] = '\0';
    strncpy(e.receiver_address, provider_address, sizeof(e.receiver_address) - 1);
    e.receiver_address[sizeof(e.receiver_address) - 1] = '\0';
    e.amount = claim_amount;
    strncpy(e.transaction_type, "CLAIM_SUBMISSION", sizeof(e.transaction_type) - 1);
    e.transaction_type[sizeof(e.transaction_type) - 1] = '\0';
    e.fee = claim_amount;
    e.status = status;
    e.flag_reason = (int)fraud;

    int result = mempool_add(e);

    if (fraud != FRAUD_NONE) {
        fprintf(stderr, "submit_claim: flagged SUSPICIOUS (reason code %d), withheld from mining\n",
                (int)fraud);
    }

    return result == 0;
}

/* ---------------------------------------------------------------------
 * vii. Claim Approval and Rejection
 * --------------------------------------------------------------------- */
int record_claim_decision(const char *claim_transaction_id, const char *decided_by,
                           int approved, const char *decision_transaction_id)
{
    const char *type = approved ? "CLAIM_APPROVAL" : "CLAIM_REJECTION";

    /* sender is recorded as the decider (e.g. an insurer/operator id);
       receiver references the original claim's transaction_id, since the
       decision is "about" that claim, not a transfer to a wallet address.
       amount carries 0.0 - this is a decision record, not a fund
       movement (that happens separately via record_claim_settlement for
       approvals). */
    return queue_entry(decision_transaction_id, decided_by, claim_transaction_id, 0.0,
                        type, 0.0, TX_CONFIRMED);
}

/* ---------------------------------------------------------------------
 * viii. Claim Settlement
 * --------------------------------------------------------------------- */
int record_claim_settlement(const char *claim_output_id, double approved_amount,
                             const char *provider_address, const char *transaction_id)
{
    SettlementResult result = process_claim_settlement(claim_output_id, approved_amount,
                                                          provider_address);
    if (result == SETTLEMENT_REJECTED) {
        return 0;
    }

    double insurance_portion = (approved_amount <= CLAIM_INSURANCE_POOL_CAP)
                                    ? approved_amount
                                    : CLAIM_INSURANCE_POOL_CAP;

    int q1 = queue_entry(transaction_id, INSURANCE_POOL_ADDRESS, provider_address,
                          insurance_portion, "CLAIM_SETTLEMENT", insurance_portion, TX_CONFIRMED);

    int q2 = 1;
    if (approved_amount > CLAIM_INSURANCE_POOL_CAP) {
        double excess = approved_amount - CLAIM_INSURANCE_POOL_CAP;
        char reinsurance_tx_id[TX_ID_LEN];
        snprintf(reinsurance_tx_id, sizeof(reinsurance_tx_id), "%s-reinsurance", transaction_id);

        /* NOTE: this records the FULL excess as the settlement record,
           even though process_claim_settlement may have only PARTIALLY
           paid it if the reinsurance pool was underfunded (result ==
           SETTLEMENT_PARTIAL). The actual amount transferred may be less
           than what's recorded here. ASSUMPTION: the record reflects the
           claim's approved/intended settlement structure (first 1000 from
           insurance, remainder from reinsurance), while the underlying
           account balances reflect what was ACTUALLY paid (capped at
           available funds) - these can diverge for a PARTIAL settlement,
           and that divergence IS the signal that manual review is needed
           (process_claim_settlement already prints a stderr warning for
           this case). If you want the recorded amount to reflect only
           what was actually paid, this would need process_claim_settlement
           to return the actual reinsurance payout, not just a result
           enum - currently it doesn't expose that number. */
        q2 = queue_entry(reinsurance_tx_id, REINSURANCE_POOL_ADDRESS, provider_address,
                          excess, "CLAIM_SETTLEMENT", excess, TX_CONFIRMED);
    }

    return q1 && q2;
}

/* ---------------------------------------------------------------------
 * ix. Token Transfer
 * --------------------------------------------------------------------- */
int record_token_transfer(const char *sender_address, const char *receiver_address,
                           long sender_nonce, double amount, const char *transaction_id)
{
    if (!account_validate_transaction(sender_address, sender_nonce, amount)) {
        return 0;
    }

    if (account_apply_transaction(sender_address, receiver_address, amount) != 0) {
        return 0;
    }

    return queue_entry(transaction_id, sender_address, receiver_address, amount,
                        "TOKEN_TRANSFER", amount, TX_CONFIRMED);
}