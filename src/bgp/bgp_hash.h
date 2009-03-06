/* Hash routine.
   Copyright (C) 1998 Kunihiro Ishiguro

This file is part of pmacct but mostly based on GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2, or (at your
option) any later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef _BGP_HASH_H_
#define _BGP_HASH_H_

/* Default hash table size.  */ 
#define HASHTABSIZE     1024

struct hash_backet
{
  /* Linked list.  */
  struct hash_backet *next;

  /* Hash key. */
  unsigned int key;

  /* Data.  */
  void *data;
};

struct hash
{
  /* Hash backet. */
  struct hash_backet **index;

  /* Hash table size. */
  unsigned int size;

  /* Key make function. */
  unsigned int (*hash_key) (void *);

  /* Data compare function. */
  int (*hash_cmp) (const void *, const void *);

  /* Backet alloc. */
  unsigned long count;
};

#if (!defined __BGP_HASH_C)
#define EXT extern
#else
#define EXT
#endif
EXT struct hash *hash_create (unsigned int (*) (void *), int (*) (const void *, const void *));
EXT struct hash *hash_create_size (unsigned int, unsigned int (*) (void *), int (*) (const void *, const void *));
EXT void *hash_get (struct hash *, void *, void * (*) (void *));
EXT void *hash_alloc_intern (void *);
EXT void *hash_lookup (struct hash *, void *);
EXT void *hash_release (struct hash *, void *);
EXT void hash_iterate (struct hash *, void (*) (struct hash_backet *, void *), void *);
EXT void hash_clean (struct hash *, void (*) (void *));
EXT void hash_free (struct hash *);

#undef EXT
#endif
