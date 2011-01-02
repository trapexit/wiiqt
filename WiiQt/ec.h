#ifndef EC_H
#define EC_H
#include "includes.h"

int check_ec( quint8 *ng, quint8 *ap, quint8 *sig, quint8 *sig_hash );
void make_ec_cert( quint8 *cert, quint8 *sig, char *signer, char *name, quint8 *priv, quint32 key_id );
void generate_ecdsa( quint8 *R, quint8 *S, quint8 *k, quint8 *hash );

#endif // EC_H
