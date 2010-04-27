/*
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2010 by Paolo Lucente
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
#define __LL_C

/* includes */
#include "pmacct.h"
#include "pmacct-data.h"

/* eth_handler() picks a whole packet, reads
   informtions contained in the link layer
   protocol header and fills a pointer structure */ 
void eth_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs) 
{
  u_int16_t e8021Q, ppp;
  struct eth_header *eth_pk;
  u_int16_t etype, caplen = h->caplen, nl;
  u_int8_t cursor = 0;

  if (caplen < ETHER_HDRLEN) {
    pptrs->iph_ptr = NULL;
    return;
  }

  eth_pk = (struct eth_header *) pptrs->packet_ptr;
  etype = ntohs(eth_pk->ether_type);
  pptrs->mac_ptr = (u_char *) eth_pk->ether_dhost; 
  pptrs->vlan_ptr = NULL; /* avoid stale vlan pointers */
  pptrs->mpls_ptr = NULL; /* avoid stale MPLS pointers */
  nl = ETHER_HDRLEN;
  caplen -= ETHER_HDRLEN;

  recurse:
  if (etype == ETHERTYPE_IP) {
    pptrs->l3_proto = ETHERTYPE_IP; 
    pptrs->l3_handler = ip_handler;
    pptrs->iph_ptr = pptrs->packet_ptr + nl;
    return;
  }
#if defined ENABLE_IPV6
  if (etype == ETHERTYPE_IPV6) {
    pptrs->l3_proto = ETHERTYPE_IPV6;
    pptrs->l3_handler = ip6_handler;
    pptrs->iph_ptr = pptrs->packet_ptr + nl;
    return;
  }
#endif

  /* originally contributed by Rich Gade */
  if (etype == ETHERTYPE_8021Q) {
    if (caplen < IEEE8021Q_TAGLEN) {
      pptrs->iph_ptr = NULL;
      return;
    }
    memcpy(&e8021Q, pptrs->packet_ptr+nl+2, 2);
    if (!cursor) pptrs->vlan_ptr = pptrs->packet_ptr + nl; 
    etype = ntohs(e8021Q);
    nl += IEEE8021Q_TAGLEN;
    caplen -= IEEE8021Q_TAGLEN;
    cursor++;
    goto recurse;
  }

  /* originally contributed by Vasiliy Ponomarev */
  if (etype == ETHERTYPE_PPPOE) {
    if (caplen < PPPOE_HDRLEN+PPP_TAGLEN) {
      pptrs->iph_ptr = NULL;
      return;
    }
    memcpy(&ppp, pptrs->packet_ptr+nl+PPPOE_HDRLEN, 2);
    etype = ntohs(ppp);
    if (etype == PPP_IP) etype = ETHERTYPE_IP; 
#if defined ENABLE_IPV6
    if (etype == PPP_IPV6) etype = ETHERTYPE_IPV6;
#endif 
    nl += PPPOE_HDRLEN+PPP_TAGLEN;
    caplen -= PPPOE_HDRLEN+PPP_TAGLEN;
    cursor = 1;
    goto recurse;
  }

  if (etype == ETHERTYPE_MPLS || etype == ETHERTYPE_MPLS_MULTI) {
    etype = mpls_handler(pptrs->packet_ptr + nl, &caplen, &nl, pptrs);
    cursor = 1;
    goto recurse;
  }

  pptrs->l3_proto = 0;
  pptrs->l3_handler = NULL;
  pptrs->iph_ptr = NULL;
}

u_int16_t mpls_handler(u_char *bp, u_int16_t *caplen, u_int16_t *nl, register struct packet_ptrs *pptrs)
{
  u_char *p = bp;
  u_int32_t label=0;

  pptrs->mpls_ptr = bp;

  if (*caplen < 4) {
    pptrs->iph_ptr = NULL;
    return;
  }

  do {
    label = ntohl(*p);
    p += 4; *nl += 4; *caplen -= 4;
  } while (!MPLS_STACK(label) && *caplen >= 4);

  switch (MPLS_LABEL(label)) {
  case 0: /* IPv4 explicit NULL label */
  case 3: /* IPv4 implicit NULL label */
    return ETHERTYPE_IP;
#if defined ENABLE_IPV6
  case 2: /* IPv6 explicit NULL label */
    return ETHERTYPE_IPV6;
#endif
  default:
    /* 
       support for what is sometimes referred as null-encapsulation:
       by looking at the first payload byte (but only if the Bottom
       of Stack bit is set) we try to determine the network layer
       protocol: 
       0x45-0x4f is IPv4
       0x60-0x6f is IPv6
    */
    if (MPLS_STACK(label)) { 
      switch (*p) {
      case 0x45:
      case 0x46:
      case 0x47:
      case 0x48:
      case 0x49:
      case 0x4a:
      case 0x4b:
      case 0x4c:
      case 0x4d:
      case 0x4e:
      case 0x4f:
	return ETHERTYPE_IP;
#if defined ENABLE_IPV6 
      case 0x60:
      case 0x61:
      case 0x62:
      case 0x63:
      case 0x64:
      case 0x65:
      case 0x66:
      case 0x67:
      case 0x68:
      case 0x69:
      case 0x6a:
      case 0x6b:
      case 0x6c:
      case 0x6d:
      case 0x6e:
      case 0x6f:
	return ETHERTYPE_IPV6;
#endif
      default:
        break;
      }
    }
    break;
  }

  return FALSE;
}

void fddi_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs) 
{
  register u_char *p;
  u_int caplen = h->caplen;
  struct fddi_header *fddi_pk;

  if (caplen < FDDI_HDRLEN) {
    pptrs->iph_ptr = NULL;
    return;
  }

  p = pptrs->packet_ptr;
  fddi_pk = (struct fddi_header *) pptrs->packet_ptr;
  if ((fddi_pk->fddi_fc & FDDIFC_CLFF) == FDDIFC_LLC_ASYNC) {
    pptrs->mac_ptr = (u_char *) fddi_pk->fddi_dhost; 

    /* going up to LLC/SNAP layer header */
    p += FDDI_HDRLEN;
    caplen -= FDDI_HDRLEN;
    if ((p = llc_handler(h, caplen, p, pptrs)) != NULL) {
      pptrs->iph_ptr = p;
      return;
    }
  }

  pptrs->iph_ptr = NULL; 
}

void tr_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  register u_char *p;
  u_int caplen = h->caplen, route_len = 0;
  struct token_header *tr_pk;

  if (caplen < ETHER_HDRLEN) {
    pptrs->iph_ptr = NULL;
    return;
  }

  p = pptrs->packet_ptr;
  tr_pk = (struct token_header *) pptrs->packet_ptr;
  if (TR_IS_SOURCE_ROUTED(tr_pk)) {
    route_len = TR_RIF_LENGTH(tr_pk);
    if (caplen < ETHER_HDRLEN + route_len) {
      pptrs->iph_ptr = NULL;
      return;
    }
  }

  if (((tr_pk->token_fc & 0xC0) >> 6) == TOKEN_FC_LLC) {
    pptrs->mac_ptr = (u_char *) tr_pk->token_dhost;

    /* going up to LLC/SNAP layer header */
    p += (ETHER_HDRLEN + route_len);
    caplen -= (ETHER_HDRLEN + route_len);
    if ((p = llc_handler(h, caplen, p, pptrs)) != NULL) {
      pptrs->iph_ptr = p;
      return;
    }
  }

  pptrs->iph_ptr = NULL;
}

void ppp_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  u_char *p = pptrs->packet_ptr;
  u_int16_t caplen = h->caplen, nl = 0;
  unsigned int proto = 0;

  if (caplen < PPP_HDRLEN) {
    pptrs->iph_ptr = NULL;
    return;
  }

  if (*p == PPP_ADDRESS && *(p + 1) == PPP_CONTROL) {
    p += 2;
    caplen -= 2;
    if (caplen < 2) {
      pptrs->iph_ptr = NULL;
      return;
    }
  }
   
  if (*p % 2) {
    proto = *p;
    p++;
  }
  else {
    proto = EXTRACT_16BITS(p);
    p += 2;
  }

  recurse:
  if ((proto == PPP_IP) || (proto == ETHERTYPE_IP)) { 
    pptrs->l3_proto = ETHERTYPE_IP; 
    pptrs->l3_handler = ip_handler;
    pptrs->iph_ptr = p;
    return;
  }
#if defined ENABLE_IPV6
  if ((proto == PPP_IPV6) || (proto == ETHERTYPE_IPV6)) {
    pptrs->l3_proto = ETHERTYPE_IPV6;
    pptrs->l3_handler = ip6_handler;
    pptrs->iph_ptr = p;
    return;
  }
#endif

  if (proto == PPP_MPLS_UCAST || proto == PPP_MPLS_MCAST) {
    proto = mpls_handler(p, &caplen, &nl, pptrs);
    goto recurse;
  }

  pptrs->l3_proto = 0;
  pptrs->l3_handler = NULL;
  pptrs->iph_ptr = NULL;
}

/*
  support for 802.11 Wireless LAN protocol. I'm writing
  it during a sad morning spent at Fiumicino's Airport
  because of Alitalia strikes. It's currently working
  well for me at FCO WiFi zone. Let me know. 

			28-11-2003, Paolo. 
*/
void ieee_802_11_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  u_int16_t fc;
  u_int caplen = h->caplen;
  short int hdrlen;
  u_char *p;

  if (caplen < IEEE802_11_FC_LEN) {
    pptrs->iph_ptr = NULL;
    return;
  }

  p = pptrs->packet_ptr;

  fc = EXTRACT_LE_16BITS(p);
  if (FC_TYPE(fc) == T_DATA) {
    if (FC_TO_DS(fc) && FC_FROM_DS(fc)) hdrlen = 30;
    else hdrlen = 24;
    if (caplen < hdrlen) {
      pptrs->iph_ptr = NULL;
      return;
    }
    caplen -= hdrlen;
    p += hdrlen;
    if (!FC_WEP(fc)) {
      if ((p = llc_handler(h, caplen, p, pptrs)) != NULL) {
	pptrs->iph_ptr = p;
        return;
      }
    }
  }
  
  pptrs->l3_proto = 0;
  pptrs->l3_handler = NULL;
  pptrs->iph_ptr = NULL;
}

void raw_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  register u_int16_t caplen = h->caplen;
  struct my_iphdr *hdr;

  if (caplen < 4) {
    pptrs->iph_ptr = NULL;
    return;
  } 

  hdr = (struct my_iphdr *) pptrs->packet_ptr;
  switch (IP_V(hdr)) {
  case 4:
    pptrs->iph_ptr = pptrs->packet_ptr; 
    pptrs->l3_proto = ETHERTYPE_IP;
    pptrs->l3_handler = ip_handler;
    return;
#if defined ENABLE_IPV6
  case 6:
    pptrs->iph_ptr = pptrs->packet_ptr;
    pptrs->l3_proto = ETHERTYPE_IPV6;
    pptrs->l3_handler = ip6_handler;
    return;
#endif
  default:
    pptrs->iph_ptr = NULL;
    pptrs->l3_proto = 0;
    pptrs->l3_handler = NULL; 
    return;
  }
}

void null_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  register u_int32_t *family;
  u_int caplen = h->caplen;
  u_char *p;

  if (caplen < 4) {
    pptrs->iph_ptr = NULL;
    return;
  }

  family = (u_int32_t *) pptrs->packet_ptr;
  p = pptrs->packet_ptr;

  if (*family == AF_INET || ntohl(*family) == AF_INET ) {
    pptrs->l3_proto = ETHERTYPE_IP;
    pptrs->l3_handler = ip_handler;
    pptrs->iph_ptr = (u_char *)(pptrs->packet_ptr + 4);
    return;
  }

#if defined ENABLE_IPV6
  if (*family == AF_INET6 || ntohl(*family) == AF_INET6 ) {
    pptrs->l3_proto = ETHERTYPE_IPV6;
    pptrs->l3_handler = ip6_handler;
    pptrs->iph_ptr = (u_char *)(pptrs->packet_ptr + 4);
    return;
  }
#endif

  pptrs->l3_proto = 0;
  pptrs->l3_handler = NULL;
  pptrs->iph_ptr = NULL;
}

void sll_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  register const struct sll_header *sllp;
  register u_short etype;
  u_char *p;
  u_int caplen = h->caplen;
  u_int16_t e8021Q, nl;

  if (caplen < SLL_HDR_LEN) {
    pptrs->iph_ptr = NULL;
    return;
  }

  pptrs->mac_ptr = NULL;
  pptrs->vlan_ptr = NULL;
  pptrs->mpls_ptr = NULL;

  p = pptrs->packet_ptr;

  sllp = (const struct sll_header *) pptrs->packet_ptr;
  etype = ntohs(sllp->sll_protocol);
  nl = SLL_HDR_LEN;

  if (EXTRACT_16BITS(&sllp->sll_halen) == ETH_ADDR_LEN) {
    memcpy(sll_mac[1], sllp->sll_addr, ETH_ADDR_LEN);
    pptrs->mac_ptr = (char *) sll_mac;
  }

  recurse:
  if (etype == ETHERTYPE_IP) {
    pptrs->l3_proto = ETHERTYPE_IP;
    pptrs->l3_handler = ip_handler; 
    pptrs->iph_ptr = (u_char *)(pptrs->packet_ptr + nl);
    return;
  }
  
  /* XXX: ETHERTYPE_IPV6 ? */

  if (etype == LINUX_SLL_P_802_2) {
    /* going up to LLC/SNAP layer header */
    p += SLL_HDR_LEN;
    caplen -= SLL_HDR_LEN;
    if ((p = llc_handler(h, caplen, p, pptrs)) != NULL) {
      pptrs->iph_ptr = p;
      return;
    }
  }

  /* originally contributed by Rich Gade for eth_handler() */
  if (etype == ETHERTYPE_8021Q) {
    if (caplen < IEEE8021Q_TAGLEN) {
      pptrs->iph_ptr = NULL;
      return;
    }
    memcpy(&e8021Q, pptrs->packet_ptr+nl+2, 2);
    pptrs->vlan_ptr = pptrs->packet_ptr + nl;
    etype = ntohs(e8021Q);
    nl += IEEE8021Q_TAGLEN;
    caplen -= IEEE8021Q_TAGLEN;
    goto recurse;
  }

  pptrs->l3_proto = 0;
  pptrs->l3_handler = NULL;
  pptrs->iph_ptr = NULL;
}

u_char *llc_handler(const struct pcap_pkthdr *h, u_int caplen, register u_char *buf, register struct packet_ptrs *pptrs)
{
  struct llc llc;
  register u_short etype;

  if (caplen < 3) return NULL;

  memcpy((char *)&llc, (char *) buf, min(caplen, sizeof(llc)));
  if (llc.ssap == LLCSAP_SNAP && llc.dsap == LLCSAP_SNAP
      && llc.ctl.snap.snap_ui == LLC_UI) {
    etype = EXTRACT_16BITS(&llc.ctl.snap_ether.snap_ethertype[0]);
    if (etype == ETHERTYPE_IP) {
      pptrs->l3_proto = ETHERTYPE_IP;
      pptrs->l3_handler = ip_handler;
      return (u_char *)(buf + min(caplen, sizeof(llc)));
    }
#if defined ENABLE_IPV6
    if (etype == ETHERTYPE_IPV6) {
      pptrs->l3_proto = ETHERTYPE_IPV6;
      pptrs->l3_handler = ip6_handler;
      return (u_char *)(buf + min(caplen, sizeof(llc)));
    }
#endif
    else return 0; 
  }
  else return 0;
}

void chdlc_handler(const struct pcap_pkthdr *h, register struct packet_ptrs *pptrs)
{
  u_char *p = pptrs->packet_ptr;
  u_int16_t caplen = h->caplen, nl = 0;
  u_int16_t proto;

  if (caplen < CHDLC_HDRLEN) {
    pptrs->iph_ptr = NULL;
    return; 
  }

  proto = EXTRACT_16BITS(&p[2]);
  caplen -= CHDLC_HDRLEN;
  p += CHDLC_HDRLEN;

  recurse:
  switch (proto) {
  case ETHERTYPE_IP:
    pptrs->l3_proto = ETHERTYPE_IP;
    pptrs->l3_handler = ip_handler;
    pptrs->iph_ptr = p;
    return;
#if defined ENABLE_IPV6 
  case ETHERTYPE_IPV6:
    pptrs->l3_proto = ETHERTYPE_IPV6;
    pptrs->l3_handler = ip6_handler;
    pptrs->iph_ptr = p;
    return;
#endif
  case ETHERTYPE_MPLS:
  case ETHERTYPE_MPLS_MULTI:
    proto = mpls_handler(p, &caplen, &nl, pptrs);
    goto recurse;
  default:
    break;
  }

  pptrs->l3_proto = 0;
  pptrs->l3_handler = NULL;
  pptrs->iph_ptr = NULL;
}
