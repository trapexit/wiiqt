#ifndef __AES_H_
#define __AES_H_
#include "includes.h"

#ifdef __cplusplus
extern "C" {
#endif

void aes_encrypt( quint8 *iv, const quint8 *inbuf, quint8 *outbuf, unsigned long long len );
void aes_decrypt( quint8 *iv, const quint8 *inbuf, quint8 *outbuf, unsigned long long len );
void aes_set_key( const quint8 *key );

#ifdef __cplusplus
}
#endif

#endif //__AES_H_

