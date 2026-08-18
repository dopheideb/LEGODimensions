#include <stdint.h>
#include <stdio.h>

// The code comes from https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
void encrypt (uint32_t v[2], const uint32_t k[4]) {
    uint32_t v0=v[0], v1=v[1], sum=0, i;           /* set up */
    uint32_t delta=0x9E3779B9;                     /* a key schedule constant */
    uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
    for (i=0; i<32; i++) {                         /* basic cycle start */
        sum += delta;
        v0 += ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        v1 += ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
    }                                              /* end cycle */
    v[0]=v0; v[1]=v1;
}

void decrypt (uint32_t v[2], const uint32_t k[4]) {
    uint32_t v0=v[0], v1=v[1], sum=0xC6EF3720, i;  /* set up; sum is (delta << 5) & 0xFFFFFFFF */
    uint32_t delta=0x9E3779B9;                     /* a key schedule constant */
    uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
    for (i=0; i<32; i++) {                         /* basic cycle start */
        v1 -= ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
        v0 -= ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        sum -= delta;
    }                                              /* end cycle */
    v[0]=v0; v[1]=v1;
}

int main()
{
    // TEA uses uint32_t arrays as inputs (both v and k), it does NOT 
    // use strings/binary data.
    // 
    // Wyldstyle
    // 
    // Binary form, on a little endian platform with 4 bytes wide ints:
    //     { 0x23, 0x82, 0xef, 0x33,   0x2f, 0x08, 0x56, 0x3a,   0x7c, 0x6c, 0xf0, 0x78,   0x10, 0x37, 0x6c, 0x24 }
    // 
    // Binary form, on a big endian platform with 4 bytes wide ints:
    //     { 0x33, 0xef, 0x82, 0x23,   0x3a, 0x56, 0x08, 0x2f,   0x78, 0xf0, 0x6c, 0x7c,   0x24, 0x6c, 0x37, 0x10 }
    uint32_t key[4] = { 0x33ef8223, 0x3a56082f, 0x78f06c7c, 0x246c3710 };
    uint32_t msg[2] = { 0x00000003, 0x00000003 };
    encrypt(msg, key);

    // Convert to hex.
    char buffer[256];
    for (unsigned n = 0; n < 8; ++n)
    {
        uint8_t nybble;
        char hexdigit;

        nybble = ((uint8_t *)msg)[n] >> 4;
        if (nybble < 10)
            hexdigit = '0' + (nybble - 0);
        else
            hexdigit = 'a' + (nybble - 10);
        buffer[2 * n + 0] = hexdigit;

        nybble = ((uint8_t *)msg)[n] & 0x0f;
        if (nybble < 10)
            hexdigit = '0' + (nybble - 0);
        else
            hexdigit = 'a' + (nybble - 10);
        buffer[2 * n + 1] = hexdigit;
    }
    buffer[16] = 0;
    printf("%s\n", buffer);
    printf("%s\n", "0139ed60e4be307c");
}
