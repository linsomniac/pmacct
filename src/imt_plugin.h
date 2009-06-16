/*
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2008 by Paolo Lucente
*/

/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <sys/poll.h>

/* defines */
#define NUM_MEMORY_POOLS 16
#define MEMORY_POOL_SIZE 8192
#define MAX_HOSTS 32771 
#define MAX_QUERIES 4096
#define MAX_PROTOCOL_LEN 16 /* same as classifier.h */

/* Structures */
struct acc {
  struct pkt_primitives primitives;
  pm_counter_t bytes_counter;
  pm_counter_t packet_counter;
  pm_counter_t flow_counter;
  u_int32_t tcp_flags; 
  unsigned int signature;
  u_int8_t reset_flag;
  struct timeval rstamp;	/* classifiers: reset timestamp */
  struct pkt_bgp_primitives *pbgp;
  struct acc *next;
};

struct bucket_desc {
  unsigned int num;
  unsigned short int howmany;
};

struct memory_pool_desc {
  int id;
  unsigned char *base_ptr;
  unsigned char *ptr;
  int space_left;
  int len;
  struct memory_pool_desc *next;
};

struct query_header {
  int type;			/* type of query */
  u_int64_t what_to_count;	/* aggregation */
  unsigned int num;		/* number of queries */
  unsigned int ip_sz;		/* IP addresses size (in bytes) */
  unsigned int cnt_sz;		/* counters size (in bytes) */
  char passwd[12];		/* OBSOLETED: password */
};

struct query_entry {
  u_int64_t what_to_count;	/* aggregation */
  struct pkt_primitives data;	/* actual data */
  struct pkt_bgp_primitives pbgp; /* extended BGP data */
};

struct reply_buffer {
  unsigned char buf[LARGEBUFLEN];
  unsigned char *ptr;
  int len;
  int packed; 
};

struct stripped_class {
  pm_class_t id;
  char protocol[MAX_PROTOCOL_LEN];
};

/* prototypes */
#if (!defined __ACCT_C)
#define EXT extern
#else
#define EXT
#endif
EXT void insert_accounting_structure(struct pkt_data *, struct pkt_bgp_primitives *);
EXT struct acc *search_accounting_structure(struct pkt_primitives *, struct pkt_bgp_primitives *);
EXT int compare_accounting_structure(struct acc *, struct pkt_primitives *, struct pkt_bgp_primitives *);
#undef EXT

#if (!defined __MEMORY_C)
#define EXT extern
#else
#define EXT
#endif
EXT void init_memory_pool_table();
EXT void clear_memory_pool_table();
EXT struct memory_pool_desc *request_memory_pool(int);
#undef EXT

#if (!defined __SERVER_C)
#define EXT extern
#else
#define EXT
#endif
EXT void set_reset_flag(struct acc *);
EXT void reset_counters(struct acc *);
EXT int build_query_server(char *);
EXT void process_query_data(int, unsigned char *, int, int);
EXT void mask_elem(struct pkt_primitives *, struct pkt_bgp_primitives *, struct acc *, u_int64_t);
EXT void enQueue_elem(int, struct reply_buffer *, void *, int, int);
EXT void Accumulate_Counters(struct pkt_data *, struct acc *);
#undef EXT

#if (!defined __IMT_PLUGIN_C)
#define EXT extern
#else
#define EXT
#endif
EXT void sum_host_insert(struct pkt_data *, struct pkt_bgp_primitives *);
EXT void sum_port_insert(struct pkt_data *, struct pkt_bgp_primitives *);
EXT void sum_as_insert(struct pkt_data *, struct pkt_bgp_primitives *);
#if defined HAVE_L2
EXT void sum_mac_insert(struct pkt_data *, struct pkt_bgp_primitives *);
#endif
EXT void exit_now(int);
EXT void free_bgp_allocs();
#undef EXT

/* global vars */
#if (!defined __IMT_PLUGIN_C && !defined __PMACCT_CLIENT_C)
#define EXT extern
#else
#define EXT
#endif
EXT void (*insert_func)(struct pkt_data *, struct pkt_bgp_primitives *); /* pointer to INSERT function */
EXT unsigned char *mpd;  /* memory pool descriptors table */
EXT unsigned char *a;  /* accounting in-memory table */
EXT struct memory_pool_desc *current_pool; /* pointer to currently used memory pool */
EXT struct acc **lru_elem_ptr; /* pointer to Last Recently Used (lru) element in a bucket */
EXT int no_more_space;
EXT struct timeval cycle_stamp; /* timestamp for the current cycle */
EXT struct timeval table_reset_stamp; /* global table reset timestamp */
#undef EXT
