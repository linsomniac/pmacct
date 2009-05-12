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

#define __SQL_COMMON_M_C

Inline void AddToLRUTail(struct db_cache *Cursor)
{
  if (Cursor == lru_tail) return;

  if (Cursor->lru_prev) Cursor->lru_prev->lru_next = Cursor->lru_next;
  if (Cursor->lru_next) Cursor->lru_next->lru_prev = Cursor->lru_prev;
  Cursor->lru_prev = lru_tail;
  Cursor->lru_prev->lru_next = Cursor;
  Cursor->lru_next = NULL;
  lru_tail = Cursor;
}

Inline void RetireElem(struct db_cache *Cursor)
{
  Cursor->lru_prev->lru_next = Cursor->lru_next;
  if (Cursor->lru_next) Cursor->lru_next->lru_prev = Cursor->lru_prev;
  if (Cursor == lru_tail) lru_tail = &lru_head; 

  if (Cursor->next) {
    Cursor->prev->next = Cursor->next;
    Cursor->next->prev = Cursor->prev;
  }
  else Cursor->prev->next = NULL;

  if (Cursor->pbgp) free(Cursor->pbgp);
  free(Cursor);
}

Inline void BuildChain(struct db_cache *Cursor, struct db_cache *newElem)
{
  Cursor->next = newElem;
  newElem->prev = Cursor;
  newElem->chained = TRUE;
}

Inline void ReBuildChain(struct db_cache *Cursor, struct db_cache *newElem)
{
  if (newElem->next) {
    newElem->prev->next = newElem->next;
    newElem->next->prev = newElem->prev;
  }
  else newElem->prev->next = NULL;

  Cursor->next = newElem;
  newElem->prev = Cursor;
  newElem->next = NULL;
}

Inline void SwapChainedElems(struct db_cache *Cursor, struct db_cache *staleElem)
{
  struct db_cache *auxPtr;

  auxPtr = Cursor->prev;
  Cursor->prev = staleElem->prev;
  Cursor->prev->next = Cursor;
  if (auxPtr == staleElem) staleElem->prev = Cursor;
  else {
    staleElem->prev = auxPtr;
    staleElem->prev->next = staleElem;
  }

  auxPtr = Cursor->next;
  if (staleElem->next == Cursor) Cursor->next = staleElem;
  else {
    Cursor->next = staleElem->next;
    Cursor->next->prev = Cursor;
  }
  staleElem->next = auxPtr;
  if (auxPtr) staleElem->next->prev = staleElem;
}

Inline void SQL_SetENV()
{
  u_char *ptrs[16];
  int count = 0, i;

  INIT_BUF(envbuf);
  memset(ptrs, 0, sizeof(ptrs));

  if (config.sql_db) {
    strncat(envbuf.ptr, "SQL_DB=", envbuf.end-envbuf.ptr);
    strncat(envbuf.ptr, config.sql_db, envbuf.end-envbuf.ptr);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++; 
  }

  if (config.sql_table) {
    strncat(envbuf.ptr, "SQL_TABLE=", envbuf.end-envbuf.ptr);
    strncat(envbuf.ptr, config.sql_table, envbuf.end-envbuf.ptr);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (config.sql_host) {
    strncat(envbuf.ptr, "SQL_HOST=", envbuf.end-envbuf.ptr);
    strncat(envbuf.ptr, config.sql_host, envbuf.end-envbuf.ptr);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (config.sql_user) {
    strncat(envbuf.ptr, "SQL_USER=", envbuf.end-envbuf.ptr);
    strncat(envbuf.ptr, config.sql_user, envbuf.end-envbuf.ptr);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "SQL_REFRESH_TIME=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", config.sql_refresh_time);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (config.sampling_rate >= 1) {
    u_char *tmpptr;

    strncat(envbuf.ptr, "SAMPLING_RATE=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", config.sampling_rate);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (config.sql_recovery_logfile) {
    strncat(envbuf.ptr, "SQL_RECOVERY_LOGFILE=", envbuf.end-envbuf.ptr);
    strncat(envbuf.ptr, config.sql_recovery_logfile, envbuf.end-envbuf.ptr);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (config.sql_backup_host) {
    strncat(envbuf.ptr, "SQL_RECOVERY_BACKUP_HOST=", envbuf.end-envbuf.ptr);
    strncat(envbuf.ptr, config.sql_backup_host, envbuf.end-envbuf.ptr);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "SQL_MAX_WRITERS=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", config.sql_max_writers);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  for (i = 0; i < count; i++)
    putenv(ptrs[i]);
}

Inline void SQL_SetENV_child(const struct insert_data *idata)
{
  u_char *ptrs[N_FUNCS];
  int count = 0, i;

  memset(ptrs, 0, sizeof(ptrs));

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "INSERT_QUERIES_NUMBER=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", idata->iqn);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "UPDATE_QUERIES_NUMBER=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", idata->uqn);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "ELAPSED_TIME=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%u", idata->elap_time);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "TOTAL_ELEM_NUMBER=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", idata->ten);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (idata->een) {
    u_char *tmpptr;

    strncat(envbuf.ptr, "EFFECTIVE_ELEM_NUMBER=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%u", idata->een);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (idata->basetime) {
    u_char *tmpptr;

    strncat(envbuf.ptr, "SQL_HISTORY_BASETIME=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%u", idata->basetime);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (idata->timeslot) {
    u_char *tmpptr;

    strncat(envbuf.ptr, "SQL_HISTORY_TIMESLOT=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%u", idata->timeslot);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  if (idata->dyn_table) {
    u_char *tmpptr;
    struct tm *nowtm;

    nowtm = localtime(&idata->basetime);
    strncat(envbuf.ptr, "EFFECTIVE_SQL_TABLE=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    strftime(tmpptr, envbuf.end-tmpptr, config.sql_table, nowtm); 
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  {
    u_char *tmpptr;

    strncat(envbuf.ptr, "SQL_ACTIVE_WRITERS=", envbuf.end-envbuf.ptr);
    tmpptr = envbuf.ptr + strlen(envbuf.ptr);
    snprintf(tmpptr, envbuf.end-tmpptr, "%d", sql_writers.active);
    ptrs[count] = envbuf.ptr;
    envbuf.ptr += strlen(envbuf.ptr)+1;
    count++;
  }

  for (i = 0; i < count; i++)
    putenv(ptrs[i]);
}

#undef __SQL_COMMON_M_C
