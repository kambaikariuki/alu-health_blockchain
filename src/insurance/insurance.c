#include "../../include/models/account.h"
#include "../../include/insurance/insurance.h"
#include "../../include/models/utxo.h"

#include <stdio.h>
#include <string.h>

int process_premium_payment(const char *member_address, long member_nonce,
                             double amount, const char *premium_transaction_id)
{
    if (!account_validate_transaction(member_address, member_nonce, amount)) {
        fprintf(stderr, "process_premium_payment: rejected, validation failed for %s\n",
                member_address);
        return 0;
    }

    /* step 1: member -> Insurance Pool, full premium amount.
       this also advances the member's nonce, since this is the confirmed
       transaction the spec ties nonce-increment to. */
    if (account_apply_transaction(member_address, INSURANCE_POOL_ADDRESS, amount) != 0) {
        fprintf(stderr, "process_premium_payment: failed to apply member->pool transfer\n");
        return 0;
    }

    /* step 2: split off 5% from what the Insurance Pool just received,
       into the Reinsurance Pool, as its own REINSURANCE_CONTRIBUTION
       transfer. This is a pool-to-pool transfer, not subject to member
       nonce/balance validation (pools aren't rate-limited by nonce in the
       same sense a member is - their "sends" are protocol-driven, not
       user-initiated). We still go through account_apply_transaction so
       balances stay consistent, but skip account_validate_transaction's
       nonce check for this internal transfer. */
    double reinsurance_cut = amount * REINSURANCE_CONTRIBUTION_RATE;
    if (account_apply_transaction(INSURANCE_POOL_ADDRESS, REINSURANCE_POOL_ADDRESS, reinsurance_cut) != 0) {
        fprintf(stderr, "process_premium_payment: failed to apply reinsurance contribution\n");
        /* NOTE: at this point the member->pool transfer already happened
           and is not rolled back. A true atomic multi-step transaction
           would need a rollback/journal mechanism, which doesn't exist
           anywhere in this codebase yet. Flagging this as a known gap. */
        return 0;
    }

    /* step 3: create the member's UTXO for the premium, per spec - this is
       what the member can later "spend" via a claim settlement. */
    char output_id[UTXO_OUTPUT_ID_LEN];
    snprintf(output_id, sizeof(output_id), "utxo-%s", premium_transaction_id);

    if (utxo_create(output_id, member_address, amount, premium_transaction_id) != 0) {
        fprintf(stderr, "process_premium_payment: failed to create UTXO\n");
        return 0;
    }

    return 1;
}

SettlementResult process_claim_settlement(const char *claim_output_id, double claim_amount,
                                            const char *provider_address)
{
    if (!utxo_validate(claim_output_id)) {
        fprintf(stderr, "process_claim_settlement: rejected, output %s is not a valid unspent UTXO\n",
                claim_output_id);
        return SETTLEMENT_REJECTED;
    }

    UTXO *claim_output = utxo_find(claim_output_id);
    /* utxo_validate already confirmed this exists and is unspent, so
       claim_output is guaranteed non-NULL here */

    if (claim_amount > claim_output->amount) {
        fprintf(stderr,
                "process_claim_settlement: rejected, claim_amount %.8f exceeds output amount %.8f\n",
                claim_amount, claim_output->amount);
        return SETTLEMENT_REJECTED;
    }

    Account *insurance_pool = account_find(INSURANCE_POOL_ADDRESS);
    double insurance_portion = (claim_amount <= CLAIM_INSURANCE_POOL_CAP)
                                    ? claim_amount
                                    : CLAIM_INSURANCE_POOL_CAP;

    if (insurance_pool->balance < insurance_portion) {
        fprintf(stderr,
                "process_claim_settlement: rejected, insurance pool has insufficient funds "
                "(has %.8f, needs %.8f)\n",
                insurance_pool->balance, insurance_portion);
        return SETTLEMENT_REJECTED;
    }

    /* validation passed for at least the insurance-pool portion - consume
       the UTXO now, since from here on we're committing to the
       settlement (possibly partial, but not rejected). */
    if (utxo_consume(claim_output_id) != 0) {
        /* shouldn't happen, utxo_validate already confirmed spendability
           moments ago, but defensive in case of a future concurrency
           change */
        fprintf(stderr, "process_claim_settlement: unexpected failure consuming UTXO\n");
        return SETTLEMENT_REJECTED;
    }

    SettlementResult result = SETTLEMENT_FULL;

    /* insurance pool pays its portion */
    account_apply_transaction(INSURANCE_POOL_ADDRESS, provider_address, insurance_portion);

    /* reinsurance overflow, if the claim exceeded the cap */
    if (claim_amount > CLAIM_INSURANCE_POOL_CAP) {
        double excess = claim_amount - CLAIM_INSURANCE_POOL_CAP;
        Account *reinsurance_pool = account_find(REINSURANCE_POOL_ADDRESS);

        double reinsurance_payout = excess;
        if (reinsurance_pool->balance < excess) {
            /* reinsurance pool can't cover it all - pay what it has, flag
               for manual review. Pool balance never goes below zero
               because we cap the payout at exactly what's available. */
            reinsurance_payout = reinsurance_pool->balance;
            result = SETTLEMENT_PARTIAL;
            fprintf(stderr,
                    "process_claim_settlement: WARNING, reinsurance pool underfunded "
                    "(has %.8f, needs %.8f) - paying %.8f, flagged for manual review\n",
                    reinsurance_pool->balance, excess, reinsurance_payout);
        }

        if (reinsurance_payout > 0.0) {
            account_apply_transaction(REINSURANCE_POOL_ADDRESS, provider_address, reinsurance_payout);
        }
    }

    /* remainder: if the claim didn't consume the full output amount,
       create a new UTXO for whatever's left, owned by the original
       claimant (per spec: "creates a new output for any remainder") */
    double remainder = claim_output->amount - claim_amount;
    if (remainder > 0.0) {
        char remainder_id[UTXO_OUTPUT_ID_LEN];
        snprintf(remainder_id, sizeof(remainder_id), "%s-remainder", claim_output_id);
        utxo_create(remainder_id, claim_output->owner_address, remainder, claim_output_id);
    }

    /* fold this settled claim into the provider's running average, used by
       fraud detection's abnormal-claim-amount heuristic for FUTURE claims.
       Recorded for both FULL and PARTIAL outcomes - the claim still
       genuinely settled for claim_amount from the provider's perspective,
       even if the reinsurance portion was short. */
    account_record_claim_settlement(provider_address, claim_amount);

    return result;
}

void reinsurance_balance()
{
    Account *pool = account_find(REINSURANCE_POOL_ADDRESS);
    if (pool == NULL) {
        printf("Reinsurance pool account does not exist (was init_accounts() called?).\n");
        return;
    }
    printf("Reinsurance Pool balance: %.8f AHT\n", pool->balance);
}