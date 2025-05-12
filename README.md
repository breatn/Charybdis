# 🔱 Charybdis Hashing Algorithm

> ⚠️ **DISCLAIMER**  
> This hashing algorithm is **experimental** and **has not been peer-reviewed**.  
> It is intended strictly for **research, educational, and cryptographic exploration purposes**.  
> **Do not** use this algorithm in production systems, cryptographic protocols, or security-critical infrastructure without formal security analysis and third-party cryptanalysis.

---

## Introduction

**Charybdis** is a 512-bit cryptographic hashing algorithm built around chaotic, nonlinear mixing. Its design emphasizes **avalanche diffusion**, **block permutation**, and **chaotic round functions**, drawing from cryptographic sponge construction principles and chaos theory.

Charybdis is simple to implement, tunable in security and speed via `ROUNDS`, and written in portable C99. It aims to push beyond traditional designs by layering unpredictable bit interactions and full-state transformations.

---

## Key Features

- **Full 512-bit output**
-  **Chaotic mixing** of input and state
-  **Tunable number of rounds**
-  **State permutations** and nonlinear feedback
-  Achieves **ideal avalanche behavior** in statistical testing

---

## Design Philosophy

Charybdis is inspired by:

- **Sponge-based hash functions** (like SHA-3, BLAKE3)
- **Chaotic iteration systems** for mixing
- **High-entropy bit transformations** via algebraic randomness

Its core design involves:

- Absorbing input blocks into an internal 8×64-bit state
- Applying a non-linear `chaos_mix` function
- Interleaved state shuffling and sponge-style final rounds

---

## Algorithm Overview

Let:

- `M` be the input message
- `Bᵢ` be the `i`th block of the message
- `S = [s₀, s₁, ..., s₇]` be the 512-bit internal state
- `R` be the number of sponge rounds
- `f(x, k, r)` be the `chaos_mix` function

### Chaotic Mixing Function

```c
uint64_t chaos_mix(uint64_t x, uint64_t key, int rnd) {
    const uint64_t ADD = 0x9E3779B97F4A7C15ULL;
    const uint64_t MUL = 0x94D049BB133111EBULL;
    for (int i = 0; i < (rnd % 16) + 1; i++) {
        x ^= key + ADD;
        x = rotl64(x, (i * 13 + rnd) & 63);
        x *= (MUL ^ (key >> (i & 31)));
        x ^= x >> 27;
    }
    return x;
}
```

The function mixes bits using modular arithmetic, XOR, multiplication, and rotation, all modulated by a round-dependent feedback loop.

 ## State Absorption
Each 64-bit block Bᵢ is absorbed into state S using: sⱼ ← f(sⱼ ⊕ (Bᵢ ⊕ s_{(j+5) mod 8}), s_{(j+3) mod 8}, i + j + 1)

 ## Permutation Step
After every block:

Swap(s₀, s₄); Swap(s₂, s₆);
Swap(s₁, s₅); Swap(s₃, s₇);

 ## Final Sponge Phase
After message absorption, apply R rounds of mixing:

for r in 0..R:
    for j in 0..7:
        sⱼ ← f(sⱼ ⊕ s_{(j+2)%8}, s_{(j+1)%8} ⊕ s_{(j+3)%8}, r + j)
    swap(s₀, s₇)

## Avalanche Results

Using statistical testing on 2048-bit inputs:

| **Metric**              | **Value**       |
|-------------------------|-----------------|
| Avalanche Coefficient   | 0.5006          |
| Avalanche Weight        | 256.33 bits     |
| Avalanche Entropy       | 511.65 bits     |
| Avalanche Dependence    | 512 bits        |

This indicates near-perfect bit diffusion: flipping a single input bit flips approximately 50% of the output bits, uniformly distributed across the 512-bit hash.
