# Blockchain-Based Health Insurance Management System

## Overview

The Blockchain-Based Health Insurance Management System is a C-based blockchain application developed for the African Leadership University (ALU) Software Engineering – Low-Level Specialization (Introduction to Blockchain Development).

The system simulates a decentralized health insurance platform where policy enrollment, premium payments, healthcare claims, reimbursements, and auditing activities are recorded on an immutable blockchain ledger.

Unlike traditional centralized insurance systems, this platform uses blockchain technology to ensure:

* Transparency
* Tamper resistance
* Accountability
* Auditability
* Fraud detection
* Secure transaction verification

The platform introduces a native cryptocurrency called **ALU Health Token (AHT)** used for premium payments, claim settlements, reinsurance contributions, and mining rewards.

---

# Features

## Membership Management

* Register members
* View member details
* Wallet management
* Account lookup

## Policy Management

* Policy enrollment
* Policy renewal
* Policy status tracking
* Automatic expiry validation

## Insurance Operations

* Premium payments
* Healthcare service requests
* Pre-authorization requests
* Claim submission
* Claim approval/rejection
* Claim settlement

## Blockchain Features

* Block creation
* Proof-of-Work mining
* Merkle tree verification
* SHA-256 hashing
* ECDSA digital signatures
* Chain validation
* Difficulty retargeting

## Security Features

* Transaction signing
* Signature verification
* Replay protection
* Fraud detection
* Double-spending prevention
* Tamper detection

## Token System

* ALU Health Token (AHT)
* Account balances
* Wallet transfers
* Mining rewards

## Reinsurance Pool

* Automatic 5% premium contribution
* Secondary claim coverage
* Large claim settlement support

## Persistence

* Save blockchain state to disk
* Restore blockchain state on startup
* Blockchain verification after loading


# Blockchain Design

## Block Structure

Each block contains:

* Block ID
* Timestamp
* Transaction Count
* Previous Hash
* Merkle Root
* Nonce
* Miner ID
* Difficulty

## Transaction Structure

Each transaction contains:

* Transaction ID
* Sender Address
* Receiver Address
* Amount
* Transaction Type
* Timestamp
* Sender Nonce
* Digital Signature

## Mempool

Transactions enter a mempool before being mined.

Statuses:

* PENDING
* CONFIRMED
* SUSPICIOUS

Priority Rules:

1. Fee Descending
2. Timestamp Ascending

---

# Merkle Tree

A Merkle Tree is constructed from transaction hashes.

```text
TX1 TX2 TX3 TX4
 |   |   |   |
 H1  H2  H3  H4
  \ /     \ /
  H12     H34
      \ /
    ROOT
```

The resulting Merkle Root is stored in the block header.

---

# Transaction Models

## UTXO Model

Used for:

* Output creation
* Output spending
* Double-spending prevention

Features:

* UTXO validation
* UTXO consumption
* Change outputs

## Account-Based Model

Each account stores:

* Wallet Address
* Balance
* Nonce

Replay protection is implemented using account nonces.

---

# Mining

## Proof-of-Work

Mining steps:

1. Select transactions from mempool
2. Compute Merkle Root
3. Build block header
4. Hash block
5. Increment nonce
6. Repeat until difficulty target is met

## Difficulty Retargeting

Every 10 blocks:

```text
Average Time < 30 sec
        ↓
Difficulty +1

Average Time > 90 sec
        ↓
Difficulty -1
```

Difficulty never drops below 1.

---

# Fraud Detection

Transactions are flagged as SUSPICIOUS if:

### High Frequency Claims

More than 3 claims within 24 hours.

### Abnormal Claim Amount

Claim exceeds 2× provider historical average.

### Duplicate Transaction

Transaction ID already exists.

Suspicious transactions require manual review.

---

# Reinsurance Pool

Every premium payment automatically creates:

```text
Premium Payment
       |
       +----> Insurance Pool (95%)
       |
       +----> Reinsurance Pool (5%)
```

For claims larger than 1000 AHT:

```text
Insurance Pool → First 1000 AHT

Reinsurance Pool → Remaining Amount
```

---

# Persistence

The system stores:

* Blockchain
* Transactions
* Accounts
* Nonces
* UTXOs
* Mempool
* ChainState

Data is saved in:

```text
blockchain.bin
```

On startup:

1. Load blockchain
2. Verify chain
3. Restore state
4. Resume operations

---

# Available Commands

## Membership

```text
register_member
view_member
wallet_balance
```

## Policies

```text
enroll_policy
view_policy
renew_policy
policy_status
```

## Insurance

```text
pay_premium
service_request
preauth_request
preauth_approve
submit_claim
approve_claim
reject_claim
settle_claim
reinsurance_balance
```

## Blockchain

```text
create_transaction
mempool_view
mine_solo
mine_pool
blockchain_view
blockchain_verify
chain_save
chain_load
difficulty_status
```

## UTXO

```text
utxo_view
utxo_validate
```

## Accounts

```text
account_balance
account_transfer
account_nonce
```

## Fraud and Auditing

```text
fraud_review
approve_suspicious <tx_id>
reject_suspicious <tx_id>

transaction_history
provider_history
premium_history
claim_history
```

---

# Build Instructions

Compile with make:

```bash
make
```

---

# Dependencies

* GCC
* OpenSSL libcrypto
* Linux environment (recommended)

Install OpenSSL development package:

Arch Linux:

```bash
sudo pacman -S openssl
```

Ubuntu/Debian:

```bash
sudo apt install libssl-dev
```

---

# License

Educational project developed for academic purposes.
