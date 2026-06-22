#ifndef INSURANCE_H
#define INSURANCE_H

#define REINSURANCE_CONTRIBUTION_RATE 0.05   /* 5% of every premium */
#define CLAIM_INSURANCE_POOL_CAP 1000.0      /* insurance pool covers up to this much of any single claim;reinsurance covers the rest */

typedef enum {
    SETTLEMENT_FULL = 0,        /* claim paid in full */
    SETTLEMENT_PARTIAL,         /* reinsurance pool couldn't cover the full
                                    excess - paid what it could, flagged for
                                    manual review */
    SETTLEMENT_REJECTED,        /* could not be processed at all (e.g.
                                    insurance pool itself lacks funds for
                                    even the first CLAIM_INSURANCE_POOL_CAP
                                    portion) */
} SettlementResult;

/* Processes a premium payment from member_address:
 *   1. Validates and applies the account-balance transfer: member -> 
 *      Insurance Pool, for the full `amount` (per spec, this is an
 *      account-balance transaction, not the UTXO model - the UTXO side of
 *      this is the next step).
 *   2. Splits off REINSURANCE_CONTRIBUTION_RATE (5%) and applies a second
 *      account-balance transfer: Insurance Pool -> Reinsurance Pool,
 *      recorded as its own REINSURANCE_CONTRIBUTION transaction.
 *   3. Creates a UTXO owned by member_address for `amount` (the unspent
 *      output the spec describes: "A premium payment creates an unspent
 *      output") - tracing back to the premium's transaction_id.
 *
 * ASSUMPTION: the 5% reinsurance contribution is taken OUT of the premium
 * (insurance pool nets 95%, reinsurance pool gets 5%, member pays `amount`
 * total) rather than being an extra 5% added on top of the member's
 * payment. The spec says "Receives 5% of every premium payment" without
 * specifying which side it comes out of - this is the more standard
 * insurance-industry reading, but flagging it as a choice.
 *
 * member_nonce is the member's account nonce to validate against (per the
 * account-balance model's replay protection). Returns 1 on success, 0 if
 * validation failed (insufficient balance, bad nonce, account doesn't
 * exist) - on failure, NOTHING is applied (no partial state change). */
int process_premium_payment(const char *member_address, long member_nonce,
                             double amount, const char *premium_transaction_id);

/* Processes a claim settlement, consuming the member's UTXO and paying out
 * via the account-balance model:
 *   1. Validates the claim_output_id is a legitimate, currently-unspent
 *      UTXO (rejects double-spend / unknown outputs).
 *   2. Consumes that UTXO.
 *   3. If claim_amount <= CLAIM_INSURANCE_POOL_CAP: Insurance Pool pays the
 *      full amount to provider_address in one transaction.
 *   4. If claim_amount > CLAIM_INSURANCE_POOL_CAP: Insurance Pool pays the
 *      first CLAIM_INSURANCE_POOL_CAP, and the Reinsurance Pool pays the
 *      excess (claim_amount - CLAIM_INSURANCE_POOL_CAP), as two SEPARATE
 *      transactions, both to provider_address. If the Reinsurance Pool's
 *      balance is insufficient for the full excess, it pays as much as it
 *      can (down to 0, never negative) and the result is
 *      SETTLEMENT_PARTIAL, signaling manual review is needed for the
 *      shortfall.
 *   5. If the remainder of the original UTXO amount (claim_output's amount
 *      minus claim_amount, if the claim doesn't consume the full output)
 *      is positive, a new UTXO is created for that remainder, owned by the
 *      original claimant - per spec: "creates a new output for any
 *      remainder."
 *
 * Returns a SettlementResult. On SETTLEMENT_REJECTED, no state is changed
 * (UTXO is not consumed, no transfers applied). On SETTLEMENT_FULL or
 * SETTLEMENT_PARTIAL, the UTXO has been consumed and whatever payouts were
 * possible have been applied - SETTLEMENT_PARTIAL specifically means "the
 * insurance-pool portion was paid in full, but the reinsurance portion was
 * short" and should be surfaced to an operator for manual follow-up. */
SettlementResult process_claim_settlement(const char *claim_output_id, double claim_amount,
                                            const char *provider_address);

/* Prints the current reinsurance pool balance, per spec:
 * "The reinsurance_balance command displays the current pool balance." */
void reinsurance_balance();

#endif