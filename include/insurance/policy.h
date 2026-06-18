#ifndef POLICY_H
#define POLICY_H

typedef struct {
    char member_id[64];
    char policy_id[64];
    char coverage_plan[64];
    long enrollment_date;
    long expiry_date;
    char status[16];
} Policy;

void enroll_policy();
void renew_policy();

#endif