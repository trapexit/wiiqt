#ifndef BN_H
#define BN_H
#include "includes.h"

int bn_compare(quint8 *a, quint8 *b, quint32 n);
void bn_sub_modulus(quint8 *a, quint8 *N, quint32 n);
void bn_add(quint8 *d, quint8 *a, quint8 *b, quint8 *N, quint32 n);
void bn_mul(quint8 *d, quint8 *a, quint8 *b, quint8 *N, quint32 n);
void bn_exp(quint8 *d, quint8 *a, quint8 *N, quint32 n, quint8 *e, quint32 en);
void bn_inv(quint8 *d, quint8 *a, quint8 *N, quint32 n);
#endif // BN_H
