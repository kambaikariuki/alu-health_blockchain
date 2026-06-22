#include "include/models/account.h"
#include "include/core/blockchain.h"
#include "include/core/chainstate.h"
#include "include/insurance/fraud.h"
#include "include/insurance/insurance.h"
#include "include/insurance/insurance_tx.h"
#include "include/core/member.h"
#include "include/core/mempool.h"
#include "include/core/mining.h"
#include "include/core/persistence.h"
#include "include/insurance/policy.h"
#include "include/insurance/policy_internal.h"
#include "include/core/pow.h"
#include "include/core/token.h"
#include "include/models/utxo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ARGS 16
#define MAX_LINE 512

/* -----------------------------------------------------------------------
 * Argument tokenizer: splits a line into at most MAX_ARGS tokens
 * in-place (modifies line). Returns token count.
 * --------------------------------------------------------------------- */
static int tokenize(char *line, char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

static unsigned int tx_seq = 0;
static void make_tx_id(char *buf, size_t len, const char *prefix) {
    snprintf(buf, len, "%s-%u", prefix, ++tx_seq);
}

static void cmd_register_member(int argc, char **argv)
{
    if (argc < 2) { printf("usage: register_member <address>\n"); return; }
    register_member(argv[1]);
}

static void cmd_view_member(int argc, char **argv)
{
    if (argc < 2) { printf("usage: view_member <address>\n"); return; }
    view_member(argv[1]);
}

static void cmd_wallet_balance(int argc, char **argv)
{
    if (argc < 2) { printf("usage: wallet_balance <address>\n"); return; }
    Account *a = account_find(argv[1]);
    if (!a) { printf("No account found for %s.\n", argv[1]); return; }
    printf("%s: %.8f AHT (account) | %.8f AHT (UTXO)\n",
           argv[1], a->balance, utxo_get_balance(argv[1]));
}

static void cmd_enroll_policy(int argc, char **argv)
{
    if (argc < 4) { printf("usage: enroll_policy <member_id> <policy_id> <coverage_plan>\n"); return; }
    int r = record_policy_enrollment(argv[1], argv[2], argv[3]);
    if (r) printf("Policy %s enrolled for member %s.\n", argv[2], argv[1]);
}

static void cmd_view_policy(int argc, char **argv)
{
    (void)argc; (void)argv;
    view_policy();   /* prompts via stdin per policy.h's fixed interface */
}

static void cmd_renew_policy(int argc, char **argv)
{
    (void)argc; (void)argv;
    renew_policy();
}

static void cmd_policy_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    policy_status();
}

static void cmd_token_transfer(int argc, char **argv)
{
    if (argc < 5) { printf("usage: token_transfer <sender> <receiver> <nonce> <amount>\n"); return; }
    long nonce = atol(argv[3]);
    double amount = atof(argv[4]);
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "transfer");
    int r = record_token_transfer(argv[1], argv[2], nonce, amount, tx_id);
    if (r) printf("Transfer of %.8f AHT from %s to %s recorded.\n", amount, argv[1], argv[2]);
}

static void cmd_token_balance(int argc, char **argv)
{
    if (argc < 2) { printf("usage: token_balance <address>\n"); return; }
    Account *a = account_find(argv[1]);
    if (!a) { printf("No account: %s.\n", argv[1]); return; }
    printf("%s balance: %.8f AHT\n", argv[1], a->balance);
}

static void cmd_pay_premium(int argc, char **argv)
{
    if (argc < 4) { printf("usage: pay_premium <member_address> <nonce> <amount>\n"); return; }
    long nonce = atol(argv[2]);
    double amount = atof(argv[3]);
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "premium");
    int r = record_premium_payment(argv[1], nonce, amount, tx_id);
    if (r) printf("Premium payment of %.8f AHT recorded.\n", amount);
}

static void cmd_service_request(int argc, char **argv)
{
    if (argc < 4) { printf("usage: service_request <member_id> <provider_id> <description>\n"); return; }
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "service");
    record_service_request(argv[1], argv[2], argv[3], tx_id);
    printf("Service request recorded.\n");
}

static void cmd_preauth_request(int argc, char **argv)
{
    if (argc < 4) { printf("usage: preauth_request <member_id> <provider_id> <estimated_amount>\n"); return; }
    double amount = atof(argv[3]);
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "preauth-req");
    record_preauthorization(argv[1], argv[2], amount, 0, tx_id);
    printf("Pre-authorization request recorded.\n");
}

static void cmd_preauth_approve(int argc, char **argv)
{
    if (argc < 4) { printf("usage: preauth_approve <member_id> <provider_id> <estimated_amount>\n"); return; }
    double amount = atof(argv[3]);
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "preauth-approve");
    record_preauthorization(argv[1], argv[2], amount, 1, tx_id);
    printf("Pre-authorization approval recorded.\n");
}

static void cmd_submit_claim(int argc, char **argv)
{
    if (argc < 5) { printf("usage: submit_claim <member_id> <policy_id> <provider_address> <amount>\n"); return; }
    double amount = atof(argv[4]);
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "claim");
    int r = submit_claim(argv[1], argv[2], argv[3], amount, tx_id);
    if (r) printf("Claim %s submitted.\n", tx_id);
}

static void cmd_approve_claim(int argc, char **argv)
{
    if (argc < 3) { printf("usage: approve_claim <claim_tx_id> <decided_by>\n"); return; }
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "decision");
    record_claim_decision(argv[1], argv[2], 1, tx_id);
    printf("Claim %s approved.\n", argv[1]);
}

static void cmd_reject_claim(int argc, char **argv)
{
    if (argc < 3) { printf("usage: reject_claim <claim_tx_id> <decided_by>\n"); return; }
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "decision");
    record_claim_decision(argv[1], argv[2], 0, tx_id);
    printf("Claim %s rejected.\n", argv[1]);
}

static void cmd_settle_claim(int argc, char **argv)
{
    if (argc < 4) { printf("usage: settle_claim <utxo_output_id> <approved_amount> <provider_address>\n"); return; }
    double amount = atof(argv[2]);
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "settlement");
    int r = record_claim_settlement(argv[1], amount, argv[3], tx_id);
    if (r) printf("Claim settled: %.8f AHT to %s.\n", amount, argv[3]);
}

static void cmd_reinsurance_balance(int argc, char **argv)
{
    (void)argc; (void)argv;
    reinsurance_balance();
}

static void cmd_create_transaction(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Transaction types:\n");
    printf("  1) policy_enrollment  2) premium_payment  3) service_request\n");
    printf("  4) preauth_request    5) preauth_approve  6) submit_claim\n");
    printf("  7) approve_claim      8) reject_claim     9) settle_claim\n");
    printf("  0) token_transfer\n");
    printf("Select type: ");
    int choice = 0;
    if (scanf("%d", &choice) != 1) return;
    getchar(); /* consume newline */

    char a[64], b[64], c[64], d[64];
    double amount;
    long nonce;
    char tx_id[64];
    make_tx_id(tx_id, sizeof(tx_id), "manual");

    switch (choice) {
        case 1:
            printf("member_id: "); scanf("%63s", a);
            printf("policy_id: "); scanf("%63s", b);
            printf("coverage_plan: "); scanf("%63s", c);
            record_policy_enrollment(a, b, c);
            break;
        case 2:
            printf("member_address: "); scanf("%63s", a);
            printf("nonce: "); scanf("%ld", &nonce);
            printf("amount: "); scanf("%lf", &amount);
            record_premium_payment(a, nonce, amount, tx_id);
            break;
        case 3:
            printf("member_id: "); scanf("%63s", a);
            printf("provider_id: "); scanf("%63s", b);
            printf("description: "); scanf("%63s", c);
            record_service_request(a, b, c, tx_id);
            break;
        case 4:
            printf("member_id: "); scanf("%63s", a);
            printf("provider_id: "); scanf("%63s", b);
            printf("estimated_amount: "); scanf("%lf", &amount);
            record_preauthorization(a, b, amount, 0, tx_id);
            break;
        case 5:
            printf("member_id: "); scanf("%63s", a);
            printf("provider_id: "); scanf("%63s", b);
            printf("estimated_amount: "); scanf("%lf", &amount);
            record_preauthorization(a, b, amount, 1, tx_id);
            break;
        case 6:
            printf("member_id: "); scanf("%63s", a);
            printf("policy_id: "); scanf("%63s", b);
            printf("provider_address: "); scanf("%63s", c);
            printf("claim_amount: "); scanf("%lf", &amount);
            submit_claim(a, b, c, amount, tx_id);
            break;
        case 7:
            printf("claim_tx_id: "); scanf("%63s", a);
            printf("decided_by: "); scanf("%63s", b);
            make_tx_id(tx_id, sizeof(tx_id), "decision");
            record_claim_decision(a, b, 1, tx_id);
            break;
        case 8:
            printf("claim_tx_id: "); scanf("%63s", a);
            printf("decided_by: "); scanf("%63s", b);
            make_tx_id(tx_id, sizeof(tx_id), "decision");
            record_claim_decision(a, b, 0, tx_id);
            break;
        case 9:
            printf("utxo_output_id: "); scanf("%63s", a);
            printf("approved_amount: "); scanf("%lf", &amount);
            printf("provider_address: "); scanf("%63s", b);
            record_claim_settlement(a, amount, b, tx_id);
            break;
        case 0:
            printf("sender: "); scanf("%63s", a);
            printf("receiver: "); scanf("%63s", b);
            printf("nonce: "); scanf("%ld", &nonce);
            printf("amount: "); scanf("%lf", &amount);
            record_token_transfer(a, b, nonce, amount, tx_id);
            break;
        default:
            printf("Unknown type.\n");
    }
    /* suppress unused variable warning */
    (void)c; (void)d;
}

static void cmd_mempool_view(int argc, char **argv)
{
    (void)argc; (void)argv;
    view_mempool();
}

static void cmd_mine_solo(int argc, char **argv)
{
    if (argc < 2) { printf("usage: mine_solo <miner_id>\n"); return; }
    printf("Mining block (difficulty %d)...\n", chain_state.difficulty);
    int r = mine_block_solo(argv[1]);
    if (r == 0) printf("Block mined successfully. Chain height: %d\n", blockchain.height);
    else printf("Mining failed (code %d).\n", r);
}

static void cmd_mine_pool(int argc, char **argv)
{
    (void)argc; (void)argv;
    /* collect miners interactively */
    printf("Pool mining: enter miners (blank miner_id to finish).\n");

    PoolMiner miners[32];
    int count = 0;

    while (count < 32) {
        char miner_id[64];
        long hashes;
        printf("  miner_id (or blank to finish): ");
        if (fgets(miner_id, sizeof(miner_id), stdin) == NULL) break;
        miner_id[strcspn(miner_id, "\n")] = '\0';
        if (miner_id[0] == '\0') break;

        printf("  hashes_attempted: ");
        if (scanf("%ld", &hashes) != 1) break;
        getchar(); /* consume newline */

        strncpy(miners[count].miner_id, miner_id, sizeof(miners[count].miner_id) - 1);
        miners[count].miner_id[sizeof(miners[count].miner_id) - 1] = '\0';
        miners[count].hashes_attempted = hashes;
        count++;
    }

    if (count == 0) { printf("No miners provided.\n"); return; }

    printf("Mining block with %d miners (difficulty %d)...\n", count, chain_state.difficulty);
    int r = mine_block_pool(miners, count);
    if (r == 0) printf("Block mined. Chain height: %d\n", blockchain.height);
    else printf("Pool mining failed (code %d).\n", r);
}

static void cmd_blockchain_view(int argc, char **argv)
{
    (void)argc; (void)argv;
    view_blockchain();
}

static void cmd_blockchain_verify(int argc, char **argv)
{
    (void)argc; (void)argv;
    int r = validate_chain();
    switch (r) {
        case 0: printf("Chain valid.\n"); break;
        case 1: printf("Chain is empty.\n"); break;
        case 2: printf("INVALID: broken hash link detected.\n"); break;
        case 3: printf("INVALID: merkle root mismatch detected.\n"); break;
        default: printf("INVALID: unknown error (%d).\n", r); break;
    }
}

static void cmd_chain_save(int argc, char **argv)
{
    if (argc < 2) { printf("usage: chain_save <filepath>\n"); return; }
    chain_save(argv[1]);
}

static void cmd_chain_load(int argc, char **argv)
{
    if (argc < 2) { printf("usage: chain_load <filepath>\n"); return; }
    chain_load(argv[1]);
}

static void cmd_difficulty_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("current difficulty:      %d\n", chain_state.difficulty);
    printf("last retarget block:     %d\n", chain_state.last_retarget_block);
    printf("blocks since retarget:   %d\n", blockchain.height - chain_state.last_retarget_block);
    printf("next retarget at block:  %d\n", chain_state.last_retarget_block + RETARGET_INTERVAL);
}

static void cmd_utxo_view(int argc, char **argv)
{
    (void)argc; (void)argv;
    view_utxo_set();
}

static void cmd_utxo_validate(int argc, char **argv)
{
    if (argc < 2) { printf("usage: utxo_validate <output_id>\n"); return; }
    int r = utxo_validate(argv[1]);
    printf("%s: %s\n", argv[1], r ? "VALID (unspent)" : "INVALID (spent or not found)");
}

/* account_balance/account_nonce/account_transfer alias the wallet/token commands */
static void cmd_account_balance(int argc, char **argv) { cmd_wallet_balance(argc, argv); }
static void cmd_submit_pending(int argc, char **argv)
{
    if (argc < 4) { printf("usage: submit_pending <sender> <receiver> <amount>\n"); return; }
    MempoolEntry e;
    make_tx_id(e.transaction_id, sizeof(e.transaction_id), "pending");
    strncpy(e.sender_address, argv[1], sizeof(e.sender_address) - 1); e.sender_address[sizeof(e.sender_address)-1] = '\0';
    strncpy(e.receiver_address, argv[2], sizeof(e.receiver_address) - 1); e.receiver_address[sizeof(e.receiver_address)-1] = '\0';
    e.amount = atof(argv[3]);
    strcpy(e.transaction_type, "TOKEN_TRANSFER");
    e.fee = e.amount * 0.01;
    e.status = TX_PENDING;
    e.flag_reason = 0;
    int r = mempool_add(e);
    if (r == 0) printf("Pending transaction %s added to mempool (fee: %.4f).\n", e.transaction_id, e.fee);
}

static void cmd_fund_account(int argc, char **argv)
{
    if (argc < 3) { printf("usage: fund_account <address> <amount>\n"); return; }
    Account *a = account_find(argv[1]);
    if (!a) { printf("No account: %s.\n", argv[1]); return; }
    double amount = atof(argv[2]);
    a->balance += amount;
    token_mint(amount);
    printf("Funded %s with %.2f AHT. New balance: %.2f AHT\n", argv[1], amount, a->balance);
}

static void cmd_account_nonce(int argc, char **argv)
{
    if (argc < 2) { printf("usage: account_nonce <address>\n"); return; }
    Account *a = account_find(argv[1]);
    if (!a) { printf("No account: %s.\n", argv[1]); return; }
    printf("%s nonce: %ld\n", argv[1], a->nonce);
}
static void cmd_account_transfer(int argc, char **argv) { cmd_token_transfer(argc, argv); }

static void cmd_fraud_review(int argc, char **argv)
{
    (void)argc; (void)argv;
    fraud_review();
}

static void cmd_approve_suspicious(int argc, char **argv)
{
    if (argc < 2) { printf("usage: approve_suspicious <tx_id>\n"); return; }
    approve_suspicious(argv[1]);
}

static void cmd_reject_suspicious(int argc, char **argv)
{
    if (argc < 2) { printf("usage: reject_suspicious <tx_id>\n"); return; }
    reject_suspicious(argv[1]);
}

/* -----------------------------------------------------------------------
 * History queries - scan blockchain.blocks for matching transactions
 * --------------------------------------------------------------------- */

static void print_tx(int block_id, const Transaction *tx)
{
    printf("  [block %d] id=%-30s type=%-28s sender=%-20s receiver=%-20s amount=%.2f\n",
           block_id, tx->transaction_id, tx->transaction_type,
           tx->sender_address, tx->receiver_address, tx->amount);
}

static void cmd_transaction_history(int argc, char **argv)
{
    if (argc < 2) { printf("usage: transaction_history <address>\n"); return; }
    const char *addr = argv[1];
    int found = 0;
    printf("Transaction history for %s:\n", addr);
    for (int b = 0; b < blockchain.height; b++) {
        Block *blk = &blockchain.blocks[b];
        for (int t = 0; t < blk->transaction_count; t++) {
            Transaction *tx = &blk->transactions[t];
            if (strcmp(tx->sender_address, addr) == 0 ||
                strcmp(tx->receiver_address, addr) == 0) {
                print_tx(blk->block_id, tx);
                found++;
            }
        }
    }
    if (!found) printf("  (none)\n");
}

static void cmd_provider_history(int argc, char **argv)
{
    if (argc < 2) { printf("usage: provider_history <provider_address>\n"); return; }
    const char *addr = argv[1];
    int found = 0;
    printf("Claim history for provider %s:\n", addr);
    for (int b = 0; b < blockchain.height; b++) {
        Block *blk = &blockchain.blocks[b];
        for (int t = 0; t < blk->transaction_count; t++) {
            Transaction *tx = &blk->transactions[t];
            if (strcmp(tx->receiver_address, addr) == 0 &&
                (strcmp(tx->transaction_type, "CLAIM_SETTLEMENT") == 0 ||
                 strcmp(tx->transaction_type, "CLAIM_SUBMISSION") == 0)) {
                print_tx(blk->block_id, tx);
                found++;
            }
        }
    }
    if (!found) printf("  (none)\n");
}

static void cmd_premium_history(int argc, char **argv)
{
    if (argc < 2) { printf("usage: premium_history <member_address>\n"); return; }
    const char *addr = argv[1];
    int found = 0;
    printf("Premium history for %s:\n", addr);
    for (int b = 0; b < blockchain.height; b++) {
        Block *blk = &blockchain.blocks[b];
        for (int t = 0; t < blk->transaction_count; t++) {
            Transaction *tx = &blk->transactions[t];
            if (strcmp(tx->sender_address, addr) == 0 &&
                strcmp(tx->transaction_type, "PREMIUM_PAYMENT") == 0) {
                print_tx(blk->block_id, tx);
                found++;
            }
        }
    }
    if (!found) printf("  (none)\n");
}

static void cmd_claim_history(int argc, char **argv)
{
    if (argc < 2) { printf("usage: claim_history <member_address>\n"); return; }
    const char *addr = argv[1];
    int found = 0;
    printf("Claim history for %s:\n", addr);
    for (int b = 0; b < blockchain.height; b++) {
        Block *blk = &blockchain.blocks[b];
        for (int t = 0; t < blk->transaction_count; t++) {
            Transaction *tx = &blk->transactions[t];
            if (strcmp(tx->sender_address, addr) == 0 &&
                strcmp(tx->transaction_type, "CLAIM_SUBMISSION") == 0) {
                print_tx(blk->block_id, tx);
                found++;
            }
        }
    }
    if (!found) printf("  (none)\n");
}

/* -----------------------------------------------------------------------
 * Dispatch table
 * --------------------------------------------------------------------- */

typedef struct {
    const char *name;
    void (*fn)(int argc, char **argv);
    const char *help;
} Command;

static const Command commands[] = {
    /* Membership */
    { "register_member",     cmd_register_member,    "register_member <address>" },
    { "view_member",         cmd_view_member,         "view_member <address>" },
    { "wallet_balance",      cmd_wallet_balance,      "wallet_balance <address>" },
    /* Policy */

    { "enroll_policy",       cmd_enroll_policy,       "enroll_policy <member_id> <policy_id> <coverage_plan>" },
    { "view_policy",         cmd_view_policy,         "view_policy  (prompts for policy_id)" },
    { "renew_policy",        cmd_renew_policy,        "renew_policy  (prompts for policy_id)" },
    { "policy_status",       cmd_policy_status,       "policy_status  (prompts for policy_id)\n" },
    /* Token */

    { "token_transfer",      cmd_token_transfer,      "token_transfer <sender> <receiver> <nonce> <amount>" },
    { "token_balance",       cmd_token_balance,       "token_balance <address>\n" },
    /* Insurance */

    { "pay_premium",         cmd_pay_premium,         "pay_premium <member_address> <nonce> <amount>" },
    { "service_request",     cmd_service_request,     "service_request <member_id> <provider_id> <description>" },
    { "preauth_request",     cmd_preauth_request,     "preauth_request <member_id> <provider_id> <estimated_amount>" },
    { "preauth_approve",     cmd_preauth_approve,     "preauth_approve <member_id> <provider_id> <estimated_amount>" },
    { "submit_claim",        cmd_submit_claim,        "submit_claim <member_id> <policy_id> <provider_address> <amount>" },
    { "approve_claim",       cmd_approve_claim,       "approve_claim <claim_tx_id> <decided_by>" },
    { "reject_claim",        cmd_reject_claim,        "reject_claim <claim_tx_id> <decided_by>" },
    { "settle_claim",        cmd_settle_claim,        "settle_claim <utxo_output_id> <approved_amount> <provider_address>" },
    { "fund_account",        cmd_fund_account,         "fund_account <address> <amount>  [DEBUG]" },
    { "reinsurance_balance", cmd_reinsurance_balance, "reinsurance_balance\n" },
    /* Blockchain */

    { "create_transaction",  cmd_create_transaction,  "create_transaction  (interactive menu)" },
    { "mempool_view",        cmd_mempool_view,         "mempool_view" },
    { "mine_solo",           cmd_mine_solo,            "mine_solo <miner_id>" },
    { "mine_pool",           cmd_mine_pool,            "mine_pool  (interactive, prompts for miners)" },
    { "blockchain_view",     cmd_blockchain_view,      "blockchain_view" },
    { "blockchain_verify",   cmd_blockchain_verify,    "blockchain_verify" },
    { "chain_save",          cmd_chain_save,           "chain_save <filepath>" },
    { "chain_load",          cmd_chain_load,           "chain_load <filepath>" },
    { "difficulty_status",   cmd_difficulty_status,    "difficulty_status\n" },
    /* UTXO */

    { "utxo_view",           cmd_utxo_view,            "utxo_view" },
    { "utxo_validate",       cmd_utxo_validate,        "utxo_validate <output_id>\n" },
    /* Account model (aliases) */

    { "submit_pending",      cmd_submit_pending,       "submit_pending <sender> <receiver> <amount>  (adds PENDING tx for mining)" },
    { "fund_account",        cmd_fund_account,         "fund_account <address> <amount>  (debug: direct credit)" },
    { "account_balance",     cmd_account_balance,      "account_balance <address>" },
    { "account_transfer",    cmd_account_transfer,     "account_transfer <sender> <receiver> <nonce> <amount>" },
    { "account_nonce",       cmd_account_nonce,        "account_nonce <address>\n" },
    /* Fraud */

    { "fraud_review",        cmd_fraud_review,         "fraud_review" },
    { "approve_suspicious",  cmd_approve_suspicious,   "approve_suspicious <tx_id>" },
    { "reject_suspicious",   cmd_reject_suspicious,    "reject_suspicious <tx_id>\n" },
    /* History */

    { "transaction_history", cmd_transaction_history,  "transaction_history <address>" },
    { "provider_history",    cmd_provider_history,     "provider_history <provider_address>" },
    { "premium_history",     cmd_premium_history,      "premium_history <member_address>" },
    { "claim_history",       cmd_claim_history,        "claim_history <member_address>\n" },
    { NULL, NULL, NULL }
};

static void print_help()
{
    printf("\nAvailable commands:\n");
    for (int i = 0; commands[i].name != NULL; i++) {
        printf("%d. %s\n", i + 1, commands[i].help);
    }
    printf("  help\n  exit\n\n");
}

/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* initialize all state */
    init_chain_state();
    init_blockchain();
    init_mempool();
    init_accounts();
    init_utxo_set();
    init_token();

    printf("AHT Blockchain v1.0  |  type 'help' for commands\n\n");

    char line[MAX_LINE];
    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* strip trailing newline */
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;

        char *args[MAX_ARGS];
        int nargs = tokenize(line, args, MAX_ARGS);
        if (nargs == 0) continue;

        if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
            printf("Goodbye.\n");
            break;
        }

        if (strcmp(args[0], "help") == 0) {
            print_help();
            continue;
        }

        int found = 0;
        for (int i = 0; commands[i].name != NULL; i++) {
            if (strcmp(args[0], commands[i].name) == 0) {
                commands[i].fn(nargs, args);
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Unknown command '%s'. Type 'help' for a list.\n", args[0]);
        }
    }

    return 0;
}