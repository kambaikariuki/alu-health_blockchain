#include "../../include/core/mempool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MEMPOOL_ADD_OK              0
#define MEMPOOL_ADD_OUT_OF_MEMORY   1
#define MEMPOOL_ADD_DUPLICATE_ID    2

Mempool mempool;

void init_mempool()
{
    mempool.entries = malloc(sizeof(MempoolEntry) * MEMPOOL_INITIAL_CAPACITY);
    if (mempool.entries == NULL) {
        fprintf(stderr, "FATAL: failed to allocate initial mempool\n");
        exit(EXIT_FAILURE);
    }
    mempool.count = 0;
    mempool.capacity = MEMPOOL_INITIAL_CAPACITY;
}

/* Returns a pointer to the entry with the given transaction_id, or NULL if
 * not found. Internal helper used by mempool_remove and mempool_mark_status
 * so both stay consistent about how lookups are done. */
static MempoolEntry* find_entry(const char *transaction_id)
{
    for (int i = 0; i < mempool.count; i++) {
        if (strcmp(mempool.entries[i].transaction_id, transaction_id) == 0) {
            return &mempool.entries[i];
        }
    }
    return NULL;
}

/* Adds entry to the mempool, growing the backing array if needed.
 * Returns 0 (MEMPOOL_ADD_OK) on success.
 * Rejects duplicate transaction_id (same transaction submitted twice). */
int mempool_add(MempoolEntry entry)
{
    if (find_entry(entry.transaction_id) != NULL) {
        fprintf(stderr, "mempool_add: rejected, duplicate transaction_id %s\n",
                entry.transaction_id);
        return MEMPOOL_ADD_DUPLICATE_ID;
    }

    if (mempool.count >= mempool.capacity) {
        int new_capacity = mempool.capacity * 2;
        MempoolEntry *resized = realloc(mempool.entries, sizeof(MempoolEntry) * new_capacity);
        if (resized == NULL) {
            fprintf(stderr, "mempool_add: out of memory growing mempool\n");
            return MEMPOOL_ADD_OUT_OF_MEMORY;
        }
        mempool.entries = resized;
        mempool.capacity = new_capacity;
    }

    mempool.entries[mempool.count] = entry;
    mempool.entries[mempool.count].submitted_at = time(NULL);
    if (entry.status != TX_SUSPICIOUS) {
        mempool.entries[mempool.count].flag_reason = 0;
    }
    mempool.count++;
    return MEMPOOL_ADD_OK;
}

/* Removes the entry with the given transaction_id, if present. Shifts all
 * later entries down by one to keep the array contiguous (no gaps).
 * No-op (not an error) if the id isn't found, since "remove something
 * that's already gone" is a reasonable no-op for a mempool. */
void mempool_remove(const char *transaction_id)
{
    int found_index = -1;
    for (int i = 0; i < mempool.count; i++) {
        if (strcmp(mempool.entries[i].transaction_id, transaction_id) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        return;
    }

    for (int i = found_index; i < mempool.count - 1; i++) {
        mempool.entries[i] = mempool.entries[i + 1];
    }
    mempool.count--;
}

/* Comparator for qsort: descending by fee (highest fee first), per the
 * spec's "higher fee means higher priority". */
static int compare_by_fee_desc(const void *a, const void *b)
{
    const MempoolEntry *ea = (const MempoolEntry *)a;
    const MempoolEntry *eb = (const MempoolEntry *)b;

    if (ea->fee > eb->fee) return -1;
    if (ea->fee < eb->fee) return 1;
    return 0;
}

void mempool_sort_by_fee()
{
    qsort(mempool.entries, mempool.count, sizeof(MempoolEntry), compare_by_fee_desc);
}

/* Returns a pointer to the highest-fee PENDING entry, skipping CONFIRMED
 * and SUSPICIOUS entries, or NULL if no PENDING entries exist.
 *
 * Calls mempool_sort_by_fee() first so the mempool stays sorted as a side
 * effect — meaning the returned pointer is always entries[0] after a
 * PENDING entry is found, but the scan still has to walk past any
 * higher-fee CONFIRMED/SUSPICIOUS entries that may be sorted ahead of it. */
MempoolEntry* mempool_get_next_for_mining()
{
    mempool_sort_by_fee();

    for (int i = 0; i < mempool.count; i++) {
        if (mempool.entries[i].status == TX_PENDING) {
            return &mempool.entries[i];
        }
    }
    return NULL;
}

/* Updates the status of the entry with the given transaction_id, if found.
 * Silently does nothing if the id isn't present — callers that need to
 * know whether the update happened should call find_entry-equivalent logic
 * themselves, or this function could be changed to return int if that
 * becomes necessary. */
void mempool_mark_status(const char *transaction_id, TxStatus status)
{
    MempoolEntry *entry = find_entry(transaction_id);
    if (entry != NULL) {
        entry->status = status;
    }
}

/* Frees the mempool's backing array. After calling this, the mempool must
 * not be used again unless init_mempool() is called fresh. */
void free_mempool()
{
    free(mempool.entries);
    mempool.entries = NULL;
    mempool.count = 0;
    mempool.capacity = 0;
}

static const char* status_to_string(TxStatus status)
{
    switch (status) {
        case TX_PENDING:    return "PENDING";
        case TX_CONFIRMED:  return "CONFIRMED";
        case TX_SUSPICIOUS: return "SUSPICIOUS";
        default:            return "UNKNOWN";
    }
}

void view_mempool()
{
    if (mempool.count <= 0) {
        printf("Mempool is empty.\n");
        return;
    }

    printf("===== Mempool (%d entr%s) =====\n\n",
           mempool.count, mempool.count == 1 ? "y" : "ies");

    for (int i = 0; i < mempool.count; i++) {
        MempoolEntry *e = &mempool.entries[i];
        printf("[%d] id:       %s\n", i, e->transaction_id);
        printf("    sender:   %s\n", e->sender_address);
        printf("    receiver: %s\n", e->receiver_address);
        printf("    amount:   %.8f\n", e->amount);
        printf("    type:     %s\n", e->transaction_type);
        printf("    fee:      %.8f\n", e->fee);
        printf("    submitted_at: %ld\n", e->submitted_at);
        printf("    status:   %s\n\n", status_to_string(e->status));
    }

    printf("===== End of mempool =====\n");
}