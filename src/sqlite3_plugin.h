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

/* includes */
#include <sqlite3.h>

/* prototypes */
void sqlite3_plugin(int, struct configuration *, void *);
int SQLI_cache_dbop(struct DBdesc *, struct db_cache *, struct insert_data *);
void SQLI_cache_purge(struct db_cache *[], int, struct insert_data *);
int SQLI_evaluate_history(int);
int SQLI_compose_static_queries();
void SQLI_Lock(struct DBdesc *);
void SQLI_Unlock(struct BE_descs *);
void SQLI_DB_Connect(struct DBdesc *, char *);
void SQLI_DB_Close(struct BE_descs *); 
void SQLI_create_dyn_table(struct DBdesc *, char *);
void SQLI_get_errmsg(struct DBdesc *);
void SQLI_create_backend(struct DBdesc *);
void SQLI_set_callbacks(struct sqlfunc_cb_registry *);
void SQLI_init_default_values(struct insert_data *);

/* variables */
static char sqlite3_db[] = "/tmp/pmacct.db";
static char sqlite3_table[] = "acct";
static char sqlite3_table_v2[] = "acct_v2";
static char sqlite3_table_v3[] = "acct_v3";
static char sqlite3_table_v4[] = "acct_v4";
static char sqlite3_table_v5[] = "acct_v5";
static char sqlite3_table_v6[] = "acct_v6";
static char sqlite3_table_v7[] = "acct_v7";
