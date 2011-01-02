// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

//#include <string.h>
//#include <stdio.h>

#include "bn.h"
/*
static void bn_print(char *name, quint8 *a, quint32 n)
{
    quint32 i;

	printf("%s = ", name);

	for (i = 0; i < n; i++)
		printf("%02x", a[i]);

	printf("\n");
}
*/
static void bn_zero(quint8 *d, quint32 n)
{
	memset(d, 0, n);
}

static void bn_copy(quint8 *d, quint8 *a, quint32 n)
{
	memcpy(d, a, n);
}

int bn_compare(quint8 *a, quint8 *b, quint32 n)
{
    quint32 i;

	for (i = 0; i < n; i++) {
		if (a[i] < b[i])
			return -1;
		if (a[i] > b[i])
			return 1;
	}

	return 0;
}

void bn_sub_modulus(quint8 *a, quint8 *N, quint32 n)
{
    quint32 i;
    quint32 dig;
    quint8 c;

	c = 0;
	for (i = n - 1; i < n; i--) {
		dig = N[i] + c;
		c = (a[i] < dig);
		a[i] -= dig;
	}
}

void bn_add(quint8 *d, quint8 *a, quint8 *b, quint8 *N, quint32 n)
{
    quint32 i;
    quint32 dig;
    quint8 c;

	c = 0;
	for (i = n - 1; i < n; i--) {
		dig = a[i] + b[i] + c;
		c = (dig >= 0x100);
		d[i] = dig;
	}

	if (c)
		bn_sub_modulus(d, N, n);

	if (bn_compare(d, N, n) >= 0)
		bn_sub_modulus(d, N, n);
}

void bn_mul(quint8 *d, quint8 *a, quint8 *b, quint8 *N, quint32 n)
{
    quint32 i;
    quint8 mask;

	bn_zero(d, n);

	for (i = 0; i < n; i++)
		for (mask = 0x80; mask != 0; mask >>= 1) {
			bn_add(d, d, d, N, n);
			if ((a[i] & mask) != 0)
				bn_add(d, d, b, N, n);
		}
}

void bn_exp(quint8 *d, quint8 *a, quint8 *N, quint32 n, quint8 *e, quint32 en)
{
    quint8 t[512];
    quint32 i;
    quint8 mask;

	bn_zero(d, n);
	d[n-1] = 1;
	for (i = 0; i < en; i++)
		for (mask = 0x80; mask != 0; mask >>= 1) {
			bn_mul(t, d, d, N, n);
			if ((e[i] & mask) != 0)
				bn_mul(d, t, a, N, n);
			else
				bn_copy(d, t, n);
		}
}

// only for prime N -- stupid but lazy, see if I care
void bn_inv(quint8 *d, quint8 *a, quint8 *N, quint32 n)
{
    quint8 t[512], s[512];

	bn_copy(t, N, n);
	bn_zero(s, n);
	s[n-1] = 2;
	bn_sub_modulus(t, s, n);
	bn_exp(d, a, N, n, t, n);
}
