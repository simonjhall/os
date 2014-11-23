/*
 * clib.cpp
 *
 *  Created on: 28 May 2013
 *      Author: simon
 */

#include <string.h>
#include <stddef.h>

#include "common.h"
#include "malloc.h"
#include "virt_memory.h"
//#include "Process.h"

#pragma GCC push_options
#pragma GCC optimize ("O0")

extern "C" char *
__strncpy_chk (char *s1, const char *s2, size_t n, size_t s1len)
{
  char c;
  char *s = s1;

  if (__builtin_expect (s1len < n, 0))
    ASSERT(0);

  --s1;

  if (n >= 4)
    {
      size_t n4 = n >> 2;

      for (;;)
	{
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  if (--n4 == 0)
	    goto last_chars;
	}
      n = n - (s1 - s) - 1;
      if (n == 0)
	return s;
      goto zero_fill;
    }

 last_chars:
  n &= 3;
  if (n == 0)
    return s;

  do
    {
      c = *s2++;
      *++s1 = c;
      if (--n == 0)
	return s;
    }
  while (c != '\0');

 zero_fill:
  do
    *++s1 = '\0';
  while (--n > 0);

  return s;
}

#ifdef __ARM_ARCH_7M__
extern "C" void *memset(void *s, int c, size_t n)
{
	char b = (char)c;
	char *p = (char *)s;

	for (size_t count = 0; count < n; count++)
		p[count] = b;

	return s;
}
#endif

#define	op_t	unsigned long int
#define OPSIZ	(sizeof(op_t))

/* Type to use for unaligned operations.  */
typedef unsigned char byte;

#define MERGE(w0, sh_1, w1, sh_2) (((w0) >> (sh_1)) | ((w1) << (sh_2)))

/* Copy exactly NBYTES bytes from SRC_BP to DST_BP,
   without any assumptions about alignment of the pointers.  */
#define BYTE_COPY_FWD(dst_bp, src_bp, nbytes)				      \
  do									      \
    {									      \
      size_t __nbytes = (nbytes);					      \
      while (__nbytes > 0)						      \
	{								      \
	  byte __x = ((byte *) src_bp)[0];				      \
	  src_bp += 1;							      \
	  __nbytes -= 1;						      \
	  ((byte *) dst_bp)[0] = __x;					      \
	  dst_bp += 1;							      \
	}								      \
    } while (0)

/* Copy exactly NBYTES_TO_COPY bytes from SRC_END_PTR to DST_END_PTR,
   beginning at the bytes right before the pointers and continuing towards
   smaller addresses.  Don't assume anything about alignment of the
   pointers.  */
#define BYTE_COPY_BWD(dst_ep, src_ep, nbytes)				      \
  do									      \
    {									      \
      size_t __nbytes = (nbytes);					      \
      while (__nbytes > 0)						      \
	{								      \
	  byte __x;							      \
	  src_ep -= 1;							      \
	  __x = ((byte *) src_ep)[0];					      \
	  dst_ep -= 1;							      \
	  __nbytes -= 1;						      \
	  ((byte *) dst_ep)[0] = __x;					      \
	}								      \
    } while (0)

/* Copy *up to* NBYTES bytes from SRC_BP to DST_BP, with
   the assumption that DST_BP is aligned on an OPSIZ multiple.  If
   not all bytes could be easily copied, store remaining number of bytes
   in NBYTES_LEFT, otherwise store 0.  */
extern void _wordcopy_fwd_aligned (long int, long int, size_t);
extern void _wordcopy_fwd_dest_aligned (long int, long int, size_t);
#define WORD_COPY_FWD(dst_bp, src_bp, nbytes_left, nbytes)		      \
  do									      \
    {									      \
      if (src_bp % OPSIZ == 0)						      \
	_wordcopy_fwd_aligned (dst_bp, src_bp, (nbytes) / OPSIZ);	      \
      else								      \
	_wordcopy_fwd_dest_aligned (dst_bp, src_bp, (nbytes) / OPSIZ);	      \
      src_bp += (nbytes) & -OPSIZ;					      \
      dst_bp += (nbytes) & -OPSIZ;					      \
      (nbytes_left) = (nbytes) % OPSIZ;					      \
    } while (0)

/* Copy *up to* NBYTES_TO_COPY bytes from SRC_END_PTR to DST_END_PTR,
   beginning at the words (of type op_t) right before the pointers and
   continuing towards smaller addresses.  May take advantage of that
   DST_END_PTR is aligned on an OPSIZ multiple.  If not all bytes could be
   easily copied, store remaining number of bytes in NBYTES_REMAINING,
   otherwise store 0.  */
extern void _wordcopy_bwd_aligned (long int, long int, size_t);
extern void _wordcopy_bwd_dest_aligned (long int, long int, size_t);
#define WORD_COPY_BWD(dst_ep, src_ep, nbytes_left, nbytes)		      \
  do									      \
    {									      \
      if (src_ep % OPSIZ == 0)						      \
	_wordcopy_bwd_aligned (dst_ep, src_ep, (nbytes) / OPSIZ);	      \
      else								      \
	_wordcopy_bwd_dest_aligned (dst_ep, src_ep, (nbytes) / OPSIZ);	      \
      src_ep -= (nbytes) & -OPSIZ;					      \
      dst_ep -= (nbytes) & -OPSIZ;					      \
      (nbytes_left) = (nbytes) % OPSIZ;					      \
    } while (0)


/* Threshold value for when to enter the unrolled loops.  */
#define	OP_T_THRES	16

extern "C" void *__memcpy_chk(void *dest, const void *src,
size_t copy_amount, size_t dest_len)
{
	ASSERT (copy_amount <= dest_len);
	return memcpy(dest, src, copy_amount);
}

#ifdef __ARM_ARCH_7M__
extern "C" void *memcpy(void *dest, const void *src, size_t n)
{
	char *d = (char *)dest;
	char *s = (char *)src;

	for (size_t count = 0; count < n; count++)
		d[count] = s[count];

	return dest;
}
#endif

extern "C" void *memmove(void *dest, const void *src, size_t len)
{
  unsigned long int dstp = (long int) dest;
  unsigned long int srcp = (long int) src;

  /* This test makes the forward copying code be used whenever possible.
     Reduces the working set.  */
  if (dstp - srcp >= len)	/* *Unsigned* compare!  */
    {
      /* Copy from the beginning to the end.  */

      /* If there not too few bytes to copy, use word copy.  */
      if (len >= OP_T_THRES)
	{
	  /* Copy just a few bytes to make DSTP aligned.  */
	  len -= (-dstp) % OPSIZ;
	  BYTE_COPY_FWD (dstp, srcp, (-dstp) % OPSIZ);

	  /* Copy whole pages from SRCP to DSTP by virtual address
	     manipulation, as much as possible.  */

//	  PAGE_COPY_FWD_MAYBE (dstp, srcp, len, len);
	  memcpy((void *)dstp, (void *)srcp, len);

	  /* Copy from SRCP to DSTP taking advantage of the known
	     alignment of DSTP.  Number of bytes remaining is put
	     in the third argument, i.e. in LEN.  This number may
	     vary from machine to machine.  */

	  WORD_COPY_FWD (dstp, srcp, len, len);

	  /* Fall out and copy the tail.  */
	}

      /* There are just a few bytes to copy.  Use byte memory operations.  */
      BYTE_COPY_FWD (dstp, srcp, len);
    }
  else
    {
      /* Copy from the end to the beginning.  */
      srcp += len;
      dstp += len;

      /* If there not too few bytes to copy, use word copy.  */
      if (len >= OP_T_THRES)
	{
	  /* Copy just a few bytes to make DSTP aligned.  */
	  len -= dstp % OPSIZ;
	  BYTE_COPY_BWD (dstp, srcp, dstp % OPSIZ);

	  /* Copy from SRCP to DSTP taking advantage of the known
	     alignment of DSTP.  Number of bytes remaining is put
	     in the third argument, i.e. in LEN.  This number may
	     vary from machine to machine.  */

	  WORD_COPY_BWD (dstp, srcp, len, len);

	  /* Fall out and copy the tail.  */
	}

      /* There are just a few bytes to copy.  Use byte memory operations.  */
      BYTE_COPY_BWD (dstp, srcp, len);
    }

  return dest;
}

void
_wordcopy_fwd_aligned (long int dstp, long int srcp, size_t len)
{
  op_t a0, a1;

  switch (len % 8)
    {
    case 2:
      a0 = ((op_t *) srcp)[0];
      srcp -= 6 * OPSIZ;
      dstp -= 7 * OPSIZ;
      len += 6;
      goto do1;
    case 3:
      a1 = ((op_t *) srcp)[0];
      srcp -= 5 * OPSIZ;
      dstp -= 6 * OPSIZ;
      len += 5;
      goto do2;
    case 4:
      a0 = ((op_t *) srcp)[0];
      srcp -= 4 * OPSIZ;
      dstp -= 5 * OPSIZ;
      len += 4;
      goto do3;
    case 5:
      a1 = ((op_t *) srcp)[0];
      srcp -= 3 * OPSIZ;
      dstp -= 4 * OPSIZ;
      len += 3;
      goto do4;
    case 6:
      a0 = ((op_t *) srcp)[0];
      srcp -= 2 * OPSIZ;
      dstp -= 3 * OPSIZ;
      len += 2;
      goto do5;
    case 7:
      a1 = ((op_t *) srcp)[0];
      srcp -= 1 * OPSIZ;
      dstp -= 2 * OPSIZ;
      len += 1;
      goto do6;

    case 0:
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	return;
      a0 = ((op_t *) srcp)[0];
      srcp -= 0 * OPSIZ;
      dstp -= 1 * OPSIZ;
      goto do7;
    case 1:
      a1 = ((op_t *) srcp)[0];
      srcp -=-1 * OPSIZ;
      dstp -= 0 * OPSIZ;
      len -= 1;
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	goto do0;
      goto do8;			/* No-op.  */
    }

  do
    {
    do8:
      a0 = ((op_t *) srcp)[0];
      ((op_t *) dstp)[0] = a1;
    do7:
      a1 = ((op_t *) srcp)[1];
      ((op_t *) dstp)[1] = a0;
    do6:
      a0 = ((op_t *) srcp)[2];
      ((op_t *) dstp)[2] = a1;
    do5:
      a1 = ((op_t *) srcp)[3];
      ((op_t *) dstp)[3] = a0;
    do4:
      a0 = ((op_t *) srcp)[4];
      ((op_t *) dstp)[4] = a1;
    do3:
      a1 = ((op_t *) srcp)[5];
      ((op_t *) dstp)[5] = a0;
    do2:
      a0 = ((op_t *) srcp)[6];
      ((op_t *) dstp)[6] = a1;
    do1:
      a1 = ((op_t *) srcp)[7];
      ((op_t *) dstp)[7] = a0;

      srcp += 8 * OPSIZ;
      dstp += 8 * OPSIZ;
      len -= 8;
    }
  while (len != 0);

  /* This is the right position for do0.  Please don't move
     it into the loop.  */
 do0:
  ((op_t *) dstp)[0] = a1;
}

/* _wordcopy_fwd_dest_aligned -- Copy block beginning at SRCP to
   block beginning at DSTP with LEN `op_t' words (not LEN bytes!).
   DSTP should be aligned for memory operations on `op_t's, but SRCP must
   *not* be aligned.  */

void
_wordcopy_fwd_dest_aligned (long int dstp, long int srcp, size_t len)
{
  op_t a0, a1, a2, a3;
  int sh_1, sh_2;

  /* Calculate how to shift a word read at the memory operation
     aligned srcp to make it aligned for copy.  */

  sh_1 = 8 * (srcp % OPSIZ);
  sh_2 = 8 * OPSIZ - sh_1;

  /* Make SRCP aligned by rounding it down to the beginning of the `op_t'
     it points in the middle of.  */
  srcp &= -OPSIZ;

  switch (len % 4)
    {
    case 2:
      a1 = ((op_t *) srcp)[0];
      a2 = ((op_t *) srcp)[1];
      srcp -= 1 * OPSIZ;
      dstp -= 3 * OPSIZ;
      len += 2;
      goto do1;
    case 3:
      a0 = ((op_t *) srcp)[0];
      a1 = ((op_t *) srcp)[1];
      srcp -= 0 * OPSIZ;
      dstp -= 2 * OPSIZ;
      len += 1;
      goto do2;
    case 0:
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	return;
      a3 = ((op_t *) srcp)[0];
      a0 = ((op_t *) srcp)[1];
      srcp -=-1 * OPSIZ;
      dstp -= 1 * OPSIZ;
      len += 0;
      goto do3;
    case 1:
      a2 = ((op_t *) srcp)[0];
      a3 = ((op_t *) srcp)[1];
      srcp -=-2 * OPSIZ;
      dstp -= 0 * OPSIZ;
      len -= 1;
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	goto do0;
      goto do4;			/* No-op.  */
    }

  do
    {
    do4:
      a0 = ((op_t *) srcp)[0];
      ((op_t *) dstp)[0] = MERGE (a2, sh_1, a3, sh_2);
    do3:
      a1 = ((op_t *) srcp)[1];
      ((op_t *) dstp)[1] = MERGE (a3, sh_1, a0, sh_2);
    do2:
      a2 = ((op_t *) srcp)[2];
      ((op_t *) dstp)[2] = MERGE (a0, sh_1, a1, sh_2);
    do1:
      a3 = ((op_t *) srcp)[3];
      ((op_t *) dstp)[3] = MERGE (a1, sh_1, a2, sh_2);

      srcp += 4 * OPSIZ;
      dstp += 4 * OPSIZ;
      len -= 4;
    }
  while (len != 0);

  /* This is the right position for do0.  Please don't move
     it into the loop.  */
 do0:
  ((op_t *) dstp)[0] = MERGE (a2, sh_1, a3, sh_2);
}

/* _wordcopy_bwd_aligned -- Copy block finishing right before
   SRCP to block finishing right before DSTP with LEN `op_t' words
   (not LEN bytes!).  Both SRCP and DSTP should be aligned for memory
   operations on `op_t's.  */

void
_wordcopy_bwd_aligned (long int dstp, long int srcp, size_t len)
{
  op_t a0, a1;

  switch (len % 8)
    {
    case 2:
      srcp -= 2 * OPSIZ;
      dstp -= 1 * OPSIZ;
      a0 = ((op_t *) srcp)[1];
      len += 6;
      goto do1;
    case 3:
      srcp -= 3 * OPSIZ;
      dstp -= 2 * OPSIZ;
      a1 = ((op_t *) srcp)[2];
      len += 5;
      goto do2;
    case 4:
      srcp -= 4 * OPSIZ;
      dstp -= 3 * OPSIZ;
      a0 = ((op_t *) srcp)[3];
      len += 4;
      goto do3;
    case 5:
      srcp -= 5 * OPSIZ;
      dstp -= 4 * OPSIZ;
      a1 = ((op_t *) srcp)[4];
      len += 3;
      goto do4;
    case 6:
      srcp -= 6 * OPSIZ;
      dstp -= 5 * OPSIZ;
      a0 = ((op_t *) srcp)[5];
      len += 2;
      goto do5;
    case 7:
      srcp -= 7 * OPSIZ;
      dstp -= 6 * OPSIZ;
      a1 = ((op_t *) srcp)[6];
      len += 1;
      goto do6;

    case 0:
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	return;
      srcp -= 8 * OPSIZ;
      dstp -= 7 * OPSIZ;
      a0 = ((op_t *) srcp)[7];
      goto do7;
    case 1:
      srcp -= 9 * OPSIZ;
      dstp -= 8 * OPSIZ;
      a1 = ((op_t *) srcp)[8];
      len -= 1;
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	goto do0;
      goto do8;			/* No-op.  */
    }

  do
    {
    do8:
      a0 = ((op_t *) srcp)[7];
      ((op_t *) dstp)[7] = a1;
    do7:
      a1 = ((op_t *) srcp)[6];
      ((op_t *) dstp)[6] = a0;
    do6:
      a0 = ((op_t *) srcp)[5];
      ((op_t *) dstp)[5] = a1;
    do5:
      a1 = ((op_t *) srcp)[4];
      ((op_t *) dstp)[4] = a0;
    do4:
      a0 = ((op_t *) srcp)[3];
      ((op_t *) dstp)[3] = a1;
    do3:
      a1 = ((op_t *) srcp)[2];
      ((op_t *) dstp)[2] = a0;
    do2:
      a0 = ((op_t *) srcp)[1];
      ((op_t *) dstp)[1] = a1;
    do1:
      a1 = ((op_t *) srcp)[0];
      ((op_t *) dstp)[0] = a0;

      srcp -= 8 * OPSIZ;
      dstp -= 8 * OPSIZ;
      len -= 8;
    }
  while (len != 0);

  /* This is the right position for do0.  Please don't move
     it into the loop.  */
 do0:
  ((op_t *) dstp)[7] = a1;
}

/* _wordcopy_bwd_dest_aligned -- Copy block finishing right
   before SRCP to block finishing right before DSTP with LEN `op_t'
   words (not LEN bytes!).  DSTP should be aligned for memory
   operations on `op_t', but SRCP must *not* be aligned.  */

void
_wordcopy_bwd_dest_aligned (long int dstp, long int srcp, size_t len)
{
  op_t a0, a1, a2, a3;
  int sh_1, sh_2;

  /* Calculate how to shift a word read at the memory operation
     aligned srcp to make it aligned for copy.  */

  sh_1 = 8 * (srcp % OPSIZ);
  sh_2 = 8 * OPSIZ - sh_1;

  /* Make srcp aligned by rounding it down to the beginning of the op_t
     it points in the middle of.  */
  srcp &= -OPSIZ;
  srcp += OPSIZ;

  switch (len % 4)
    {
    case 2:
      srcp -= 3 * OPSIZ;
      dstp -= 1 * OPSIZ;
      a2 = ((op_t *) srcp)[2];
      a1 = ((op_t *) srcp)[1];
      len += 2;
      goto do1;
    case 3:
      srcp -= 4 * OPSIZ;
      dstp -= 2 * OPSIZ;
      a3 = ((op_t *) srcp)[3];
      a2 = ((op_t *) srcp)[2];
      len += 1;
      goto do2;
    case 0:
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	return;
      srcp -= 5 * OPSIZ;
      dstp -= 3 * OPSIZ;
      a0 = ((op_t *) srcp)[4];
      a3 = ((op_t *) srcp)[3];
      goto do3;
    case 1:
      srcp -= 6 * OPSIZ;
      dstp -= 4 * OPSIZ;
      a1 = ((op_t *) srcp)[5];
      a0 = ((op_t *) srcp)[4];
      len -= 1;
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	goto do0;
      goto do4;			/* No-op.  */
    }

  do
    {
    do4:
      a3 = ((op_t *) srcp)[3];
      ((op_t *) dstp)[3] = MERGE (a0, sh_1, a1, sh_2);
    do3:
      a2 = ((op_t *) srcp)[2];
      ((op_t *) dstp)[2] = MERGE (a3, sh_1, a0, sh_2);
    do2:
      a1 = ((op_t *) srcp)[1];
      ((op_t *) dstp)[1] = MERGE (a2, sh_1, a3, sh_2);
    do1:
      a0 = ((op_t *) srcp)[0];
      ((op_t *) dstp)[0] = MERGE (a1, sh_1, a2, sh_2);

      srcp -= 4 * OPSIZ;
      dstp -= 4 * OPSIZ;
      len -= 4;
    }
  while (len != 0);

  /* This is the right position for do0.  Please don't move
     it into the loop.  */
 do0:
  ((op_t *) dstp)[3] = MERGE (a0, sh_1, a1, sh_2);
}

extern "C" int strcmp (const char *p1, const char *p2)
{
  register const unsigned char *s1 = (const unsigned char *) p1;
  register const unsigned char *s2 = (const unsigned char *) p2;
  unsigned char c1, c2;

  do
    {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0')
	return c1 - c2;
    }
  while (c1 == c2);

  return c1 - c2;
}

extern "C" char *strncpy(char *s1, const char *s2, size_t n)
{
  char c;
  char *s = s1;

  --s1;

  if (n >= 4)
    {
      size_t n4 = n >> 2;

      for (;;)
	{
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  if (--n4 == 0)
	    goto last_chars;
	}
      n = n - (s1 - s) - 1;
      if (n == 0)
	return s;
      goto zero_fill;
    }

 last_chars:
  n &= 3;
  if (n == 0)
    return s;

  do
    {
      c = *s2++;
      *++s1 = c;
      if (--n == 0)
	return s;
    }
  while (c != '\0');

 zero_fill:
  do
    *++s1 = '\0';
  while (--n > 0);

  return s;
}

extern "C" char *strcpy(char *dest, const char *src)
{
  char c;
  char *s = (char *) src;
  const ptrdiff_t off = dest - s - 1;

  do
    {
      c = *s++;
      s[off] = c;
    }
  while (c != '\0');

  return dest;
}

extern "C" size_t strlen (const char *str)
{
  const char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, himagic, lomagic;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = str; ((unsigned long int) char_ptr
			& (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == '\0')
      return char_ptr - str;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  himagic = 0x80808080L;
  lomagic = 0x01010101L;
  if (sizeof (longword) > 4)
    {
      /* 64-bit version of the magic.  */
      /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
      himagic = ((himagic << 16) << 16) | himagic;
      lomagic = ((lomagic << 16) << 16) | lomagic;
    }
//  if (sizeof (longword) > 8)
//    abort ();

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  for (;;)
    {
      longword = *longword_ptr++;

      if (((longword - lomagic) & ~longword & himagic) != 0)
	{
	  /* Which of the bytes was the zero?  If none of them were, it was
	     a misfire; continue the search.  */

	  const char *cp = (const char *) (longword_ptr - 1);

	  if (cp[0] == 0)
	    return cp - str;
	  if (cp[1] == 0)
	    return cp - str + 1;
	  if (cp[2] == 0)
	    return cp - str + 2;
	  if (cp[3] == 0)
	    return cp - str + 3;
	  if (sizeof (longword) > 4)
	    {
	      if (cp[4] == 0)
		return cp - str + 4;
	      if (cp[5] == 0)
		return cp - str + 5;
	      if (cp[6] == 0)
		return cp - str + 6;
	      if (cp[7] == 0)
		return cp - str + 7;
	    }
	}
    }
}

extern "C" int strncmp (const char *s1, const char *s2, size_t n)
{
  unsigned char c1 = '\0';
  unsigned char c2 = '\0';

  if (n >= 4)
    {
      size_t n4 = n >> 2;
      do
	{
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	} while (--n4 > 0);
      n &= 3;
    }

  while (n > 0)
    {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
	return c1 - c2;
      n--;
    }

  return c1 - c2;
}

# define LONG_NEEDLE_THRESHOLD 32U
# define CANON_ELEMENT(c) c
# define CMP_FUNC memcmp
#define AVAILABLE(h, h_l, j, n_l)			\
  (!memchr ((h) + (h_l), '\0', (j) + (n_l) - (h_l))	\
   && ((h_l) = (j) + (n_l)))
#ifndef SIZE_MAX
#  define SIZE_MAX		(4294967295U)
#endif
#define	MAX(a,b) (((a)>(b))?(a):(b))
# define RET0_IF_0(a) /* nothing */
#define CHAR_BIT __CHAR_BIT__

static size_t
critical_factorization (const unsigned char *needle, size_t needle_len,
			size_t *period)
{
  /* Index of last byte of left half, or SIZE_MAX.  */
  size_t max_suffix, max_suffix_rev;
  size_t j; /* Index into NEEDLE for current candidate suffix.  */
  size_t k; /* Offset into current period.  */
  size_t p; /* Intermediate period.  */
  unsigned char a, b; /* Current comparison bytes.  */

  /* Invariants:
     0 <= j < NEEDLE_LEN - 1
     -1 <= max_suffix{,_rev} < j (treating SIZE_MAX as if it were signed)
     min(max_suffix, max_suffix_rev) < global period of NEEDLE
     1 <= p <= global period of NEEDLE
     p == global period of the substring NEEDLE[max_suffix{,_rev}+1...j]
     1 <= k <= p
  */

  /* Perform lexicographic search.  */
  max_suffix = SIZE_MAX;
  j = 0;
  k = p = 1;
  while (j + k < needle_len)
    {
      a = CANON_ELEMENT (needle[j + k]);
      b = CANON_ELEMENT (needle[max_suffix + k]);
      if (a < b)
	{
	  /* Suffix is smaller, period is entire prefix so far.  */
	  j += k;
	  k = 1;
	  p = j - max_suffix;
	}
      else if (a == b)
	{
	  /* Advance through repetition of the current period.  */
	  if (k != p)
	    ++k;
	  else
	    {
	      j += p;
	      k = 1;
	    }
	}
      else /* b < a */
	{
	  /* Suffix is larger, start over from current location.  */
	  max_suffix = j++;
	  k = p = 1;
	}
    }
  *period = p;

  /* Perform reverse lexicographic search.  */
  max_suffix_rev = SIZE_MAX;
  j = 0;
  k = p = 1;
  while (j + k < needle_len)
    {
      a = CANON_ELEMENT (needle[j + k]);
      b = CANON_ELEMENT (needle[max_suffix_rev + k]);
      if (b < a)
	{
	  /* Suffix is smaller, period is entire prefix so far.  */
	  j += k;
	  k = 1;
	  p = j - max_suffix_rev;
	}
      else if (a == b)
	{
	  /* Advance through repetition of the current period.  */
	  if (k != p)
	    ++k;
	  else
	    {
	      j += p;
	      k = 1;
	    }
	}
      else /* a < b */
	{
	  /* Suffix is larger, start over from current location.  */
	  max_suffix_rev = j++;
	  k = p = 1;
	}
    }

  /* Choose the longer suffix.  Return the first byte of the right
     half, rather than the last byte of the left half.  */
  if (max_suffix_rev + 1 < max_suffix + 1)
    return max_suffix + 1;
  *period = p;
  return max_suffix_rev + 1;
}

static char *
two_way_short_needle (const unsigned char *haystack, size_t haystack_len,
		      const unsigned char *needle, size_t needle_len)
{
  size_t i; /* Index into current byte of NEEDLE.  */
  size_t j; /* Index into current window of HAYSTACK.  */
  size_t period; /* The period of the right half of needle.  */
  size_t suffix; /* The index of the right half of needle.  */

  /* Factor the needle into two halves, such that the left half is
     smaller than the global period, and the right half is
     periodic (with a period as large as NEEDLE_LEN - suffix).  */
  suffix = critical_factorization (needle, needle_len, &period);

  /* Perform the search.  Each iteration compares the right half
     first.  */
  if (CMP_FUNC (needle, needle + period, suffix) == 0)
    {
      /* Entire needle is periodic; a mismatch can only advance by the
	 period, so use memory to avoid rescanning known occurrences
	 of the period.  */
      size_t memory = 0;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
	{
	  const unsigned char *pneedle;
	  const unsigned char *phaystack;

	  /* Scan for matches in right half.  */
	  i = MAX (suffix, memory);
	  pneedle = &needle[i];
	  phaystack = &haystack[i + j];
	  while (i < needle_len && (CANON_ELEMENT (*pneedle++)
				    == CANON_ELEMENT (*phaystack++)))
	    ++i;
	  if (needle_len <= i)
	    {
	      /* Scan for matches in left half.  */
	      i = suffix - 1;
	      pneedle = &needle[i];
	      phaystack = &haystack[i + j];
	      while (memory < i + 1 && (CANON_ELEMENT (*pneedle--)
					== CANON_ELEMENT (*phaystack--)))
		--i;
	      if (i + 1 < memory + 1)
		return (char *) (haystack + j);
	      /* No match, so remember how many repetitions of period
		 on the right half were scanned.  */
	      j += period;
	      memory = needle_len - period;
	    }
	  else
	    {
	      j += i - suffix + 1;
	      memory = 0;
	    }
	}
    }
  else
    {
      const unsigned char *phaystack = &haystack[suffix];
      /* The comparison always starts from needle[suffix], so cache it
	 and use an optimized first-character loop.  */
      unsigned char needle_suffix = CANON_ELEMENT (needle[suffix]);

#if CHECK_EOL
      /* We start matching from the SUFFIX'th element, so make sure we
	 don't hit '\0' before that.  */
      if (haystack_len < suffix + 1
	  && !AVAILABLE (haystack, haystack_len, 0, suffix + 1))
	return NULL;
#endif

      /* The two halves of needle are distinct; no extra memory is
	 required, and any mismatch results in a maximal shift.  */
      period = MAX (suffix, needle_len - suffix) + 1;
      j = 0;
      while (1
#if !CHECK_EOL
	     && AVAILABLE (haystack, haystack_len, j, needle_len)
#endif
	     )
	{
	  unsigned char haystack_char;
	  const unsigned char *pneedle;

	  /* TODO: The first-character loop can be sped up by adapting
	     longword-at-a-time implementation of memchr/strchr.  */
	  if (needle_suffix
	      != (haystack_char = CANON_ELEMENT (*phaystack++)))
	    {
	      RET0_IF_0 (haystack_char);
#if !CHECK_EOL
	      ++j;
#endif
	      continue;
	    }

#if CHECK_EOL
	  /* Calculate J if it wasn't kept up-to-date in the first-character
	     loop.  */
	  j = phaystack - &haystack[suffix] - 1;
#endif

	  /* Scan for matches in right half.  */
	  i = suffix + 1;
	  pneedle = &needle[i];
	  while (i < needle_len)
	    {
	      if (CANON_ELEMENT (*pneedle++)
		  != (haystack_char = CANON_ELEMENT (*phaystack++)))
		{
		  RET0_IF_0 (haystack_char);
		  break;
		}
	      ++i;
	    }
	  if (needle_len <= i)
	    {
	      /* Scan for matches in left half.  */
	      i = suffix - 1;
	      pneedle = &needle[i];
	      phaystack = &haystack[i + j];
	      while (i != SIZE_MAX)
		{
		  if (CANON_ELEMENT (*pneedle--)
		      != (haystack_char = CANON_ELEMENT (*phaystack--)))
		    {
		      RET0_IF_0 (haystack_char);
		      break;
		    }
		  --i;
		}
	      if (i == SIZE_MAX)
		return (char *) (haystack + j);
	      j += period;
	    }
	  else
	    j += i - suffix + 1;

#if CHECK_EOL
	  if (!AVAILABLE (haystack, haystack_len, j, needle_len))
	    break;
#endif

	  phaystack = &haystack[suffix + j];
	}
    }
  return NULL;
}

/* Return the first location of non-empty NEEDLE within HAYSTACK, or
   NULL.  HAYSTACK_LEN is the minimum known length of HAYSTACK.  This
   method is optimized for LONG_NEEDLE_THRESHOLD <= NEEDLE_LEN.
   Performance is guaranteed to be linear, with an initialization cost
   of 3 * NEEDLE_LEN + (1 << CHAR_BIT) operations.

   If AVAILABLE does not modify HAYSTACK_LEN (as in memmem), then at
   most 2 * HAYSTACK_LEN - NEEDLE_LEN comparisons occur in searching,
   and sublinear performance O(HAYSTACK_LEN / NEEDLE_LEN) is possible.
   If AVAILABLE modifies HAYSTACK_LEN (as in strstr), then at most 3 *
   HAYSTACK_LEN - NEEDLE_LEN comparisons occur in searching, and
   sublinear performance is not possible.  */
static char *
two_way_long_needle (const unsigned char *haystack, size_t haystack_len,
		     const unsigned char *needle, size_t needle_len)
{
  size_t i; /* Index into current byte of NEEDLE.  */
  size_t j; /* Index into current window of HAYSTACK.  */
  size_t period; /* The period of the right half of needle.  */
  size_t suffix; /* The index of the right half of needle.  */
  size_t shift_table[1U << CHAR_BIT]; /* See below.  */

  /* Factor the needle into two halves, such that the left half is
     smaller than the global period, and the right half is
     periodic (with a period as large as NEEDLE_LEN - suffix).  */
  suffix = critical_factorization (needle, needle_len, &period);

  /* Populate shift_table.  For each possible byte value c,
     shift_table[c] is the distance from the last occurrence of c to
     the end of NEEDLE, or NEEDLE_LEN if c is absent from the NEEDLE.
     shift_table[NEEDLE[NEEDLE_LEN - 1]] contains the only 0.  */
  for (i = 0; i < 1U << CHAR_BIT; i++)
    shift_table[i] = needle_len;
  for (i = 0; i < needle_len; i++)
    shift_table[CANON_ELEMENT (needle[i])] = needle_len - i - 1;

  /* Perform the search.  Each iteration compares the right half
     first.  */
  if (CMP_FUNC (needle, needle + period, suffix) == 0)
    {
      /* Entire needle is periodic; a mismatch can only advance by the
	 period, so use memory to avoid rescanning known occurrences
	 of the period.  */
      size_t memory = 0;
      size_t shift;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
	{
	  const unsigned char *pneedle;
	  const unsigned char *phaystack;

	  /* Check the last byte first; if it does not match, then
	     shift to the next possible match location.  */
	  shift = shift_table[CANON_ELEMENT (haystack[j + needle_len - 1])];
	  if (0 < shift)
	    {
	      if (memory && shift < period)
		{
		  /* Since needle is periodic, but the last period has
		     a byte out of place, there can be no match until
		     after the mismatch.  */
		  shift = needle_len - period;
		}
	      memory = 0;
	      j += shift;
	      continue;
	    }
	  /* Scan for matches in right half.  The last byte has
	     already been matched, by virtue of the shift table.  */
	  i = MAX (suffix, memory);
	  pneedle = &needle[i];
	  phaystack = &haystack[i + j];
	  while (i < needle_len - 1 && (CANON_ELEMENT (*pneedle++)
					== CANON_ELEMENT (*phaystack++)))
	    ++i;
	  if (needle_len - 1 <= i)
	    {
	      /* Scan for matches in left half.  */
	      i = suffix - 1;
	      pneedle = &needle[i];
	      phaystack = &haystack[i + j];
	      while (memory < i + 1 && (CANON_ELEMENT (*pneedle--)
					== CANON_ELEMENT (*phaystack--)))
		--i;
	      if (i + 1 < memory + 1)
		return (char *) (haystack + j);
	      /* No match, so remember how many repetitions of period
		 on the right half were scanned.  */
	      j += period;
	      memory = needle_len - period;
	    }
	  else
	    {
	      j += i - suffix + 1;
	      memory = 0;
	    }
	}
    }
  else
    {
      /* The two halves of needle are distinct; no extra memory is
	 required, and any mismatch results in a maximal shift.  */
      size_t shift;
      period = MAX (suffix, needle_len - suffix) + 1;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
	{
	  const unsigned char *pneedle;
	  const unsigned char *phaystack;

	  /* Check the last byte first; if it does not match, then
	     shift to the next possible match location.  */
	  shift = shift_table[CANON_ELEMENT (haystack[j + needle_len - 1])];
	  if (0 < shift)
	    {
	      j += shift;
	      continue;
	    }
	  /* Scan for matches in right half.  The last byte has
	     already been matched, by virtue of the shift table.  */
	  i = suffix;
	  pneedle = &needle[i];
	  phaystack = &haystack[i + j];
	  while (i < needle_len - 1 && (CANON_ELEMENT (*pneedle++)
					== CANON_ELEMENT (*phaystack++)))
	    ++i;
	  if (needle_len - 1 <= i)
	    {
	      /* Scan for matches in left half.  */
	      i = suffix - 1;
	      pneedle = &needle[i];
	      phaystack = &haystack[i + j];
	      while (i != SIZE_MAX && (CANON_ELEMENT (*pneedle--)
				       == CANON_ELEMENT (*phaystack--)))
		--i;
	      if (i == SIZE_MAX)
		return (char *) (haystack + j);
	      j += period;
	    }
	  else
	    j += i - suffix + 1;
	}
    }
  return NULL;
}

#ifdef __ARM_ARCH_7A__
const
#endif
char *
strstr (const char *haystack_start, const char *needle_start)
{
  const char *haystack = haystack_start;
  const char *needle = needle_start;
  size_t needle_len; /* Length of NEEDLE.  */
  size_t haystack_len; /* Known minimum length of HAYSTACK.  */
  bool ok = true; /* True if NEEDLE is prefix of HAYSTACK.  */

  /* Determine length of NEEDLE, and in the process, make sure
     HAYSTACK is at least as long (no point processing all of a long
     NEEDLE if HAYSTACK is too short).  */
  while (*haystack && *needle)
    ok &= *haystack++ == *needle++;
  if (*needle)
    return NULL;
  if (ok)
    return (char *) haystack_start;

  /* Reduce the size of haystack using strchr, since it has a smaller
     linear coefficient than the Two-Way algorithm.  */
  needle_len = needle - needle_start;
  haystack = strchr (haystack_start + 1, *needle_start);
  if (!haystack || __builtin_expect (needle_len == 1, 0))
    return (char *) haystack;
  needle -= needle_len;
  haystack_len = (haystack > haystack_start + needle_len ? 1
		  : needle_len + haystack_start - haystack);

  /* Perform the search.  Abstract memory is considered to be an array
     of 'unsigned char' values, not an array of 'char' values.  See
     ISO C 99 section 6.2.6.1.  */
  if (needle_len < LONG_NEEDLE_THRESHOLD)
    return two_way_short_needle ((const unsigned char *) haystack,
				 haystack_len,
				 (const unsigned char *) needle, needle_len);
  return two_way_long_needle ((const unsigned char *) haystack, haystack_len,
			      (const unsigned char *) needle, needle_len);
}

/*extern "C" void abort(void)
{
	ASSERT(0);
}*/

extern "C" int raise (int sig)
{
//  __set_errno (ENOSYS);
  return -1;
}

# include <sys/types.h>

/* Type to use for aligned memory operations.
   This should normally be the biggest type supported by a single load
   and store.  Must be an unsigned type.  */
# define op_t	unsigned long int
# define OPSIZ	(sizeof(op_t))

/* Threshold value for when to enter the unrolled loops.  */
# define OP_T_THRES	16

/* Type to use for unaligned operations.  */
typedef unsigned char byte;
# define CMP_LT_OR_GT(a, b) memcmp_bytes ((a), (b))

/* BE VERY CAREFUL IF YOU CHANGE THIS CODE!  */

/* The strategy of this memcmp is:

   1. Compare bytes until one of the block pointers is aligned.

   2. Compare using memcmp_common_alignment or
      memcmp_not_common_alignment, regarding the alignment of the other
      block after the initial byte operations.  The maximum number of
      full words (of type op_t) are compared in this way.

   3. Compare the few remaining bytes.  */

#ifndef WORDS_BIGENDIAN
/* memcmp_bytes -- Compare A and B bytewise in the byte order of the machine.
   A and B are known to be different.
   This is needed only on little-endian machines.  */

static int memcmp_bytes (op_t, op_t);

static int
memcmp_bytes (op_t a, op_t b)
{
  long int srcp1 = (long int) &a;
  long int srcp2 = (long int) &b;
  op_t a0, b0;

  do
    {
      a0 = ((byte *) srcp1)[0];
      b0 = ((byte *) srcp2)[0];
      srcp1 += 1;
      srcp2 += 1;
    }
  while (a0 == b0);
  return a0 - b0;
}
#endif

static int memcmp_common_alignment (long, long, size_t);

/* memcmp_common_alignment -- Compare blocks at SRCP1 and SRCP2 with LEN `op_t'
   objects (not LEN bytes!).  Both SRCP1 and SRCP2 should be aligned for
   memory operations on `op_t's.  */
static int
memcmp_common_alignment (long int srcp1, long int srcp2, size_t len)
{
  op_t a0, a1;
  op_t b0, b1;

  switch (len % 4)
    {
    default: /* Avoid warning about uninitialized local variables.  */
    case 2:
      a0 = ((op_t *) srcp1)[0];
      b0 = ((op_t *) srcp2)[0];
      srcp1 -= 2 * OPSIZ;
      srcp2 -= 2 * OPSIZ;
      len += 2;
      goto do1;
    case 3:
      a1 = ((op_t *) srcp1)[0];
      b1 = ((op_t *) srcp2)[0];
      srcp1 -= OPSIZ;
      srcp2 -= OPSIZ;
      len += 1;
      goto do2;
    case 0:
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	return 0;
      a0 = ((op_t *) srcp1)[0];
      b0 = ((op_t *) srcp2)[0];
      goto do3;
    case 1:
      a1 = ((op_t *) srcp1)[0];
      b1 = ((op_t *) srcp2)[0];
      srcp1 += OPSIZ;
      srcp2 += OPSIZ;
      len -= 1;
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	goto do0;
      /* Fall through.  */
    }

  do
    {
      a0 = ((op_t *) srcp1)[0];
      b0 = ((op_t *) srcp2)[0];
      if (a1 != b1)
	return CMP_LT_OR_GT (a1, b1);

    do3:
      a1 = ((op_t *) srcp1)[1];
      b1 = ((op_t *) srcp2)[1];
      if (a0 != b0)
	return CMP_LT_OR_GT (a0, b0);

    do2:
      a0 = ((op_t *) srcp1)[2];
      b0 = ((op_t *) srcp2)[2];
      if (a1 != b1)
	return CMP_LT_OR_GT (a1, b1);

    do1:
      a1 = ((op_t *) srcp1)[3];
      b1 = ((op_t *) srcp2)[3];
      if (a0 != b0)
	return CMP_LT_OR_GT (a0, b0);

      srcp1 += 4 * OPSIZ;
      srcp2 += 4 * OPSIZ;
      len -= 4;
    }
  while (len != 0);

  /* This is the right position for do0.  Please don't move
     it into the loop.  */
 do0:
  if (a1 != b1)
    return CMP_LT_OR_GT (a1, b1);
  return 0;
}

static int memcmp_not_common_alignment (long, long, size_t);

/* memcmp_not_common_alignment -- Compare blocks at SRCP1 and SRCP2 with LEN
   `op_t' objects (not LEN bytes!).  SRCP2 should be aligned for memory
   operations on `op_t', but SRCP1 *should be unaligned*.  */
static int
memcmp_not_common_alignment (long int srcp1, long int srcp2, size_t len)
{
  op_t a0, a1, a2, a3;
  op_t b0, b1, b2, b3;
  op_t x;
  int shl, shr;

  /* Calculate how to shift a word read at the memory operation
     aligned srcp1 to make it aligned for comparison.  */

  shl = 8 * (srcp1 % OPSIZ);
  shr = 8 * OPSIZ - shl;

  /* Make SRCP1 aligned by rounding it down to the beginning of the `op_t'
     it points in the middle of.  */
  srcp1 &= -OPSIZ;

  switch (len % 4)
    {
    default: /* Avoid warning about uninitialized local variables.  */
    case 2:
      a1 = ((op_t *) srcp1)[0];
      a2 = ((op_t *) srcp1)[1];
      b2 = ((op_t *) srcp2)[0];
      srcp1 -= 1 * OPSIZ;
      srcp2 -= 2 * OPSIZ;
      len += 2;
      goto do1;
    case 3:
      a0 = ((op_t *) srcp1)[0];
      a1 = ((op_t *) srcp1)[1];
      b1 = ((op_t *) srcp2)[0];
      srcp2 -= 1 * OPSIZ;
      len += 1;
      goto do2;
    case 0:
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	return 0;
      a3 = ((op_t *) srcp1)[0];
      a0 = ((op_t *) srcp1)[1];
      b0 = ((op_t *) srcp2)[0];
      srcp1 += 1 * OPSIZ;
      goto do3;
    case 1:
      a2 = ((op_t *) srcp1)[0];
      a3 = ((op_t *) srcp1)[1];
      b3 = ((op_t *) srcp2)[0];
      srcp1 += 2 * OPSIZ;
      srcp2 += 1 * OPSIZ;
      len -= 1;
      if (OP_T_THRES <= 3 * OPSIZ && len == 0)
	goto do0;
      /* Fall through.  */
    }

  do
    {
      a0 = ((op_t *) srcp1)[0];
      b0 = ((op_t *) srcp2)[0];
      x = MERGE(a2, shl, a3, shr);
      if (x != b3)
	return CMP_LT_OR_GT (x, b3);

    do3:
      a1 = ((op_t *) srcp1)[1];
      b1 = ((op_t *) srcp2)[1];
      x = MERGE(a3, shl, a0, shr);
      if (x != b0)
	return CMP_LT_OR_GT (x, b0);

    do2:
      a2 = ((op_t *) srcp1)[2];
      b2 = ((op_t *) srcp2)[2];
      x = MERGE(a0, shl, a1, shr);
      if (x != b1)
	return CMP_LT_OR_GT (x, b1);

    do1:
      a3 = ((op_t *) srcp1)[3];
      b3 = ((op_t *) srcp2)[3];
      x = MERGE(a1, shl, a2, shr);
      if (x != b2)
	return CMP_LT_OR_GT (x, b2);

      srcp1 += 4 * OPSIZ;
      srcp2 += 4 * OPSIZ;
      len -= 4;
    }
  while (len != 0);

  /* This is the right position for do0.  Please don't move
     it into the loop.  */
 do0:
  x = MERGE(a2, shl, a3, shr);
  if (x != b3)
    return CMP_LT_OR_GT (x, b3);
  return 0;
}

int
memcmp (const __ptr_t s1,
     const __ptr_t s2,
     size_t len)
{
  op_t a0;
  op_t b0;
  long int srcp1 = (long int) s1;
  long int srcp2 = (long int) s2;
  op_t res;

  if (len >= OP_T_THRES)
    {
      /* There are at least some bytes to compare.  No need to test
	 for LEN == 0 in this alignment loop.  */
      while (srcp2 % OPSIZ != 0)
	{
	  a0 = ((byte *) srcp1)[0];
	  b0 = ((byte *) srcp2)[0];
	  srcp1 += 1;
	  srcp2 += 1;
	  res = a0 - b0;
	  if (res != 0)
	    return res;
	  len -= 1;
	}

      /* SRCP2 is now aligned for memory operations on `op_t'.
	 SRCP1 alignment determines if we can do a simple,
	 aligned compare or need to shuffle bits.  */

      if (srcp1 % OPSIZ == 0)
	res = memcmp_common_alignment (srcp1, srcp2, len / OPSIZ);
      else
	res = memcmp_not_common_alignment (srcp1, srcp2, len / OPSIZ);
      if (res != 0)
	return res;

      /* Number of bytes remaining in the interval [0..OPSIZ-1].  */
      srcp1 += len & -OPSIZ;
      srcp2 += len & -OPSIZ;
      len %= OPSIZ;
    }

  /* There are just a few bytes to compare.  Use byte memory operations.  */
  while (len != 0)
    {
      a0 = ((byte *) srcp1)[0];
      b0 = ((byte *) srcp2)[0];
      srcp1 += 1;
      srcp2 += 1;
      res = a0 - b0;
      if (res != 0)
	return res;
      len -= 1;
    }

  return 0;
}

/* Search no more than N bytes of S for C.  */
#ifdef __ARM_ARCH_7A__
const
#endif
__ptr_t memchr (const __ptr_t s, int c_in, size_t n)
{
  const unsigned char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, charmask;
  unsigned char c;

  c = (unsigned char) c_in;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = (const unsigned char *) s;
       n > 0 && ((unsigned long int) char_ptr
		 & (sizeof (longword) - 1)) != 0;
       --n, ++char_ptr)
    if (*char_ptr == c)
      return (__ptr_t) char_ptr;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */

  if (sizeof (longword) != 4 && sizeof (longword) != 8)
    ASSERT(0);

#if LONG_MAX <= LONG_MAX_32_BITS
  magic_bits = 0x7efefeff;
#else
  magic_bits = ((unsigned long int) 0x7efefefe << 32) | 0xfefefeff;
#endif

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;
#if LONG_MAX > LONG_MAX_32_BITS
  charmask |= charmask << 32;
#endif

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  while (n >= sizeof (longword))
    {
      /* We tentatively exit the loop if adding MAGIC_BITS to
	 LONGWORD fails to change any of the hole bits of LONGWORD.

	 1) Is this safe?  Will it catch all the zero bytes?
	 Suppose there is a byte with all zeros.  Any carry bits
	 propagating from its left will fall into the hole at its
	 least significant bit and stop.  Since there will be no
	 carry from its most significant bit, the LSB of the
	 byte to the left will be unchanged, and the zero will be
	 detected.

	 2) Is this worthwhile?  Will it ignore everything except
	 zero bytes?  Suppose every byte of LONGWORD has a bit set
	 somewhere.  There will be a carry into bit 8.  If bit 8
	 is set, this will carry into bit 16.  If bit 8 is clear,
	 one of bits 9-15 must be set, so there will be a carry
	 into bit 16.  Similarly, there will be a carry into bit
	 24.  If one of bits 24-30 is set, there will be a carry
	 into bit 31, so all of the hole bits will be changed.

	 The one misfire occurs when bits 24-30 are clear and bit
	 31 is set; in this case, the hole at bit 31 is not
	 changed.  If we had access to the processor carry flag,
	 we could close this loophole by putting the fourth hole
	 at bit 32!

	 So it ignores everything except 128's, when they're aligned
	 properly.

	 3) But wait!  Aren't we looking for C, not zero?
	 Good point.  So what we do is XOR LONGWORD with a longword,
	 each of whose bytes is C.  This turns each byte that is C
	 into a zero.  */

      longword = *longword_ptr++ ^ charmask;

      /* Add MAGIC_BITS to LONGWORD.  */
      if ((((longword + magic_bits)

	    /* Set those bits that were unchanged by the addition.  */
	    ^ ~longword)

	   /* Look at only the hole bits.  If any of the hole bits
	      are unchanged, most likely one of the bytes was a
	      zero.  */
	   & ~magic_bits) != 0)
	{
	  /* Which of the bytes was C?  If none of them were, it was
	     a misfire; continue the search.  */

	  const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

	  if (cp[0] == c)
	    return (__ptr_t) cp;
	  if (cp[1] == c)
	    return (__ptr_t) &cp[1];
	  if (cp[2] == c)
	    return (__ptr_t) &cp[2];
	  if (cp[3] == c)
	    return (__ptr_t) &cp[3];
#if LONG_MAX > 2147483647
	  if (cp[4] == c)
	    return (__ptr_t) &cp[4];
	  if (cp[5] == c)
	    return (__ptr_t) &cp[5];
	  if (cp[6] == c)
	    return (__ptr_t) &cp[6];
	  if (cp[7] == c)
	    return (__ptr_t) &cp[7];
#endif
	}

      n -= sizeof (longword);
    }

  char_ptr = (const unsigned char *) longword_ptr;

  while (n-- > 0)
    {
      if (*char_ptr == c)
	return (__ptr_t) char_ptr;
      else
	++char_ptr;
    }

  return 0;
}

#ifdef __ARM_ARCH_7A__
const
#endif
char * strchr (const char *s, int c_in)
{
  const unsigned char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, charmask;
  unsigned char c;

  c = (unsigned char) c_in;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = (const unsigned char *) s;
       ((unsigned long int) char_ptr & (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == c)
      return (char *) char_ptr;
    else if (*char_ptr == '\0')
      return NULL;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  switch (sizeof (longword))
    {
    case 4: magic_bits = 0x7efefeffL; break;
    case 8: magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL; break;
    default:
      ASSERT(0);
    }

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;
  if (sizeof (longword) > 4)
    /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
    charmask |= (charmask << 16) << 16;
  if (sizeof (longword) > 8)
    ASSERT(0);

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  for (;;)
    {
      /* We tentatively exit the loop if adding MAGIC_BITS to
	 LONGWORD fails to change any of the hole bits of LONGWORD.

	 1) Is this safe?  Will it catch all the zero bytes?
	 Suppose there is a byte with all zeros.  Any carry bits
	 propagating from its left will fall into the hole at its
	 least significant bit and stop.  Since there will be no
	 carry from its most significant bit, the LSB of the
	 byte to the left will be unchanged, and the zero will be
	 detected.

	 2) Is this worthwhile?  Will it ignore everything except
	 zero bytes?  Suppose every byte of LONGWORD has a bit set
	 somewhere.  There will be a carry into bit 8.  If bit 8
	 is set, this will carry into bit 16.  If bit 8 is clear,
	 one of bits 9-15 must be set, so there will be a carry
	 into bit 16.  Similarly, there will be a carry into bit
	 24.  If one of bits 24-30 is set, there will be a carry
	 into bit 31, so all of the hole bits will be changed.

	 The one misfire occurs when bits 24-30 are clear and bit
	 31 is set; in this case, the hole at bit 31 is not
	 changed.  If we had access to the processor carry flag,
	 we could close this loophole by putting the fourth hole
	 at bit 32!

	 So it ignores everything except 128's, when they're aligned
	 properly.

	 3) But wait!  Aren't we looking for C as well as zero?
	 Good point.  So what we do is XOR LONGWORD with a longword,
	 each of whose bytes is C.  This turns each byte that is C
	 into a zero.  */

      longword = *longword_ptr++;

      /* Add MAGIC_BITS to LONGWORD.  */
      if ((((longword + magic_bits)

	    /* Set those bits that were unchanged by the addition.  */
	    ^ ~longword)

	   /* Look at only the hole bits.  If any of the hole bits
	      are unchanged, most likely one of the bytes was a
	      zero.  */
	   & ~magic_bits) != 0 ||

	  /* That caught zeroes.  Now test for C.  */
	  ((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask))
	   & ~magic_bits) != 0)
	{
	  /* Which of the bytes was C or zero?
	     If none of them were, it was a misfire; continue the search.  */

	  const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

	  if (*cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (*++cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (*++cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (*++cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (sizeof (longword) > 4)
	    {
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	    }
	}
    }

  return NULL;
}










static mspace s_pool;
static bool s_poolInit = false;

bool InitMempool(void *pBase, unsigned int numPages, bool phys, mspace *pPool)
{
	if (!pPool)
		pPool = &s_pool;

	if (phys || VirtMem::AllocAndMapVirtContig(pBase, numPages, true,
			TranslationTable::kRwRo, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0))
	{
		*pPool = create_mspace_with_base(pBase, numPages << 12, 0);

		if (pPool == &s_pool)
			s_poolInit = true;
		return true;
	}
	else
		return false;
}

void *operator new( size_t stAllocateBlock ) {
//   static int fInOpNew = 0;   // Guard flag.
//
//   if ( fLogMemory && !fInOpNew ) {
//      fInOpNew = 1;
//      clog << "Memory block " << ++cBlocksAllocated
//          << " allocated for " << stAllocateBlock
//          << " bytes\n";
//      fInOpNew = 0;
//   }
//   return malloc( stAllocateBlock );
//	ASSERT(0);
//	return 0;

#ifdef __ARM_ARCH_7A__
	//scope for problems here?
	bool e = IsIrqEnabled();
	EnableIrq(false);
#endif

	ASSERT(s_poolInit);
	void *r = mspace_malloc(s_pool, stAllocateBlock);

#ifdef __ARM_ARCH_7A__
	EnableIrq(e);
#endif
	return r;
}

void *operator new[]( size_t stAllocateBlock ) {
//   static int fInOpNew = 0;   // Guard flag.
//
//   if ( fLogMemory && !fInOpNew ) {
//      fInOpNew = 1;
//      clog << "Memory block " << ++cBlocksAllocated
//          << " allocated for " << stAllocateBlock
//          << " bytes\n";
//      fInOpNew = 0;
//   }
//   return malloc( stAllocateBlock );
//	ASSERT(0);
//	return 0;

#ifdef __ARM_ARCH_7A__
	bool e = IsIrqEnabled();
	EnableIrq(false);
#endif

	ASSERT(s_poolInit);
	void *r = mspace_malloc(s_pool, stAllocateBlock);

#ifdef __ARM_ARCH_7A__
	EnableIrq(e);
#endif
	return r;
}

// User-defined operator delete.
void operator delete( void *pvMem ) {
//   static int fInOpDelete = 0;   // Guard flag.
//   if ( fLogMemory && !fInOpDelete ) {
//      fInOpDelete = 1;
//      clog << "Memory block " << cBlocksAllocated--
//          << " deallocated\n";
//      fInOpDelete = 0;
//   }
//
//   free( pvMem );
//	ASSERT(0);

#ifdef __ARM_ARCH_7A__
	bool e = IsIrqEnabled();
	EnableIrq(false);
#endif

	ASSERT(s_poolInit);
	mspace_free(s_pool, pvMem);

#ifdef __ARM_ARCH_7A__
	EnableIrq(e);
#endif
}

extern "C" void __cxa_pure_virtual()
{
	ASSERT(0);
}

namespace std
{

void __throw_length_error(char const*)
{
	ASSERT(0);
}

void __throw_bad_alloc(void)
{
	ASSERT(0);
}

}

