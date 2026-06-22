#ifndef INSURANCE_TX_H
#define INSURANCE_TX_H

int record_policy_enrollment(const char *member_id, const char *policy_id,
                              const char *coverage_plan);
int record_premium_payment(const char *member_address, long member_nonce,
                            double amount, const char *transaction_id);
int record_service_request(const char *member_id, const char *provider_id,
                            const char *service_description, const char *transaction_id);
int record_preauthorization(const char *member_id, const char *provider_id,
                             double estimated_amount, int approved,
                             const char *transaction_id);
int submit_claim(const char *member_id, const char *policy_id, const char *provider_address,
                  double claim_amount, const char *transaction_id);
int record_claim_decision(const char *claim_transaction_id, const char *decided_by,
                           int approved, const char *decision_transaction_id);
int record_claim_settlement(const char *claim_output_id, double approved_amount,
                             const char *provider_address, const char *transaction_id);
int record_token_transfer(const char *sender_address, const char *receiver_address,
                           long sender_nonce, double amount, const char *transaction_id);

#endif