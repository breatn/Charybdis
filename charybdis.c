/* Charybdis Lattice-Based Hashing Algorithm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/sha.h>

#define INITIAL_CAPACITY 1024
#define BLOCK_SIZE 8  // bytes
#define CHARYBDIS_ROUNDS 250
#define LATTICE_DIM 64 

// Utility: 64-bit rotate-left
static inline uint64_t rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

static inline uint32_t rotl32(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

#define QR(a,b,c,d) do {         \
    a += b; d ^= a; d = rotl32(d,16);  \
    c += d; b ^= c; b = rotl32(b,12);    \
    a += b; d ^= a; d = rotl32(d,8);     \
    c += d; b ^= c; b = rotl32(b,7);      \
} while(0)

// ChaCha20 block function.
// key_seed is used to derive a 256-bit key and counter is the block counter.
static void chacha20_block(uint64_t key_seed, uint32_t counter, uint8_t output[64]) {
    uint32_t state[16], working[16];

    //constants
    state[0]  = 0x61707865;
    state[1]  = 0x3320646e;
    state[2]  = 0x79622d32;
    state[3]  = 0x6b206574;

    // Derive 256-bit key from key_seed.
    uint32_t k0 = (uint32_t)(key_seed & 0xffffffff);
    uint32_t k1 = (uint32_t)((key_seed >> 32) & 0xffffffff);
    uint32_t k2 = k0 ^ 0xdeadbeef;
    uint32_t k3 = k1 ^ 0xcafebabe;
    uint32_t k4 = ~k0;
    uint32_t k5 = ~k1;
    uint32_t k6 = k0 + k1;
    uint32_t k7 = k0 ^ k1;
    state[4]  = k0; state[5]  = k1; state[6]  = k2; state[7]  = k3;
    state[8]  = k4; state[9]  = k5; state[10] = k6; state[11] = k7;

    state[12] = counter;
    state[13] = 0;
    state[14] = 0;
    state[15] = 0;

    memcpy(working, state, sizeof(state));

    // 20 rounds 
    for (int i = 0; i < 10; i++) {
        // Column rounds
        QR(working[0], working[4],  working[8],  working[12]);
        QR(working[1], working[5],  working[9],  working[13]);
        QR(working[2], working[6],  working[10], working[14]);
        QR(working[3], working[7],  working[11], working[15]);
        // Diagonal rounds
        QR(working[0], working[5],  working[10], working[15]);
        QR(working[1], working[6],  working[11], working[12]);
        QR(working[2], working[7],  working[8],  working[13]);
        QR(working[3], working[4],  working[9],  working[14]);
    }
    for (int i = 0; i < 16; i++) {
        working[i] += state[i];
    }
    // Serialize state to output
    for (int i = 0; i < 16; i++) {
        output[i * 4 + 0] = working[i] & 0xff;
        output[i * 4 + 1] = (working[i] >> 8) & 0xff;
        output[i * 4 + 2] = (working[i] >> 16) & 0xff;
        output[i * 4 + 3] = (working[i] >> 24) & 0xff;
    }
}

// ChaCha20 PRNG
uint64_t prng64(uint64_t *state) {
    // Use the current state as the key seed and block counter
    uint64_t key_seed = *state;
    uint32_t counter = (uint32_t)(*state & 0xffffffff);
    *state = *state + 1;

    uint8_t block[64];
    chacha20_block(key_seed, counter, block);

    // Return the first 8 bytes of the block as a 64-bit number
    uint64_t result = 0;
    for (int i = 7; i >= 0; i--) {
        result = (result << 8) | block[i];
    }
    return result;
}

// LWE/NTRU lattice mixing function
uint64_t lattice_mix(uint64_t x, uint64_t vec[LATTICE_DIM], int rnd) {
    uint64_t noise[LATTICE_DIM];
    uint64_t conv[LATTICE_DIM];
    // Duplicate noise into an extended array to avoid modulo calculations
    uint64_t noise_ext[2 * LATTICE_DIM];

    // Generate a noise polynomial with 8-bit coefficients
    for (int i = 0; i < LATTICE_DIM; i++) {
        noise[i] = (((x >> (i % 32)) & 0xFF) + rnd + i) & 0xFF;
        noise_ext[i] = noise[i];
        noise_ext[i + LATTICE_DIM] = noise[i];
    }

    // Compute circular convolution using extended noise array
    for (int k = 0; k < LATTICE_DIM; k++) {
        conv[k] = 0;
        int offset = LATTICE_DIM - k;
        for (int i = 0; i < LATTICE_DIM; i++) {
            conv[k] += vec[i] * noise_ext[offset + i];
        }
    }

    // Combine the convolution coefficients into a 64-bit value using rotate-mix
    uint64_t result = x;
    for (int i = 0; i < LATTICE_DIM; i++) {
        result ^= rotl64(conv[i], (i * 7 + rnd) & 63);
    }
    return result ^ (result >> 29);
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
