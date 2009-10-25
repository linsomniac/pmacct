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

/* defines */
#define __NL_C

/* includes */
#include "pmacct.h"
#include "pmacct-data.h"
#include "pmacct-dlt.h"
#include "pretag_handlers.h"
#include "plugin_hooks.h"
#include "pkt_handlers.h"
#include "ip_frag.h"
#include "ip_flow.h"
#include "net_aggr.h"
#include "thread_pool.h"

void pcap_cb(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *buf)
{
  struct packet_ptrs pptrs;
  struct pcap_callback_data *cb_data = (struct pcap_callback_data *) user;
  struct pcap_device *device = cb_data->device;
  struct plugin_requests req;

  /* We process the packet with the appropriate
     data link layer function */
  if (buf) {
    pptrs.pkthdr = (struct pcap_pkthdr *) pkthdr;
    pptrs.packet_ptr = (u_char *) buf;
    pptrs.mac_ptr = 0; pptrs.vlan_ptr = 0; pptrs.mpls_ptr = 0;
    pptrs.pf = 0; pptrs.shadow = 0; pptrs.tag = 0; pptrs.tag2 = 0;
    pptrs.class = 0; pptrs.bpas = 0, pptrs.bta = 0;
    pptrs.f_agent = cb_data->f_agent;
    pptrs.idtable = cb_data->idt;
    pptrs.bpas_table = cb_data->bpas_table;
    pptrs.bta_table = cb_data->bta_table;

    (*device->data->handler)(pkthdr, &pptrs);
    if (pptrs.iph_ptr) {
      if ((*pptrs.l3_handler)(&pptrs)) {
        if (config.nfacctd_bgp) {
          PM_find_id((struct id_table *)pptrs.bta_table, &pptrs, &pptrs.bta, NULL);
          bgp_srcdst_lookup(&pptrs);
        }
        if (config.nfacctd_bgp_peer_as_src_map) PM_find_id((struct id_table *)pptrs.bpas_table, &pptrs, &pptrs.bpas, NULL);
        if (config.pre_tag_map) PM_find_id((struct id_table *)pptrs.idtable, &pptrs, &pptrs.tag, &pptrs.tag2);

        exec_plugins(&pptrs);
      }
    }
  }

  if (reload_map) {
    load_networks(config.networks_file, &nt, &nc);
    if (config.nfacctd_bgp && config.nfacctd_bgp_peer_as_src_map)
      load_id_file(MAP_BGP_PEER_AS_SRC, config.nfacctd_bgp_peer_as_src_map, (struct id_table *)cb_data->bpas_table, &req, &bpas_map_allocated);
    if (config.nfacctd_bgp)
      load_id_file(MAP_BGP_TO_XFLOW_AGENT, config.nfacctd_bgp_to_agent_map, (struct id_table *)cb_data->bta_table, &req, &bta_map_allocated);
    if (config.pre_tag_map)
      load_id_file(config.acct_type, config.pre_tag_map, (struct id_table *) pptrs.idtable, &req, &tag_map_allocated);
    reload_map = FALSE;
  }
}

int ip_handler(register struct packet_ptrs *pptrs)
{
  register u_int8_t len = 0;
  register u_int16_t caplen = ((struct pcap_pkthdr *)pptrs->pkthdr)->caplen;
  register unsigned char *ptr;
  register u_int16_t off = pptrs->iph_ptr-pptrs->packet_ptr, off_l4;
  int ret = TRUE;

  /* len: number of 32bit words forming the header */
  len = IP_HL(((struct my_iphdr *) pptrs->iph_ptr));
  len <<= 2;
  ptr = pptrs->iph_ptr+len;
  off += len;

  /* check len */
  if (off > caplen) return FALSE; /* IP packet truncated */
  pptrs->l4_proto = ((struct my_iphdr *)pptrs->iph_ptr)->ip_p;
  pptrs->payload_ptr = NULL;
  off_l4 = off;

  /* check fragments if needed */
  if (config.handle_fragments) {
    if (pptrs->l4_proto == IPPROTO_TCP || pptrs->l4_proto == IPPROTO_UDP) {
      if (off+MyTLHdrSz > caplen) {
        Log(LOG_INFO, "INFO ( default/core ): short IPv4 packet read (%u/%u/frags). Snaplen issue ?\n", caplen, off+MyTLHdrSz);
        return FALSE;
      }
      pptrs->tlh_ptr = ptr;

      if (((struct my_iphdr *)pptrs->iph_ptr)->ip_off & htons(IP_MF|IP_OFFMASK)) {
        ret = ip_fragment_handler(pptrs);
        if (!ret) {
          if (!config.ext_sampling_rate) goto quit;
          else {
            pptrs->tlh_ptr = dummy_tlhdr;
            pptrs->tcp_flags = FALSE;
            if (off < caplen) pptrs->payload_ptr = ptr;
            ret = TRUE;
            goto quit;
          }
        }
      }

      /* Let's handle both fragments and packets. If we are facing any subsequent frag
         our pointer is in place; we handle unknown L4 protocols likewise. In case of
         "entire" TCP/UDP packets we have to jump the L4 header instead */
      if (((struct my_iphdr *)pptrs->iph_ptr)->ip_off & htons(IP_OFFMASK));
      else if (pptrs->l4_proto == IPPROTO_UDP) {
        ptr += UDPHdrSz;
        off += UDPHdrSz;
      }
      else if (pptrs->l4_proto == IPPROTO_TCP) {
        ptr += ((struct my_tcphdr *)pptrs->tlh_ptr)->th_off << 2;
        off += ((struct my_tcphdr *)pptrs->tlh_ptr)->th_off << 2;
      }
      if (off < caplen) pptrs->payload_ptr = ptr;
    }
    else {
      pptrs->tlh_ptr = dummy_tlhdr;
      if (off < caplen) pptrs->payload_ptr = ptr;
    }

    if (config.handle_flows) {
      pptrs->tcp_flags = FALSE;

      if (pptrs->l4_proto == IPPROTO_TCP) {
        if (off_l4+TCPFlagOff+1 > caplen) {
          Log(LOG_INFO, "INFO ( default/core ): short IPv4 packet read (%u/%u/flows). Snaplen issue ?\n", caplen, off_l4+TCPFlagOff+1);
          return FALSE;
        }
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_SYN) pptrs->tcp_flags |= TH_SYN;
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_FIN) pptrs->tcp_flags |= TH_FIN;
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_RST) pptrs->tcp_flags |= TH_RST;
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_ACK && pptrs->tcp_flags) pptrs->tcp_flags |= TH_ACK;
      }

      ip_flow_handler(pptrs);
    }

    /* XXX: optimize/short circuit here! */
    pptrs->tcp_flags = FALSE;
    if (pptrs->l4_proto == IPPROTO_TCP && off_l4+TCPFlagOff+1 <= caplen)
      pptrs->tcp_flags = ((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags;
  }

  quit:
  return ret;
}

#if defined ENABLE_IPV6
int ip6_handler(register struct packet_ptrs *pptrs)
{
  struct ip6_frag *fhdr = NULL;
  register u_int16_t caplen = ((struct pcap_pkthdr *)pptrs->pkthdr)->caplen;
  u_int16_t len = 0, plen = ntohs(((struct ip6_hdr *)pptrs->iph_ptr)->ip6_plen);
  u_int16_t off = pptrs->iph_ptr-pptrs->packet_ptr, off_l4;
  u_int32_t advance;
  u_int8_t nh, fragmented = 0;
  u_char *ptr = pptrs->iph_ptr;
  int ret = TRUE;

  /* length checks */
  if (off+IP6HdrSz > caplen) return FALSE; /* IP packet truncated */
  if (plen == 0) {
    Log(LOG_INFO, "INFO ( default/core ): NULL IPv6 payload length. Jumbo packets are currently not supported.\n");
    return FALSE;
  }

  pptrs->l4_proto = 0;
  pptrs->payload_ptr = NULL;
  nh = ((struct ip6_hdr *)pptrs->iph_ptr)->ip6_nxt;
  advance = IP6HdrSz;

  while ((off+advance <= caplen) && advance) {
    off += advance;
    ptr += advance;

    switch(nh) {
    case IPPROTO_HOPOPTS:
    case IPPROTO_DSTOPTS:
    case IPPROTO_ROUTING:
    case IPPROTO_MOBILITY:
      nh = ((struct ip6_ext *)ptr)->ip6e_nxt;
      advance = (((struct ip6_ext *)ptr)->ip6e_len + 1) << 3;
      break;
    case IPPROTO_AH:
      nh = ((struct ip6_ext *)ptr)->ip6e_nxt;
      advance = sizeof(struct ah)+(((struct ah *)ptr)->ah_len << 2); /* hdr + sumlen */
      break;
    case IPPROTO_FRAGMENT:
      fhdr = (struct ip6_frag *) ptr;
      nh = ((struct ip6_ext *)ptr)->ip6e_nxt;
      advance = sizeof(struct ip6_frag);
      break;
    /* XXX: case IPPROTO_ESP: */
    /* XXX: case IPPROTO_IPCOMP: */
    default:
      pptrs->tlh_ptr = ptr;
      pptrs->l4_proto = nh;
      goto end;
    }
  }

  end:

  off_l4 = off;
  if (config.handle_fragments) {
    if (pptrs->l4_proto == IPPROTO_TCP || pptrs->l4_proto == IPPROTO_UDP) {
      if (off+MyTLHdrSz > caplen) {
        Log(LOG_INFO, "INFO ( default/core ): short IPv6 packet read (%u/%u/frags). Snaplen issue ?\n", caplen, off+MyTLHdrSz);
        return FALSE;
      }

      if (fhdr && (fhdr->ip6f_offlg & htons(IP6F_MORE_FRAG|IP6F_OFF_MASK))) {
        ret = ip6_fragment_handler(pptrs, fhdr);
        if (!ret) {
          if (!config.ext_sampling_rate) goto quit;
          else {
            pptrs->tlh_ptr = dummy_tlhdr;
            pptrs->tcp_flags = FALSE;
            if (off < caplen) pptrs->payload_ptr = ptr;
            ret = TRUE;
            goto quit;
          }
        }
      }

      /* Let's handle both fragments and packets. If we are facing any subsequent frag
         our pointer is in place; we handle unknown L4 protocols likewise. In case of
         "entire" TCP/UDP packets we have to jump the L4 header instead */
      if (fhdr && (fhdr->ip6f_offlg & htons(IP6F_OFF_MASK)));
      else if (pptrs->l4_proto == IPPROTO_UDP) {
        ptr += UDPHdrSz;
        off += UDPHdrSz;
      }
      else if (pptrs->l4_proto == IPPROTO_TCP) {
        ptr += ((struct my_tcphdr *)pptrs->tlh_ptr)->th_off << 2;
        off += ((struct my_tcphdr *)pptrs->tlh_ptr)->th_off << 2;
      }
      if (off < caplen) pptrs->payload_ptr = ptr;
    }
    else {
      pptrs->tlh_ptr = dummy_tlhdr;
      if (off < caplen) pptrs->payload_ptr = ptr;
    }

    if (config.handle_flows) {
      pptrs->tcp_flags = FALSE;

      if (pptrs->l4_proto == IPPROTO_TCP) {
        if (off_l4+TCPFlagOff+1 > caplen) {
          Log(LOG_INFO, "INFO ( default/core ): short IPv6 packet read (%u/%u/flows). Snaplen issue ?\n", caplen, off_l4+TCPFlagOff+1);
          return FALSE;
        }
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_SYN) pptrs->tcp_flags |= TH_SYN;
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_FIN) pptrs->tcp_flags |= TH_FIN;
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_RST) pptrs->tcp_flags |= TH_RST;
        if (((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags & TH_ACK && pptrs->tcp_flags) pptrs->tcp_flags |= TH_ACK;
      }

      ip_flow6_handler(pptrs);
    }

    /* XXX: optimize/short circuit here! */
    pptrs->tcp_flags = FALSE;
    if (pptrs->l4_proto == IPPROTO_TCP && off_l4+TCPFlagOff+1 <= caplen)
      pptrs->tcp_flags = ((struct my_tcphdr *)pptrs->tlh_ptr)->th_flags;
  }

  quit:
  return TRUE;
}
#endif

void PM_find_id(struct id_table *t, struct packet_ptrs *pptrs, pm_id_t *tag, pm_id_t *tag2)
{
  int x, j, stop;
  pm_id_t id;

  if (!t) return;

  id = 0;
  for (x = 0; x < t->ipv4_num; x++) {
    for (j = 0, stop = 0; !stop; j++) stop = (*t->e[x].func[j])(pptrs, &id, &t->e[x]);
    if (id) {
      if (stop == PRETAG_MAP_RCODE_ID) {
        if (t->e[x].stack.func) id = (*t->e[x].stack.func)(id, *tag);
        *tag = id;
      }
      else if (stop == PRETAG_MAP_RCODE_ID2) {
        if (t->e[x].stack.func) id = (*t->e[x].stack.func)(id, *tag2);
        *tag2 = id;
      }

      if (t->e[x].jeq.ptr) {
        if (t->e[x].ret) {
          exec_plugins(pptrs);
          set_shadow_status(pptrs);
          *tag = 0;
          *tag2 = 0;
        }
        x = t->e[x].jeq.ptr->pos;
        x--; /* yes, it will be automagically incremented by the for() cycle */
        id = 0;
      }
      else break;
    }
  }
}

void compute_once()
{
  struct pkt_data dummy;

  CounterSz = sizeof(dummy.pkt_len);
  PdataSz = sizeof(struct pkt_data);
  PpayloadSz = sizeof(struct pkt_payload);
  PextrasSz = sizeof(struct pkt_extras);
  PbgpSz = sizeof(struct pkt_bgp_primitives);
  ChBufHdrSz = sizeof(struct ch_buf_hdr);
  CharPtrSz = sizeof(char *);
  IP4HdrSz = sizeof(struct my_iphdr);
  MyTLHdrSz = sizeof(struct my_tlhdr);
  TCPFlagOff = 13;
  MyTCPHdrSz = TCPFlagOff+1;
  PptrsSz = sizeof(struct packet_ptrs);
  UDPHdrSz = 8;
  CSSz = sizeof(struct class_st);
  IpFlowCmnSz = sizeof(struct ip_flow_common);
  HostAddrSz = sizeof(struct host_addr);
#if defined ENABLE_IPV6
  IP6HdrSz = sizeof(struct ip6_hdr);
  IP6AddrSz = sizeof(struct in6_addr);
#endif
}
