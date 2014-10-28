/*
 * malloc.h
 *
 *  Created on: 1 Jun 2013
 *      Author: simon
 */

#ifndef MALLOC_H_
#define MALLOC_H_

extern "C"
{

typedef void* mspace;

mspace create_mspace_with_base(void* base, size_t capacity, int locked);
size_t destroy_mspace(mspace msp);
mspace create_mspace_with_base(void* base, size_t capacity, int locked);
void* mspace_malloc(mspace msp, size_t bytes);
void mspace_free(mspace msp, void* mem);

mspace GetUncachedPool(void);
}

#endif /* MALLOC_H_ */
