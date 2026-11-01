#include "common.h"

const Color PALETTE[C_NCOLORS] = {
    {0x04, 0x08, 0x05}, /* black (very slightly green) */
    {0x0c, 0x3a, 0x14}, /* dark green */
    {0x22, 0xb8, 0x3c}, /* phosphor green */
    {0x6a, 0xff, 0x7a}, /* bright green */
    {0xe8, 0xa8, 0x20}, /* amber */
    {0x7a, 0x50, 0x10}, /* dark amber */
    {0x50, 0x58, 0x52}, /* muted grey */
    {0xa8, 0xb4, 0xaa}, /* light grey */
    {0xe0, 0x30, 0x28}, /* warning red */
    {0x70, 0x14, 0x14}, /* dark red */
    {0xe8, 0xf4, 0xe8}, /* white */
    {0x40, 0xd0, 0xe0}, /* cyan */
};

uint32_t rng_u32(uint32_t *s)
{
    uint32_t x = *s;
    if (!x) x = 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

int rng_range(uint32_t *s, int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + (int)(rng_u32(s) % (uint32_t)(hi - lo));
}

float rng_float(uint32_t *s)
{
    return (float)(rng_u32(s) >> 8) * (1.0f / 16777216.0f);
}
