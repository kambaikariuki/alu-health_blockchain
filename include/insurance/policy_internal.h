#ifndef POLICY_INTERNAL_H
#define POLICY_INTERNAL_H

#include "policy.h"

/* These mirror policy.h's four commands but take explicit parameters and
 * return status codes instead of prompting via stdin - for use by other
 * modules (insurance_tx.c) that already have the relevant ids/values in
 * hand and need policy logic as a function call, not an interactive
 * prompt. policy.h itself is left exactly as given; these are declared
 * separately so that header's contents stay untouched. */

#define POLICY_OK                0
#define POLICY_OUT_OF_MEMORY     1
#define POLICY_DUPLICATE_ID      2
#define POLICY_NOT_FOUND         3
#define POLICY_EXPIRED           4

int enroll_policy_internal(const char *member_id, const char *policy_id,
                            const char *coverage_plan);

/* Returns POLICY_OK (not expired), POLICY_EXPIRED (just-expired or already
 * EXPIRED), or POLICY_NOT_FOUND. Updates status to EXPIRED as a side
 * effect if the policy's expiry_date has passed - matches spec section
 * 1.iii: "The system must check policy expiry on every claim submission." */
int check_and_update_policy_expiry(const char *policy_id);

int renew_policy_internal(const char *policy_id);

/* Returns a pointer to the policy, or NULL if not found. */
Policy* find_policy_internal(const char *policy_id);

/* Accessors for persistence.c (chain_save/chain_load) to read and rebuild
 * the policy store, which is otherwise file-local (static) to policy.c -
 * kept as narrow accessor functions rather than exposing the PolicySet
 * struct/global directly, so policy.c's internal storage representation
 * can still change without breaking callers. */

/* Returns the number of policies currently stored. */
int policy_count_internal();

/* Returns a pointer to the i'th stored policy (0-indexed), or NULL if i is
 * out of range. Used by persistence.c to iterate and write every policy
 * to disk. */
Policy* policy_at_internal(int i);

/* Clears all existing policies and replaces the store with exactly the
 * policies in `policies` (count entries). Used by persistence.c on
 * chain_load to rebuild the policy store from a saved snapshot. Returns 0
 * on success, nonzero on allocation failure. */
int policy_load_internal(const Policy *policies, int count);

#endif