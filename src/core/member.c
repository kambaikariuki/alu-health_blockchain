#include "../../include/models/account.h"
#include "../../include/core/member.h"
#include "../../include/insurance/policy_internal.h"
#include "../../include/models/utxo.h"

#include <stdio.h>
#include <string.h>

int register_member(const char *member_address)
{
    int result = account_create(member_address, ACCOUNT_MEMBER);
    if (result != 0) {
        fprintf(stderr, "register_member: rejected, address %s already exists\n", member_address);
        return MEMBER_DUPLICATE;
    }

    printf("Member %s registered.\n", member_address);
    return MEMBER_OK;
}

int view_member(const char *member_address)
{
    Account *acct = account_find(member_address);
    if (acct == NULL) {
        printf("No account found for %s.\n", member_address);
        return MEMBER_NOT_FOUND;
    }

    printf("===== Member: %s =====\n", member_address);
    printf("  account balance:    %.8f AHT\n", acct->balance);
    printf("  account nonce:      %ld\n", acct->nonce);
    printf("  UTXO balance:       %.8f AHT\n", utxo_get_balance(member_address));

    printf("  policies:\n");
    int policy_count = policy_count_internal();
    int found_any = 0;
    for (int i = 0; i < policy_count; i++) {
        Policy *p = policy_at_internal(i);
        if (strcmp(p->member_id, member_address) != 0) {
            continue;
        }
        found_any = 1;
        printf("    - policy_id: %s | plan: %s | status: %s | expiry: %ld\n",
               p->policy_id, p->coverage_plan, p->status, p->expiry_date);
    }
    if (!found_any) {
        printf("    (none)\n");
    }

    printf("=========================\n");
    return MEMBER_OK;
}