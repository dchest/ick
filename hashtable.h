#ifndef __HASHTABLE_ICK_H_
#define __HASHTABLE_ICK_H_

struct entry
{
    void *k, *v;
    unsigned int h;
    struct entry *next;
};

struct hashtable {
    unsigned int tablelength;
    struct entry **table;
    unsigned int entrycount;
    unsigned int loadlimit;
    unsigned int (*hashfn) (void *k);
    int (*eqfn) (void *k1, void *k2);
};


/* Public */
struct hashtable *create_hashtable(unsigned int minsize,
                 unsigned int (*hashfunction) (void*),
                 int (*key_eq_fn) (void*,void*));
struct hashtable *create_hashtable_m(unsigned int minsize);
int hashtable_insert(struct hashtable *h, void *k, void *v);
void *hashtable_search(struct hashtable *h, void *k);
void *hashtable_remove(struct hashtable *h, void *k);
unsigned int hashtable_count(struct hashtable *h);
void hashtable_iter(struct hashtable *h, void (*iterf)(void *));
void hashtable_destroy(struct hashtable *h, int free_values);

/* Private */
unsigned int hash(struct hashtable *h, void *k);

static inline unsigned int
indexFor(unsigned int tablelength, unsigned int hashvalue) {
    return (hashvalue % tablelength);
};

#define freekey(X) free(X)


#endif /* __HASHTABLE_ICK_H_ */

/*
 * Copyright (c) 2002, Christopher Clark
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
