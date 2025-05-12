/* Charybdis Lattice-Based Hashing Algorithm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITIAL_CAPACITY 1024
#define BLOCK_SIZE 8  // bytes
#define CHARYBDIS_ROUNDS 10000
#define LATTICE_DIM 64 

// Utility: 64-bit rotate-left
static inline uint64_t rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

// PRNG: Xorshift64*
uint64_t prng64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

// Simulated lattice-based trapdoor mixing 
uint64_t lattice_mix(uint64_t x, uint64_t vec[LATTICE_DIM], int rnd) {
    uint64_t acc = x;
    for (int i = 0; i < LATTICE_DIM; i++) {
        acc ^= rotl64(vec[i] ^ (x + i * rnd), (i * 17 + rnd) & 63);
    }
    return acc ^ (acc >> 29);
}

// Read entire input from stdin into a buffer
char *read_message(size_t *out_len) {
    size_t capacity = INITIAL_CAPACITY, length = 0;
    char *buffer = malloc(capacity);
    if (!buffer) { perror("malloc"); exit(1); }

    int c;
    while ((c = fgetc(stdin)) != EOF) {
        buffer[length++] = (char)c;
        if (length == capacity) {
            capacity <<= 1;
            buffer = realloc(buffer, capacity);
            if (!buffer) { perror("realloc"); exit(1); }
        }
    }
    *out_len = length;
    return buffer;
}

// Fisher–Yates shuffle of block indices
void shuffle_indices(size_t *indices, size_t count, uint64_t seed) {
    for (size_t i = 0; i < count; i++) indices[i] = i;
    for (size_t i = count - 1; i > 0; i--) {
        uint64_t r = prng64(&seed);
        size_t j = r % (i + 1);
        size_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}

// Charybdis 256-bit hash
void secure_hash256(const char *msg, size_t len, uint64_t out[8]) {
    uint64_t state[8] = {
        0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
        0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
        0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
        0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
    };

    uint64_t lattice[LATTICE_DIM];
    for (int i = 0; i < LATTICE_DIM; i++) lattice[i] = (i * 0xABCDEF1234567890ULL) ^ (i * i + 1);

    size_t blocks = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t *order = malloc(blocks * sizeof(size_t));
    if (!order) { perror("malloc"); exit(1); }

    shuffle_indices(order, blocks, (uint64_t)len * 0xDEADBEEFCAFEBABEULL);

    // Process shuffled blocks
    for (size_t n = 0; n < blocks; n++) {
        size_t i = order[n];
        uint64_t block = 0;
        size_t remaining = len > i * BLOCK_SIZE ? len - i * BLOCK_SIZE : 0;
        size_t size = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        memcpy(&block, msg + i * BLOCK_SIZE, size);

        block ^= (uint64_t)len ^ ((uint64_t)i << 3);

        for (int j = 0; j < 8; j++) {
            uint64_t m = block ^ state[(j + 5) & 7];
            state[j] = lattice_mix(state[j] ^ m, lattice, (int)i + j + 1);
        }

        // state permutation
        uint64_t tmp = state[0];
        state[0] = state[4]; state[4] = state[2]; state[2] = state[6]; state[6] = tmp;
        tmp = state[1];
        state[1] = state[5]; state[5] = state[3]; state[3] = state[7]; state[7] = tmp;
    }

    free(order);

    // Final sponge-like lattice rounds
    for (int r = 0; r < CHARYBDIS_ROUNDS; r++) {
        for (int j = 0; j < 8; j++) {
            state[j] = lattice_mix(state[j] ^ state[(j + 2) & 7], lattice, r + j);
        }
        uint64_t tmp = state[r % 8];
        state[r % 8] = state[(r + 4) % 8];
        state[(r + 4) % 8] = tmp;
    }

    memcpy(out, state, 8 * sizeof(uint64_t));
}

int main(void) {
    size_t len;
    char *msg = read_message(&len);
    uint64_t hash[8];
    secure_hash256(msg, len, hash);

    for (int i = 0; i < 8; i++)
        printf("%016llx", (unsigned long long)hash[i]);
    printf("\n");

    free(msg);
    return 0;
}
