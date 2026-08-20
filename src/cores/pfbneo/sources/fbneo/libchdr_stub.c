#include <stdint.h>

typedef int chd_error;
#define CHDERR_UNSUPPORTED_FORMAT 3

#ifdef __cplusplus
extern "C" {
#endif

chd_error lzma_codec_init(void *codec, uint32_t hunkbytes)
{
    (void)codec;
    (void)hunkbytes;
    return CHDERR_UNSUPPORTED_FORMAT;
}

void lzma_codec_free(void *codec)
{
    (void)codec;
}

chd_error lzma_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
    (void)codec;
    (void)src;
    (void)complen;
    (void)dest;
    (void)destlen;
    return CHDERR_UNSUPPORTED_FORMAT;
}

chd_error zstd_codec_init(void *codec, uint32_t hunkbytes)
{
    (void)codec;
    (void)hunkbytes;
    return CHDERR_UNSUPPORTED_FORMAT;
}

void zstd_codec_free(void *codec)
{
    (void)codec;
}

chd_error zstd_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
    (void)codec;
    (void)src;
    (void)complen;
    (void)dest;
    (void)destlen;
    return CHDERR_UNSUPPORTED_FORMAT;
}

#ifdef __cplusplus
}
#endif
