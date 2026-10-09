/* Scalar substitutes for validating the runner on a host. No EE instructions
 * execute here; only the PS2 ELF can validate the assembly implementation. */
#include <stdint.h>
#include <string.h>
static unsigned int mul(unsigned int a, unsigned int b)
{
    return (a * b + 127) / 255;
}
void ee_mmi_add_quads(uint8_t *d, const uint8_t *s, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n * 16; ++i)
    { unsigned int v = d[i] + s[i]; d[i] = v > 255 ? 255 : v; }
}
void ee_mmi_copy_quads(void *d, const void *s, unsigned int n)
{ memcpy(d, s, n * 16); }
void ee_mmi_fill_quads(uint8_t *d, const void *s, unsigned int n)
{ while (n--) { memcpy(d, s, 16); d += 16; } }
void ee_mmi_in_a8_quads(uint8_t *d, const uint8_t *s, unsigned int n, const void *c)
{
    unsigned int i;
    (void)c;
    for (i = 0; i < n * 16; ++i) d[i] = mul(d[i], s[i]);
}
void ee_mmi_over_quads(uint8_t *d, const uint8_t *s, unsigned int n, const void *c)
{
    unsigned int i;
    (void)c;
    for (i = 0; i < n * 16; i += 4)
    {
        unsigned int a = 255 - s[i + 3], j;
        for (j = 0; j < 4; ++j)
        { unsigned int v = s[i+j] + mul(d[i+j], a); d[i+j] = v > 255 ? 255 : v; }
    }
}
void ee_mmi_8888_to_0565_quads(uint16_t *d, const uint32_t *s, unsigned int n, const void *c)
{
    unsigned int i;
    (void)c;
    for (i = 0; i < n * 8; ++i)
        d[i] = ((s[i] >> 8) & 0xf800) | ((s[i] >> 5) & 0x7e0) | ((s[i] >> 3) & 0x1f);
}
void ee_mmi_ca_over_quads(uint8_t *d, const uint8_t *s, unsigned int n, const void *c)
{
    unsigned int i;
    (void)c;
    while (n--)
    {
        for (i = 0; i < 16; ++i)
        { unsigned int v = s[i] + mul(d[i], s[i+16]); d[i] = v > 255 ? 255 : v; }
        d += 16; s += 32;
    }
}
