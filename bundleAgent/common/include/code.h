#ifndef INC_COMMON_CRYPTO_H
#define INC_COMMON_CRYPTO_H

int B64_encoded_l(int decoded_lenght);
int B64_decoded_l(char *encoded, int encoded_l);
int B64_encode(unsigned char *in, int inl, unsigned char *out);
int B64_decode(unsigned char *in, int inl, unsigned char *out);
int hex_code(unsigned char *in, int inl, char *out);
int hex_decode(unsigned char *in, int inl, char *out);
unsigned long Crc32_ComputeBuf( unsigned long inCrc32, const void *buf, size_t bufLen );
#endif