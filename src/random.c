


u64 rol64(u64 x, int k)
{
    return (x << k) | (x >> (64 - k));
}

struct xoshiro256ss_state {
    u64 s[4];
};

u64 xoshiro256ss(struct xoshiro256ss_state *state)
{
    u64 *s = state->s;
    u64 const result = rol64(s[1] * 5, 7) * 9;
    u64 const t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rol64(s[3], 45);

    return result;
}

