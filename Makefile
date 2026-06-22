CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -I./include 

SRC = src/core/blockchain.c \
      src/core/member.c \
      src/core/pow.c \
      src/core/mempool.c \
      src/models/account.c \
      src/models/utxo.c \
      src/core/token.c \
      src/core/mining.c \
      src/insurance/policy.c \
      src/core/persistence.c \
      src/insurance/insurance.c \
      src/insurance/insurance_tx.c \
      src/insurance/fraud.c \
	src/core/chainstate.c \
      src/core/merkle.c \
      src/security/sha256.c \
	src/security/ecdsa.c \
      main.c

TARGET = aht_chain

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) -lssl -lcrypto

clean:
	rm -f $(TARGET)