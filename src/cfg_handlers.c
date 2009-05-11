/*
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2009 by Paolo Lucente
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

#define __CFG_HANDLERS_C

/* includes */
#include "pmacct.h"
#include "nfacctd.h"
#include "pmacct-data.h"
#include "plugin_hooks.h"
#include "cfg_handlers.h"

int parse_truefalse(char *value_ptr)
{
  int value;

  lower_string(value_ptr);
  
  if (!strcmp("true", value_ptr)) value = TRUE;
  else if (!strcmp("false", value_ptr)) value = FALSE;
  else value = ERR;

  return value;
}

int cfg_key_debug(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr); 
  if (value < 0) return ERR; 

  if (!name) for (; list; list = list->next, changes++) list->cfg.debug = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) { 
	list->cfg.debug = value;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_syslog(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.syslog = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.syslog = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_logfile(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.logfile = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'logfile'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pidfile(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.pidfile = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pidfile'. Globalized.\n", filename);

  return changes;
}

int cfg_key_daemonize(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.daemon = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'daemonize'. Globalized.\n", filename); 

  return changes;
}

int cfg_key_aggregate(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  char *count_token;
  u_int64_t value = 0;
  u_int32_t changes = 0; 

  trim_all_spaces(value_ptr);

  while (count_token = extract_token(&value_ptr, ',')) {
    if (!strcmp(count_token, "src_host")) value |= COUNT_SRC_HOST;
    else if (!strcmp(count_token, "dst_host")) value |= COUNT_DST_HOST;
    else if (!strcmp(count_token, "src_net")) value |= COUNT_SRC_NET;
    else if (!strcmp(count_token, "dst_net")) value |= COUNT_DST_NET;
    else if (!strcmp(count_token, "sum")) value |= COUNT_SUM_HOST;
    else if (!strcmp(count_token, "src_port")) value |= COUNT_SRC_PORT;
    else if (!strcmp(count_token, "dst_port")) value |= COUNT_DST_PORT;
    else if (!strcmp(count_token, "proto")) value |= COUNT_IP_PROTO;
#if defined (HAVE_L2)
    else if (!strcmp(count_token, "src_mac")) value |= COUNT_SRC_MAC;
    else if (!strcmp(count_token, "dst_mac")) value |= COUNT_DST_MAC;
    else if (!strcmp(count_token, "vlan")) value |= COUNT_VLAN;
    else if (!strcmp(count_token, "sum_mac")) value |= COUNT_SUM_MAC;
#endif
    else if (!strcmp(count_token, "tos")) value |= COUNT_IP_TOS;
    else if (!strcmp(count_token, "none")) value |= COUNT_NONE;
    else if (!strcmp(count_token, "src_as")) value |= COUNT_SRC_AS;
    else if (!strcmp(count_token, "dst_as")) value |= COUNT_DST_AS;
    else if (!strcmp(count_token, "sum_host")) value |= COUNT_SUM_HOST;
    else if (!strcmp(count_token, "sum_net")) value |= COUNT_SUM_NET;
    else if (!strcmp(count_token, "sum_as")) value |= COUNT_SUM_AS;
    else if (!strcmp(count_token, "sum_port")) value |= COUNT_SUM_PORT;
    else if (!strcmp(count_token, "tag")) value |= COUNT_ID;
    else if (!strcmp(count_token, "flows")) value |= COUNT_FLOWS;
    else if (!strcmp(count_token, "class")) value |= COUNT_CLASS;
    else if (!strcmp(count_token, "tcpflags")) value |= COUNT_TCPFLAGS;
    else if (!strcmp(count_token, "src_std_comm")) value |= COUNT_SRC_STD_COMM;
    else if (!strcmp(count_token, "dst_std_comm")) value |= COUNT_DST_STD_COMM;
    else if (!strcmp(count_token, "sum_std_comm")) value |= COUNT_SUM_STD_COMM;
    else if (!strcmp(count_token, "src_ext_comm")) value |= COUNT_SRC_EXT_COMM;
    else if (!strcmp(count_token, "dst_ext_comm")) value |= COUNT_DST_EXT_COMM;
    else if (!strcmp(count_token, "sum_ext_comm")) value |= COUNT_SUM_EXT_COMM;
    else if (!strcmp(count_token, "as_path")) value |= COUNT_AS_PATH;
    else Log(LOG_WARNING, "WARN ( %s ): ignoring unknown aggregation method: %s.\n", filename, count_token);
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.what_to_count = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.what_to_count = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_snaplen(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < DEFAULT_SNAPLEN) {
    Log(LOG_WARNING, "WARN ( %s ): 'snaplen' has to be >= %d.\n", filename, DEFAULT_SNAPLEN);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.snaplen = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'snaplen'. Globalized.\n", filename);

  return changes;
}

int cfg_key_aggregate_filter(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) {
    Log(LOG_ERR, "ERROR ( %s ): aggregation filter cannot be global. Not loaded.\n", filename);
    changes++;
  }
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.a_filter = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_pre_tag_filter(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  char *count_token, *range_ptr;
  int value, range = 0, changes = 0;
  u_int8_t neg;

  if (!name) {
    Log(LOG_ERR, "ERROR ( %s ): ID filter cannot be global. Not loaded.\n", filename);
    changes++;
  }
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
	trim_all_spaces(value_ptr);

	list->cfg.ptf.num = 0;
	while ((count_token = extract_token(&value_ptr, ',')) && changes < MAX_MAP_ENTRIES/4) {
	  neg = pt_check_neg(&count_token);
	  range_ptr = pt_check_range(count_token); 
	  value = atoi(count_token);
	  if (range_ptr) range = atoi(range_ptr);
	  else range = value;

	  if ((value < 0) || (value > 65535)) {
	    Log(LOG_ERR, "WARN ( %s ): 'pre_tag_filter' values has to be in the range 0-65535. '%d' not loaded.\n", filename, value);
	    changes++;
	    break;
	  }

	  if (range_ptr) { 
	    if ((range < 1) || (range > 65535)) {
	      Log(LOG_ERR, "WARN ( %s ): Range values has to be in the range 1-65535. '%d' not loaded.\n", filename, range);
	      changes++;
	      break;
	    }
	    if (range <= value) {
	      Log(LOG_ERR, "WARN ( %s ): Range value is expected in the format low-high. '%d-%d' not loaded.\n", filename, value, range);
	      changes++;
	      break;
	    }
	  }

          list->cfg.ptf.table[list->cfg.ptf.num].neg = neg;
          list->cfg.ptf.table[list->cfg.ptf.num].n = value;
          list->cfg.ptf.table[list->cfg.ptf.num].r = range;
	  list->cfg.ptf.num++;
          changes++;
	}
        break;
      }
    }
  }

  return changes;
}


int cfg_key_pcap_filter(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.clbuf = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pcap_filter'. Globalized.\n", filename);

  return changes;
}

int cfg_key_interface(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.dev = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'interface'. Globalized.\n", filename);

  return changes;
}

int cfg_key_interface_wait(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.if_wait = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'interface_wait'. Globalized.\n", filename);

  return changes;
}

int cfg_key_savefile_wait(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.sf_wait = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'savefile_wait'. Globalized.\n", filename);

  return changes;
}

int cfg_key_promisc(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.promisc = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'promisc'. Globalized.\n", filename);

  return changes;
}

int cfg_key_imt_path(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.imt_plugin_path = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.imt_plugin_path = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_imt_passwd(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.imt_plugin_passwd = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.imt_plugin_passwd = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_imt_buckets(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'imt_buckets' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.buckets = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.buckets = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_imt_mem_pools_number(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;
  
  value = atoi(value_ptr);
  if (value < 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'imt_mem_pools_number' has to be >= 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.num_memory_pools = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.num_memory_pools = value;
        changes++;
        break;
      }
    }
  }

  have_num_memory_pools = TRUE;
  return changes;
}

int cfg_key_imt_mem_pools_size(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  /* legal values should be >= sizeof(struct acc), though we are unable to check
     this condition here. Thus, this function will just cut clearly wrong values
     ie. < = 0. Strict checks will be accomplished later, by the memory plugin */ 
  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'imt_mem_pools_size' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.memory_pool_size = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.memory_pool_size = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_db(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_db = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_db = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_table(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  /* validations: we allow only a) certain variable names, b) a maximum of 8 variables
     and c) a maximum table name length of 64 chars */ 
  {
    int num = 0;
    char *c, *ptr = value_ptr;

    while (c = strchr(ptr, '%')) {
      c++;
      ptr = c;
      switch (*c) {
      case 'd':
	num++;
	break;
      case 'H':
	num++;
	break;
      case 'm':
	num++;
	break;
      case 'M':
	num++;
	break;
      case 'w':
	num++;
	break;
      case 'W':
	num++;
	break;
      case 'Y':
	num++;
	break;
      case 's':
	num++;
	break;
      default:
	Log(LOG_ERR, "ERROR ( %s ): sql_table, %%%c not supported.\n", filename, *c);
	exit(1);
	break;
      } 
    } 

    if (num > 8) {
      Log(LOG_ERR, "ERROR ( %s ): sql_table, exceeded the maximum allowed variables (8) into the table name.\n", filename);
      exit(1);
    }
  }

  if (strlen(value_ptr) > 64) {
    Log(LOG_ERR, "ERROR ( %s ): sql_table, exceeded the maximum SQL table name length (64).\n", filename);
    exit(1);
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_table = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_table = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_table_schema(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_table_schema = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_table_schema = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_table_version(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "ERROR ( %s ): invalid 'sql_table_version' value.\n", filename);
    exit(1);
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_table_version = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_table_version = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_table_type(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_table_type = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_table_type = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_data(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_data = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_data = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_host(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_host = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_host = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_recovery_backup_host(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_backup_host = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_backup_host = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_max_writers(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 1 || value >= 100) {
    Log(LOG_WARNING, "WARN ( %s ): invalid 'sql_max_writers' value). Allowed values are: 1 <= sql_max_writers < 100.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_max_writers = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_max_writers = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_trigger_exec(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_trigger_exec = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_trigger_exec = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_trigger_time(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0, t, t_howmany;

  parse_time(filename, value_ptr, &t, &t_howmany);

  if (!name) {
    for (; list; list = list->next, changes++) {
      list->cfg.sql_trigger_time = t;
      list->cfg.sql_trigger_time_howmany = t_howmany;
    }
  }
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_trigger_time = t;
	list->cfg.sql_trigger_time_howmany = t_howmany;
	changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_user(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_user = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_user = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_passwd(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_passwd = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_passwd = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_refresh_time(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0, i, len = strlen(value_ptr);

  for (i = 0; i < len; i++) {
    if (!isdigit(value_ptr[i]) && !isspace(value_ptr[i])) {
      Log(LOG_ERR, "WARN ( %s ): 'sql_refresh_time' is expected in secs but contains non-digit chars: '%c'\n", filename, value_ptr[i]);
      return ERR;
    }
  }

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'sql_refresh_time' has to be > 0.\n", filename);
    return ERR;
  }
     
  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_refresh_time = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_refresh_time = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_startup_delay(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'sql_startup_delay' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_startup_delay = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_startup_delay = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_optimize_clauses(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_optimize_clauses = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_optimize_clauses = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_history_roundoff(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;
  int i, check, len;

  len = strlen(value_ptr);
  for (i = 0, check = 0; i < len; i++) {
    if (value_ptr[i] == 'd') check |= COUNT_DAILY;
    if (value_ptr[i] == 'w') check |= COUNT_WEEKLY;
    if (value_ptr[i] == 'M') check |= COUNT_MONTHLY;
  } 
  if (((check & COUNT_DAILY) || (check & COUNT_MONTHLY)) && (check & COUNT_WEEKLY)) {
    Log(LOG_ERR, "WARN ( %s ): 'sql_history_roundoff' 'w' is not compatible with either 'd' or 'M'.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_history_roundoff = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_history_roundoff = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_recovery_logfile(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_recovery_logfile = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_recovery_logfile = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_history(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0, sql_history, sql_history_howmany;

  parse_time(filename, value_ptr, &sql_history, &sql_history_howmany);

  if (!name) {
    for (; list; list = list->next, changes++) {
      list->cfg.sql_history = sql_history;
      list->cfg.sql_history_howmany = sql_history_howmany;
    }
  }
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_history = sql_history;
        list->cfg.sql_history_howmany = sql_history_howmany;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_history_since_epoch(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_history_since_epoch = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_history_since_epoch = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_cache_entries(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'sql_cache_entries' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_cache_entries = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_cache_entries = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_dont_try_update(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_dont_try_update = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_dont_try_update = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_preprocess(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_preprocess = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_preprocess = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_preprocess_type(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0, value = 0;

  if (!strncmp(value_ptr, "any", 3)) value = FALSE;
  if (!strncmp(value_ptr, "all", 3)) value = TRUE;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_preprocess_type = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
	list->cfg.sql_preprocess_type = value;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_multi_values(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0, value = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'sql_multi_values' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_multi_values = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_multi_values = value; 
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_aggressive_classification(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_aggressive_classification = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_aggressive_classification = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_locking_style(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_locking_style = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
	list->cfg.sql_locking_style = value_ptr;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_sql_use_copy(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sql_use_copy = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sql_use_copy = value;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_plugin_pipe_size(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  /* legal values should be >= sizeof(struct pkt_data)+sizeof(struct ch_buf_hdr)
     though we are unable to check this condition here. Thus, this function will
     just cut clearly wrong values ie. < = 0. Strict checks will be accomplished
     later, by the load_plugins() */ 
  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'plugin_pipe_size' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.pipe_size = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.pipe_size = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_plugin_buffer_size(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  /* legal values should be >= sizeof(struct pkt_data) and < plugin_pipe_size
     value, if any though we are unable to check this condition here. Thus, this
     function will just cut clearly wrong values ie. < = 0. Strict checks will 
     be accomplished later, by the load_plugins() */
  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'plugin_buffer_size' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.buffer_size = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.buffer_size = value; 
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_networks_mask(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'networks_mask' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.networks_mask = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.networks_mask = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_networks_file(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.networks_file = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.networks_file = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_networks_cache_entries(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'networks_cache_entries' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.networks_cache_entries = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.networks_cache_entries = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_ports_file(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.ports_file = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.ports_file = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_refresh_maps(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.refresh_maps = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'refresh_maps'. Globalized.\n", filename);

  return changes;
}

int cfg_key_print_refresh_time(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'print_refresh_time' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.print_refresh_time = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.print_refresh_time = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_print_cache_entries(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'print_cache_entries' has to be > 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.print_cache_entries = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.print_cache_entries = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_print_markers(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.print_markers = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.print_markers = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_post_tag(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if ((value < 1) || (value > 65535)) {
    Log(LOG_ERR, "WARN ( %s ): 'post_tag' has to be in the range 1-65535.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.post_tag = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.post_tag = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sampling_rate(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 1) {
    Log(LOG_ERR, "WARN ( %s ): 'sampling_rate' has to be >= 1.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sampling_rate = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sampling_rate = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_nfacctd_port(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if ((value <= 0) || (value > 65535)) {
    Log(LOG_ERR, "WARN ( %s ): 'nfacctd_port' has to be in the range 0-65535.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.nfacctd_port = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_port'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_ip(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_ip = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_ip'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_allow_file(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_allow_file = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_allow_file'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pre_tag_map(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.pre_tag_map = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pre_tag_map'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pre_tag_map_entries(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'pre_tag_map_entries' has to be > 0.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.pre_tag_map_entries = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pre_tag_map_entries'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_time_secs(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_time = NF_TIME_SECS;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_time_secs'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_time_new(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_time = NF_TIME_NEW;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_time_new'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_mcast_groups(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  struct host_addr tmp_addr;
  char *count_token;
  u_int32_t value = 0, changes = 0; 
  u_int8_t idx = 0, more = 0, mcast_family; 

  trim_all_spaces(value_ptr);
  memset(mcast_groups, 0, sizeof(mcast_groups));

  while (count_token = extract_token(&value_ptr, ',')) {
    memset(&tmp_addr, 0, sizeof(tmp_addr));
    str_to_addr(count_token, &tmp_addr);
    if (is_multicast(&tmp_addr)) {
      if (idx < MAX_MCAST_GROUPS) {
        memcpy(&mcast_groups[idx], &tmp_addr, sizeof(tmp_addr));
        idx++;
      }
      else more++;
    } 
  }

  for (; list; list = list->next, changes++); /* Nothing to do because of the global array, just rolling changes counters */
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for keys '[nfacctd|sfacctd]_mcast_groups'. Globalized.\n",
		  filename);
  if (more) Log(LOG_WARNING, "WARN ( %s ): Only the first %u (on a total of %u) multicast groups will be joined.\n",
		  filename, MAX_MCAST_GROUPS, MAX_MCAST_GROUPS+more);

  return changes;
}

int cfg_key_nfacctd_sql_log(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfacctd_sql_log = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.nfacctd_sql_log = value;
        changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_nfacctd_bgp(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp = TRUE;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_bgp_msglog(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp_msglog = TRUE;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp_msglog'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_bgp_myas(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 1) {
	Log(LOG_ERR, "WARN ( %s ): 'nfacctd_bgp_myas' has to be >= 1.\n", filename);
	return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp_myas = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp_myas'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_bgp_aspath_radius(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 1) {
        Log(LOG_ERR, "WARN ( %s ): 'nfacctd_bgp_aspath_radius' has to be >= 1.\n", filename);
        return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp_aspath_radius = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp_aspath_radius'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_bgp_stdcomm_pattern(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp_stdcomm_pattern = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp_stdcomm_pattern'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_bgp_extcomm_pattern(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp_extcomm_pattern = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp_extcomm_pattern'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_max_bgp_peers(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 1) {
        Log(LOG_ERR, "WARN ( %s ): 'nfacctd_max_bgp_peers' has to be >= 1.\n", filename);
        return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.nfacctd_max_bgp_peers = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_max_bgp_peers'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_bgp_ip(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_bgp_ip = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_bgp_ip'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pmacctd_force_frag_handling(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.handle_fragments = TRUE;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pmacctd_force_frag_handling'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pmacctd_frag_buffer_size(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'pmacctd_frag_buffer_size' has to be > 0.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.frag_bufsz = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pmacctd_frag_buffer_size'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pmacctd_flow_buffer_size(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'pmacctd_flow_buffer_size' has to be > 0.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.flow_bufsz = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pmacctd_flow_buffer_size'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pmacctd_flow_buffer_buckets(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_WARNING, "WARN ( %s ): 'flow_buffer_buckets' has to be > 0.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.flow_hashsz = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pmacctd_flow_buffer_buckets'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pmacctd_conntrack_buffer_size(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'pmacctd_conntrack_buffer_size' has to be > 0.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.conntrack_bufsz = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pmacctd_conntrack_buffer_size'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pmacctd_flow_lifetime(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_ERR, "WARN ( %s ): 'pmacctd_flow_lifetime' has to be > 0.\n", filename);
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.flow_lifetime = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pmacctd_flow_lifetime'. Globalized.\n", filename);

  return changes;
}

int cfg_key_sfacctd_renormalize(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.sfacctd_renormalize = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'sfacctd_renormalize'. Globalized.\n", filename);

  return changes;
}

int cfg_key_pcap_savefile(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.pcap_savefile = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'pcap_savefile'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_as_new(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_as_str = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'nfacctd_as_new'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfacctd_disable_checks(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = parse_truefalse(value_ptr);
  if (value < 0) return ERR;

  for (; list; list = list->next, changes++) list->cfg.nfacctd_disable_checks = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key '[ns]facctd_disable_checks'. Globalized.\n", filename);

  return changes;
}


int cfg_key_classifiers(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  for (; list; list = list->next, changes++) list->cfg.classifiers_path = value_ptr;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'classifiers'. Globalized.\n", filename);

  return changes;
}

int cfg_key_classifier_tentatives(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_INFO, "INFO ( %s ): 'classifier_tentatives' has to be >= 1.\n", filename);  
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.classifier_tentatives = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'classifier_tentatives'. Globalized.\n", filename);

  return changes;
}

int cfg_key_classifier_table_num(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value <= 0) {
    Log(LOG_INFO, "INFO ( %s ): 'classifier_table_num' has to be >= 1.\n", filename);  
    return ERR;
  }

  for (; list; list = list->next, changes++) list->cfg.classifier_table_num = value;
  if (name) Log(LOG_WARNING, "WARN ( %s ): plugin name not supported for key 'classifier_table_num'. Globalized.\n", filename);

  return changes;
}

int cfg_key_nfprobe_timeouts(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfprobe_timeouts = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
	list->cfg.nfprobe_timeouts = value_ptr;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_nfprobe_hoplimit(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if ((value < 1) || (value > 255)) {
    Log(LOG_ERR, "WARN ( %s ): 'nfprobe_hoplimit' has to be in the range 1-255.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfprobe_hoplimit = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.nfprobe_hoplimit = value;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_nfprobe_maxflows(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 1) {
    Log(LOG_ERR, "WARN ( %s ): 'nfprobe_maxflows' has to be >= 1.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfprobe_maxflows = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
	list->cfg.nfprobe_maxflows = value;
	changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_nfprobe_receiver(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfprobe_receiver = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.nfprobe_receiver = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_nfprobe_version(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value != 1 && value != 5 && value != 9) {
    Log(LOG_ERR, "WARN ( %s ): 'nfprobe_version' has to be either 1/5/9.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfprobe_version = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.nfprobe_version = value;
        changes++;
	break;
      }
    }
  }

  return changes;
}

int cfg_key_nfprobe_engine(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.nfprobe_engine = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.nfprobe_engine = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sfprobe_receiver(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sfprobe_receiver = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sfprobe_receiver = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sfprobe_agentip(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;

  if (!name) for (; list; list = list->next, changes++) list->cfg.sfprobe_agentip = value_ptr;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sfprobe_agentip = value_ptr;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_sfprobe_agentsubid(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int value, changes = 0;

  value = atoi(value_ptr);
  if (value < 0) {
    Log(LOG_ERR, "WARN ( %s ): 'sfprobe_agentsubid' has to be >= 0.\n", filename);
    return ERR;
  }

  if (!name) for (; list; list = list->next, changes++) list->cfg.sfprobe_agentsubid = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.sfprobe_agentsubid = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}

int cfg_key_flow_handling_threads(char *filename, char *name, char *value_ptr)
{
  struct plugins_list_entry *list = plugins_list;
  int changes = 0;
  int value;

  value = atoi(value_ptr);
  if (!name) for (; list; list = list->next, changes++) list->cfg.flow_handling_threads = value;
  else {
    for (; list; list = list->next) {
      if (!strcmp(name, list->name)) {
        list->cfg.flow_handling_threads = value;
        changes++;
        break;
      }
    }
  }

  return changes;
}



void parse_time(char *filename, char *value, int *mu, int *howmany)
{
  int k, j, len;

  len = strlen(value);
  for (j = 0; j < len; j++) {
    if (!isdigit(value[j])) {
      if (value[j] == 'm') *mu = COUNT_MINUTELY;
      else if (value[j] == 'h') *mu = COUNT_HOURLY;
      else if (value[j] == 'd') *mu = COUNT_DAILY;
      else if (value[j] == 'w') *mu = COUNT_WEEKLY;
      else if (value[j] == 'M') *mu = COUNT_MONTHLY;
      else {
        Log(LOG_WARNING, "WARN ( %s ): Ignoring unknown time measuring unit: '%c'.\n", filename, value[j]);
        *mu = 0;
        *howmany = 0;
        return;
      }
      if (*mu) {
        value[j] = '\0';
        break;
      }
    }
  }
  k = atoi(value);
  if (k > 0) *howmany = k;
  else {
    Log(LOG_WARNING, "WARN ( %s ): ignoring invalid time value: %d\n", filename, k);
    *mu = 0;
    *howmany = 0;
  }
}

