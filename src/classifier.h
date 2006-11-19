#include <dirent.h>
#include "regexp.h"
#include "conntrack.h"

/* defines */
#define MAX_FN_LEN 256
#define MAX_SUBDIRS 128
#define MAX_PROTOCOL_LEN 16 
#define MAX_CLASSIFIERS 256
#define MAX_PATTERN_LEN 2048
#define DEFAULT_TENTATIVES 5 

/* data structures */
struct pkt_classifier_data {
  struct timeval stamp;
  u_char *packet_ptr;
  u_char *l3_ptr;
  u_char *l4_ptr;
  u_char *payload_ptr;
  u_int16_t l3_proto;
  u_int16_t l4_proto;
  u_int16_t plen;
  u_int8_t tentatives;
  u_int16_t sampling_rate;
};

struct pkt_classifier {
  pm_class_t id;
  char protocol[MAX_PROTOCOL_LEN];
  regexp *pattern;
  pm_class_t (*func)(struct pkt_classifier_data *, int, void **, void **, void **);
  conntrack_helper ct_helper;
  void *extra;
};

/* All but __CLASSIFIER_C are dummy entries. They are required to export locally
   the 'class' array. This is in order to avoid to link extra C files into nfacctd,
   sfacctd, pmmyplay and pmpgplay. */ 
#if defined __CLASSIFIER_C || defined __PMACCT_PLAYER_C || defined __NFACCTD_C || defined __SFACCTD_C
#define EXT
#else
#define EXT extern
#endif
/* prototypes */
EXT void init_classifiers(char *);
EXT void evaluate_classifiers(struct packet_ptrs *, struct ip_flow_common *, unsigned int);
EXT pm_class_t NF_evaluate_classifiers(char *);
EXT int parse_pattern_file(char *, struct pkt_classifier *);
EXT int parse_shared_object(char *, struct pkt_classifier *);
EXT int dot_pat(char *);
EXT int dot_so(char *);
EXT void init_class_accumulators(struct packet_ptrs *, struct ip_flow_common *, unsigned int);
EXT void handle_class_accumulators(struct packet_ptrs *, struct ip_flow_common *, unsigned int);
EXT int pm_scandir(const char *, struct dirent ***, int (*select)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **));
EXT int pm_alphasort(const struct dirent **, const struct dirent **);
EXT void link_conntrack_helper(struct pkt_classifier *);

EXT void *search_context_chain(struct ip_flow_common *, unsigned int, char *);
EXT void insert_context_chain(struct ip_flow_common *, unsigned int, char *, void *);
EXT void clear_context_chain(struct ip_flow_common *, unsigned int);
EXT void prepare_classifier_data(struct pkt_classifier_data *, struct ip_flow_common *, unsigned int, struct packet_ptrs *);

EXT pm_class_t pmct_register(struct pkt_classifier *);
EXT void pmct_unregister(pm_class_t);
EXT pm_class_t pmct_find_first_free();
EXT pm_class_t pmct_find_last_free();
EXT int pmct_isfree(pm_class_t);
EXT int pmct_get(pm_class_t, struct pkt_classifier *);
EXT int pmct_get_num_entries();

EXT struct pkt_classifier *class;
#undef EXT
