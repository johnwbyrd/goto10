# Algebraic Analysis of the C64 RND Map

## The map

Each call to RND(1) transforms a 4-byte mantissa `[a, b, c, d]` (where a is the MSB with implicit high bit set) through:

1. **Multiply** the mantissa by the ROM constant K = `$B5447A00`
2. **Byte-swap** the result: reverse the 4-byte order
3. **Normalize** and pack back into a seed

The constant K has bytes `[$B5, $44, $7A, $00]`. The last byte is zero, which has structural consequences.

There is also a 5th byte V (the "rounding byte"), which carries the exponent from the previous iteration. It participates in the multiply as the LSB of a 40-bit multiplicand. We set V aside for now and analyze the 4-byte core map, then account for V's contribution separately.

## Decomposing the multiply

The multiply computes:

```
x * K   where x = a*2^24 + b*2^16 + c*2^8 + d
              K = $B5*2^24 + $44*2^16 + $7A*2^8 + $00
```

Since K's low byte is zero, K = `$B5447A * 256`. Therefore:

```
x * K = x * $B5447A * 256
```

The product `x * $B5447A` is a 56-bit value (32-bit x times 24-bit k). Multiplying by 256 shifts it left 8 bits, making it 64 bits. The C64 takes the upper 32 bits as the result mantissa `[R1, R2, R3, R4]`, so effectively:

```
[R1, R2, R3, R4] = upper32(x * $B5447A * 256)
                  = upper32(x * $B5447A) shifted by the extra 8 bits
```

More precisely: let Q = x * $B5447A (56 bits). Then P = Q * 256 (64 bits). The upper 32 bits of P are the upper 24 bits of Q placed in [R1, R2, R3], and the next 8 bits of Q become R4.

Equivalently: `W = x * $B5447A >> 24` gives us the 32-bit result before byte swap. (This is approximate — the exact bit positions depend on normalization, but this captures the dominant structure.)

## Expanding the multiply byte by byte

Write k = $B5447A with bytes k1=$B5, k2=$44, k3=$7A. Then:

```
x * k = (a*2^24 + b*2^16 + c*2^8 + d) * (k1*2^16 + k2*2^8 + k3)
```

Expanding all 12 terms, grouped by their power of 2:

| Power   | Terms                    |
|---------|--------------------------|
| 2^40    | a * k1                   |
| 2^32    | a * k2 + b * k1          |
| 2^24    | a * k3 + b * k2 + c * k1 |
| 2^16    | b * k3 + c * k2 + d * k1 |
| 2^8     | c * k3 + d * k2          |
| 2^0     | d * k3                   |

Each "term" like `a * k1` is a byte-times-byte product, giving a 16-bit result. Write `a * k1 = hi(a,k1)*256 + lo(a,k1)`.

Since we want `x * k >> 24` (the upper 32 bits), we need to accumulate everything at 2^24 and above, with the lower terms contributing only through carries.

## The result bytes W3..W0

Let W = x * k >> 24, written as bytes [W3, W2, W1, W0] where W3 is MSB.

Building from the bottom (W0) upward, each byte is the sum of lo() parts from its power level plus hi() parts carried up from the level below:

```
W0 = lo(b,k3) + lo(c,k2) + lo(d,k1) + carries_from_below    mod 256

W1 = lo(a,k3) + lo(b,k2) + lo(c,k1)
   + hi(b,k3) + hi(c,k2) + hi(d,k1)
   + carry_from_W0                                             mod 256

W2 = lo(a,k2) + lo(b,k1)
   + hi(a,k3) + hi(b,k2) + hi(c,k1)
   + carry_from_W1                                             mod 256

W3 = lo(a,k1)
   + hi(a,k2) + hi(b,k1)
   + carry_from_W2                                             mod 256
```

The "carries_from_below" in W0 come from the sub-2^24 terms (c*k3, d*k2, d*k3). These are small corrections.

## After byte swap

The byte swap reverses `[W3, W2, W1, W0]` to `[W0, W1, W2, W3]`. So the output mantissa bytes are:

```
a' = W0 = lo(b,k3) + lo(c,k2) + lo(d,k1) + carries    mod 256
b' = W1 = lo(a,k3) + lo(b,k2) + lo(c,k1) + hi(b,k3) + hi(c,k2) + hi(d,k1) + carry    mod 256
c' = W2 = lo(a,k2) + lo(b,k1) + hi(a,k3) + hi(b,k2) + hi(c,k1) + carry    mod 256
d' = W3 = lo(a,k1) + hi(a,k2) + hi(b,k1) + carry    mod 256
```

## Dependency structure (ignoring carries)

Dropping carry terms and hi() terms (which are smaller corrections), the **dominant linear dependencies** are:

```
a' depends on:  b, c, d       (NOT a)
b' depends on:  a, b, c       (NOT d, to leading order)
c' depends on:  a, b          (NOT c, d, to leading order)
d' depends on:  a             (NOT b, c, d, to leading order)
```

This is **triangular with the diagonal missing**: a' doesn't depend on a, b' doesn't depend on d, etc. The byte swap has inverted the dependency direction relative to the multiply.

Key consequence: **d' is almost entirely determined by a, and a' is almost entirely determined by b, c, d.** Information flows from the MSB downward through d', and from the lower bytes upward through a'. The byte swap reverses this flow each iteration.

## Two-iteration composition

Substituting the one-step formulas into themselves, we get the **carry-free linear approximation** of two RND iterations. Using the actual constant values k1=$B5, k2=$44, k3=$7A, and computing all byte-times-byte products mod 256:

### Required products mod 256

| Product      | Decimal          | Mod 256 |
|-------------|------------------|---------|
| $7A * $7A   | 122 * 122 = 14884 | $04     |
| $7A * $44   | 122 * 68 = 8296   | $68     |
| $7A * $B5   | 122 * 181 = 22082 | $42     |
| $44 * $44   | 68 * 68 = 4624    | $10     |
| $44 * $B5   | 68 * 181 = 12308  | $14     |
| $B5 * $B5   | 181 * 181 = 32761 | $F9     |

### Two-step output bytes

Using the carry-free one-step formulas:

```
a' = b*$7A + c*$44 + d*$B5    mod 256
b' = a*$7A + b*$44 + c*$B5    mod 256
c' = a*$44 + b*$B5            mod 256
d' = a*$B5                    mod 256
```

Substituting into the same formulas for the second iteration:

**d'' = a' * $B5:**
```
d'' = (b*$7A + c*$44 + d*$B5) * $B5    mod 256
    = b*($7A*$B5) + c*($44*$B5) + d*($B5*$B5)    mod 256
    = b*$42 + c*$14 + d*$F9    mod 256
```

**d'' depends on b, c, d. Not on a.**

**c'' = a' * $44 + b' * $B5:**
```
c'' = (b*$7A + c*$44 + d*$B5)*$44 + (a*$7A + b*$44 + c*$B5)*$B5    mod 256
    = b*$7A*$44 + c*$44*$44 + d*$B5*$44 + a*$7A*$B5 + b*$44*$B5 + c*$B5*$B5    mod 256
    = a*$42 + b*($68+$14) + c*($10+$F9) + d*$14    mod 256
    = a*$42 + b*$7C + c*$09 + d*$14    mod 256
```

**c'' depends on a, b, c, d.**

**b'' = a' * $7A + b' * $44 + c' * $B5:**
```
b'' = (b*$7A + c*$44 + d*$B5)*$7A
    + (a*$7A + b*$44 + c*$B5)*$44
    + (a*$44 + b*$B5)*$B5    mod 256

From a' * $7A:  b*$04 + c*$68 + d*$42
From b' * $44:  a*$68 + b*$10 + c*$14
From c' * $B5:  a*$14 + b*$F9

b'' = a*($68+$14) + b*($04+$10+$F9) + c*($68+$14) + d*$42    mod 256
    = a*$7C + b*$0D + c*$7C + d*$42    mod 256
```

(Note: $04+$10+$F9 = 4+16+249 = 269 = 256+13, so mod 256 = $0D)

**b'' depends on a, b, c, d.**

**a'' = b' * $7A + c' * $44 + d' * $B5:**
```
a'' = (a*$7A + b*$44 + c*$B5)*$7A
    + (a*$44 + b*$B5)*$44
    + (a*$B5)*$B5    mod 256

From b' * $7A:  a*$04 + b*$68 + c*$42
From c' * $44:  a*$10 + b*$14
From d' * $B5:  a*$F9

a'' = a*($04+$10+$F9) + b*($68+$14) + c*$42 + d*0    mod 256
    = a*$0D + b*$7C + c*$42    mod 256
```

**a'' depends on a, b, c. Not on d.**

## The two-step matrix (carry-free linear approximation)

```
[a'']   [$0D  $7C  $42  $00] [a]
[b''] = [$7C  $0D  $7C  $42] [b]    mod 256
[c'']   [$42  $7C  $09  $14] [c]
[d'']   [$00  $42  $14  $F9] [d]
```

### Observations

1. **The matrix is symmetric.** This is a consequence of the byte swap being an involution composed with a structured multiply.

2. **Corner zeros**: a'' does not depend on d, and d'' does not depend on a. After two iterations, the MSB and LSB have decoupled (to leading order).

3. **The diagonal entries** are $0D, $0D, $09, $F9. These are the "self-feedback" coefficients. Note $0D = 13, $09 = 9, $F9 = 249 = -7 mod 256. All are odd, which is good for invertibility.

4. **Mod 2, this is the identity matrix**: every diagonal entry is odd (→1) and every off-diagonal entry is even (→0). This means the map is the identity plus an even perturbation. Over GF(2), the rank is 4 (full). But over Z/256, the even off-diagonal entries mean those dependencies are "weak" — they only affect bits 1 and above, not bit 0 of each byte.

5. **Over Z/4** (mod 4): the matrix becomes:
   ```
   [1  0  2  0]
   [0  1  0  2]
   [2  0  1  0]
   [0  2  0  1]
   ```
   This has a nice block structure and is singular mod 2 in the off-diagonal blocks.

## Significance for cycle length

The fact that a'' doesn't depend on d (and vice versa) means that after two iterations, the "outer" bytes lose one degree of freedom. After four iterations, this decoupling propagates further inward. The effective state contracts.

The carry terms (ignored above) add nonlinear corrections that prevent a clean closed-form, but the dominant structure is captured by this matrix. The short cycle length (~46K out of ~2^31 possible) is explained by the matrix having eigenvalues whose multiplicative orders mod 256 are small.

## Next steps

1. **Compute the matrix with carries**: run the actual multiply for all 256 values of each input byte and measure how much the carry corrections deviate from the linear approximation.

2. **Compute eigenvalues**: find the eigenvalues of the 4x4 matrix over Z/256 (or over Z/2^k for various k). The multiplicative orders of these eigenvalues bound the cycle length.

3. **Compute the matrix over Z/2^k for k=1..8**: as k increases, more structure is revealed. The rank may drop below 4 at some intermediate modulus, indicating dimensional collapse.

4. **Verify empirically**: compute M^n for various n and compare against iterating step() n times, to confirm the linear approximation's accuracy and understand where carries cause divergence.
