#include "../../include/insurance/policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define POLICY_INITIAL_CAPACITY 16
#define POLICY_DURATION_SECONDS (365L * 24L * 60L * 60L)

#define POLICY_OK                0
#define POLICY_OUT_OF_MEMORY     1
#define POLICY_DUPLICATE_ID      2
#define POLICY_NOT_FOUND         3
#define POLICY_EXPIRED           4

/* ASSUMPTION: policy.h declares the Policy struct but no store/array type
 * and no extern global - it's a narrow header exposing only four no-arg
 * "command" functions. A PolicySet store is needed somewhere for those
 * commands to actually have persistent state to read/write, so it's
 * defined here, file-local to policy.c (static), rather than added to the
 * header, since you specified policy.h's exact contents. If other files
 * eventually need direct access to policy data (not just through these
 * four commands), this would need to move into policy.h as an extern,
 * same pattern as mempool/account/utxo. */
typedef struct {
    Policy *policies;
    int count;
    int capacity;
} PolicySet;

static PolicySet policy_set = { NULL, 0, 0 };

/* Lazily initializes the policy store on first use, since policy.h has no
 * init_policies() function declared for main() to call explicitly. */
static void ensure_policy_store_initialized()
{
    if (policy_set.policies == NULL) {
        policy_set.policies = malloc(sizeof(Policy) * POLICY_INITIAL_CAPACITY);
        if (policy_set.policies == NULL) {
            fprintf(stderr, "FATAL: failed to allocate policy store\n");
            exit(EXIT_FAILURE);
        }
        policy_set.count = 0;
        policy_set.capacity = POLICY_INITIAL_CAPACITY;
    }
}

static Policy* find_policy(const char *policy_id)
{
    for (int i = 0; i < policy_set.count; i++) {
        if (strcmp(policy_set.policies[i].policy_id, policy_id) == 0) {
            return &policy_set.policies[i];
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------
 * Internal, parameterized, testable versions of each command's logic.
 * The public no-arg functions (matching policy.h exactly) wrap these
 * with stdin prompts. insurance_tx.c (built separately) calls these
 * internal versions directly, since it has the relevant ids/values
 * already in hand and has no reason to go through a stdin round-trip.
 * --------------------------------------------------------------------- */

/* Creates a new policy. expiry_date is set automatically to
 * enrollment_date + 365 days per spec. status starts ACTIVE.
 * Returns 0 on success, nonzero (POLICY_DUPLICATE_ID/OUT_OF_MEMORY) on
 * failure. */
int enroll_policy_internal(const char *member_id, const char *policy_id,
                            const char *coverage_plan)
{
    ensure_policy_store_initialized();

    if (find_policy(policy_id) != NULL) {
        fprintf(stderr, "enroll_policy: rejected, duplicate policy_id %s\n", policy_id);
        return POLICY_DUPLICATE_ID;
    }

    if (policy_set.count >= policy_set.capacity) {
        int new_capacity = policy_set.capacity * 2;
        Policy *resized = realloc(policy_set.policies, sizeof(Policy) * new_capacity);
        if (resized == NULL) {
            fprintf(stderr, "enroll_policy: out of memory growing policy store\n");
            return POLICY_OUT_OF_MEMORY;
        }
        policy_set.policies = resized;
        policy_set.capacity = new_capacity;
    }

    Policy *p = &policy_set.policies[policy_set.count];
    strncpy(p->member_id, member_id, sizeof(p->member_id) - 1);
    p->member_id[sizeof(p->member_id) - 1] = '\0';
    strncpy(p->policy_id, policy_id, sizeof(p->policy_id) - 1);
    p->policy_id[sizeof(p->policy_id) - 1] = '\0';
    strncpy(p->coverage_plan, coverage_plan, sizeof(p->coverage_plan) - 1);
    p->coverage_plan[sizeof(p->coverage_plan) - 1] = '\0';

    p->enrollment_date = time(NULL);
    p->expiry_date = p->enrollment_date + POLICY_DURATION_SECONDS;
    strncpy(p->status, "ACTIVE", sizeof(p->status) - 1);
    p->status[sizeof(p->status) - 1] = '\0';

    policy_set.count++;
    return POLICY_OK;
}

/* Checks whether policy_id exists and, if its expiry_date has passed,
 * updates its status to EXPIRED (per spec: "checked on every claim
 * submission"). Returns:
 *   POLICY_OK       - exists and is not expired (could be ACTIVE or
 *                      RENEWED; both are valid non-expired states)
 *   POLICY_EXPIRED  - existed, was past expiry, status just set to
 *                      EXPIRED (or already was EXPIRED)
 *   POLICY_NOT_FOUND - no such policy_id
 * This is the function claim submission (in insurance_tx.c) should call
 * before allowing a claim through, per spec: "No claim may be submitted
 * against a policy with status EXPIRED or a policy that does not exist." */
int check_and_update_policy_expiry(const char *policy_id)
{
    ensure_policy_store_initialized();

    Policy *p = find_policy(policy_id);
    if (p == NULL) {
        return POLICY_NOT_FOUND;
    }

    if (strcmp(p->status, "EXPIRED") == 0) {
        return POLICY_EXPIRED;
    }

    if (time(NULL) > p->expiry_date) {
        strncpy(p->status, "EXPIRED", sizeof(p->status) - 1);
        p->status[sizeof(p->status) - 1] = '\0';
        return POLICY_EXPIRED;
    }

    return POLICY_OK;
}

/* Resets expiry_date to now + 365 days and sets status to RENEWED.
 * Returns 0 on success, POLICY_NOT_FOUND if policy_id doesn't exist.
 * Per spec, this works regardless of current status (including renewing
 * an EXPIRED policy back into good standing) - the spec doesn't restrict
 * renew_policy to only-non-expired policies, and renewal existing as a
 * remedy for expiry is the natural reading of "Policy Expiry and
 * Renewal" being one combined section. */
int renew_policy_internal(const char *policy_id)
{
    ensure_policy_store_initialized();

    Policy *p = find_policy(policy_id);
    if (p == NULL) {
        fprintf(stderr, "renew_policy: rejected, policy_id %s not found\n", policy_id);
        return POLICY_NOT_FOUND;
    }

    p->expiry_date = time(NULL) + POLICY_DURATION_SECONDS;
    strncpy(p->status, "RENEWED", sizeof(p->status) - 1);
    p->status[sizeof(p->status) - 1] = '\0';

    return POLICY_OK;
}

Policy* find_policy_internal(const char *policy_id)
{
    ensure_policy_store_initialized();
    return find_policy(policy_id);
}

static void print_policy(const Policy *p)
{
    char enroll_buf[32], expiry_buf[32];
    time_t et = p->enrollment_date;
    time_t xt = p->expiry_date;
    strncpy(enroll_buf, ctime(&et), sizeof(enroll_buf) - 1);
    enroll_buf[sizeof(enroll_buf) - 1] = '\0';
    strncpy(expiry_buf, ctime(&xt), sizeof(expiry_buf) - 1);
    expiry_buf[sizeof(expiry_buf) - 1] = '\0';
    /* ctime() output includes a trailing newline already; strncpy above
       may have truncated it off given the small buffer, so don't assume
       either string ends in \n here */

    printf("  member_id:       %s\n", p->member_id);
    printf("  policy_id:       %s\n", p->policy_id);
    printf("  coverage_plan:   %s\n", p->coverage_plan);
    printf("  enrollment_date: %ld\n", p->enrollment_date);
    printf("  expiry_date:     %ld\n", p->expiry_date);
    printf("  status:          %s\n", p->status);
}

/* ---------------------------------------------------------------------
 * Public, no-arg "command" functions matching policy.h exactly. Each
 * prompts for whatever input it needs via stdin, per the spec's framing
 * of these as CLI commands (e.g. "The renew_policy command resets...").
 * --------------------------------------------------------------------- */

void enroll_policy()
{
    char member_id[64], policy_id[64], coverage_plan[64];

    printf("Enter member_id: ");
    if (scanf("%63s", member_id) != 1) { fprintf(stderr, "enroll_policy: input error\n"); return; }
    printf("Enter policy_id: ");
    if (scanf("%63s", policy_id) != 1) { fprintf(stderr, "enroll_policy: input error\n"); return; }
    printf("Enter coverage_plan: ");
    if (scanf("%63s", coverage_plan) != 1) { fprintf(stderr, "enroll_policy: input error\n"); return; }

    int result = enroll_policy_internal(member_id, policy_id, coverage_plan);
    if (result == POLICY_OK) {
        printf("Policy %s enrolled for member %s.\n", policy_id, member_id);
    }
}

void view_policy()
{
    ensure_policy_store_initialized();

    char policy_id[64];
    printf("Enter policy_id: ");
    if (scanf("%63s", policy_id) != 1) { fprintf(stderr, "view_policy: input error\n"); return; }

    Policy *p = find_policy(policy_id);
    if (p == NULL) {
        printf("No policy found with policy_id %s.\n", policy_id);
        return;
    }

    print_policy(p);
}

void renew_policy()
{
    char policy_id[64];
    printf("Enter policy_id: ");
    if (scanf("%63s", policy_id) != 1) { fprintf(stderr, "renew_policy: input error\n"); return; }

    int result = renew_policy_internal(policy_id);
    if (result == POLICY_OK) {
        printf("Policy %s renewed.\n", policy_id);
    }
}

void policy_status()
{
    ensure_policy_store_initialized();

    char policy_id[64];
    printf("Enter policy_id: ");
    if (scanf("%63s", policy_id) != 1) { fprintf(stderr, "policy_status: input error\n"); return; }

    int result = check_and_update_policy_expiry(policy_id);
    if (result == POLICY_NOT_FOUND) {
        printf("No policy found with policy_id %s.\n", policy_id);
        return;
    }

    Policy *p = find_policy(policy_id);
    print_policy(p);
}

/* ---------------------------------------------------------------------
 * Persistence accessors (see policy_internal.h) - used by persistence.c
 * to save/load the policy store without exposing the static PolicySet
 * directly.
 * --------------------------------------------------------------------- */

int policy_count_internal()
{
    ensure_policy_store_initialized();
    return policy_set.count;
}

Policy* policy_at_internal(int i)
{
    ensure_policy_store_initialized();
    if (i < 0 || i >= policy_set.count) {
        return NULL;
    }
    return &policy_set.policies[i];
}

int policy_load_internal(const Policy *policies, int count)
{
    ensure_policy_store_initialized();

    if (count > policy_set.capacity) {
        int new_capacity = count;
        Policy *resized = realloc(policy_set.policies, sizeof(Policy) * new_capacity);
        if (resized == NULL) {
            fprintf(stderr, "policy_load_internal: out of memory loading %d policies\n", count);
            return POLICY_OUT_OF_MEMORY;
        }
        policy_set.policies = resized;
        policy_set.capacity = new_capacity;
    }

    memcpy(policy_set.policies, policies, sizeof(Policy) * count);
    policy_set.count = count;

    return POLICY_OK;
}