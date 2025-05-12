## Charybdis hashing algorithm
This program has not been peer reviewed yet so is unsafe for cryptographic use

## The fundamentals
Charybdis is a 256 bit hashing algorithm which uses lattice based cryptography to convert plaintext into a hash. To achieve the avalanch effect, it uses Xorshift64* PRNG (which takes in the input as a seed and appends the result at the front of the string to hash), Fisher–Yates shuffle algorithm for shuffling blocks (8 bytes each) and bitwise rotation (per block). Thanks to these techniques it has achieved near perfection in these three avalanche effect and randomness testing equations:
Coefficient= 1/N⋅m i=1 ∑ N j=1∑m Di,j

​	
 
These are the results of these metrics on a 4096 bit input and 10000 rounds 

| **Metric**              | **Value**       |
|-------------------------|-----------------|
| Avalanche Coefficient   | 0.4996          |
| Avalanche Weight        | 255.78 bits     |
| Avalanche Entropy       | 511.6262 bits   |
| Avalanche Dependence    | 512 bits        |
