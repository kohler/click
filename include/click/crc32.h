/* -*- related-file-name: "../../lib/crc32.c" -*- */
#ifndef CLICK_CRC32_H
#define CLICK_CRC32_H
#ifdef __cplusplus
extern "C" {
#endif

unsigned long update_crc(unsigned long crc_accum,
                         char *data_blk_ptr,
                         int data_blk_size);

#ifdef __cplusplus
}
#endif
#endif
