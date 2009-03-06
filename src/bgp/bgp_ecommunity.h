/* BGP Extended Communities Attribute.
   Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>

This file is part of pmacct but mostly based on GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _BGP_ECOMMUNITY_H_
#define _BGP_ECOMMUNITY_H_

/* High-order octet of the Extended Communities type field.  */
#define ECOMMUNITY_ENCODE_AS                0x00
#define ECOMMUNITY_ENCODE_IP                0x01
#define ECOMMUNITY_ENCODE_AS4               0x02

/* Low-order octet of the Extended Communityes type field.  */
#define ECOMMUNITY_ROUTE_TARGET             0x02
#define ECOMMUNITY_SITE_ORIGIN              0x03

/* Extended communities attribute string format.  */
#define ECOMMUNITY_FORMAT_ROUTE_MAP            0
#define ECOMMUNITY_FORMAT_COMMUNITY_LIST       1
#define ECOMMUNITY_FORMAT_DISPLAY              2

/* Extended Communities value is eight octet long.  */
#define ECOMMUNITY_SIZE                        8

/* Extended Communities type flag.  */
#define ECOMMUNITY_FLAG_NON_TRANSITIVE      0x40  

/* Extended Communities attribute.  */
struct ecommunity
{
  /* Reference counter.  */
  unsigned long refcnt;

  /* Size of Extended Communities attribute.  */
  int size;

  /* Extended Communities value.  */
  u_int8_t *val;

  /* Human readable format string.  */
  char *str;
};

/* Extended community value is eight octet.  */
struct ecommunity_val
{
  char val[ECOMMUNITY_SIZE];
};

#define ecom_length(X)    ((X)->size * ECOMMUNITY_SIZE)

#if (!defined __BGP_ECOMMUNITY_C)
#define EXT extern
#else
#define EXT
#endif
EXT void ecommunity_init (void);
EXT void ecommunity_free (struct ecommunity *);
EXT struct ecommunity *ecommunity_new (void);
EXT struct ecommunity *ecommunity_parse (u_int8_t *, u_short);
EXT struct ecommunity *ecommunity_dup (struct ecommunity *);
EXT struct ecommunity *ecommunity_merge (struct ecommunity *, struct ecommunity *);
EXT struct ecommunity *ecommunity_intern (struct ecommunity *);
EXT int ecommunity_cmp (const void *, const void *);
EXT void ecommunity_unintern (struct ecommunity *);
EXT unsigned int ecommunity_hash_make (void *);
EXT struct ecommunity *ecommunity_str2com (const char *, int, int);
EXT char *ecommunity_ecom2str (struct ecommunity *, int);
EXT int ecommunity_match (const struct ecommunity *, const struct ecommunity *);
EXT char *ecommunity_str (struct ecommunity *);

#undef EXT
#endif
