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
#define __BGP_C

/* includes */
#include "pmacct.h"
#include "bgp.h"
#include "thread_pool.h"

/* variables to be exported away */
thread_pool_t *bgp_pool;

/* Functions */
void nfacctd_bgp_wrapper()
{
  /* initialize variables */
  if (!config.nfacctd_bgp_port) config.nfacctd_bgp_port = BGP_TCP_PORT;

  /* initialize threads pool */
  bgp_pool = allocate_thread_pool(1);
  assert(bgp_pool);
  Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): %d thread(s) initialized\n", 1);

  /* giving a kick to the BGP thread */
  send_to_pool(bgp_pool, skinny_bgp_daemon, NULL);
}

void skinny_bgp_daemon()
{
  int slen, ret, sock, rc, peers_idx;
  struct host_addr addr;
  struct bgp_header bhdr;
  struct bgp_peer *peer;
  struct bgp_open *bopen;
  char bgp_packet[BGP_MAX_PACKET_SIZE], *bgp_packet_ptr;
  char bgp_reply_pkt[BGP_MAX_PACKET_SIZE], *bgp_reply_pkt_ptr;
#if defined ENABLE_IPV6
  struct sockaddr_storage server, client;
  struct ipv6_mreq multi_req6;
#else
  struct sockaddr server, client;
#endif
  afi_t afi;
  safi_t safi;
  int clen = sizeof(client);
  u_int16_t remote_as = 0;
  u_int32_t remote_as4 = 0;
  time_t now;

  /* select() stuff */
  fd_set read_descs, bkp_read_descs; 
  int select_fd, select_num;

  /* initial cleanups */
  memset(&server, 0, sizeof(server));
  memset(&client, 0, sizeof(client));
  memset(bgp_packet, 0, BGP_MAX_PACKET_SIZE);
  bgp_attr_init();

  /* socket creation for BGP server: mostly stolen from nfacctd.c code */
#if (defined ENABLE_IPV6 && defined V4_MAPPED)
  if (!config.nfacctd_bgp_ip) {
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&server;

    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = htons(config.nfacctd_bgp_port);
    slen = sizeof(struct sockaddr_in6);
  }
#else
  if (!config.nfacctd_bgp_ip) {
    struct sockaddr_in *sa4 = (struct sockaddr_in *)&server;

    sa4->sin_family = AF_INET;
    sa4->sin_addr.s_addr = htonl(0);
    sa4->sin_port = htons(config.nfacctd_bgp_port);
    slen = sizeof(struct sockaddr_in);
  }
#endif
  else {
    trim_spaces(config.nfacctd_bgp_ip);
    ret = str_to_addr(config.nfacctd_bgp_ip, &addr);
    if (!ret) {
      Log(LOG_ERR, "ERROR ( default/core/BGP ): 'nfacctd_bgp_ip' value is not valid. Terminating thread.\n");
      exit_all(1);
    }
    slen = addr_to_sa((struct sockaddr *)&server, &addr, config.nfacctd_bgp_port);
  }

  if (!config.nfacctd_bgp_max_peers) config.nfacctd_bgp_max_peers = MAX_BGP_PEERS_DEFAULT;
  Log(LOG_INFO, "INFO ( default/core/BGP ): maximum BGP peers allowed: %d\n", config.nfacctd_bgp_max_peers);

  peers = malloc(config.nfacctd_bgp_max_peers*sizeof(struct bgp_peer));
  memset(peers, 0, config.nfacctd_bgp_max_peers*sizeof(struct bgp_peer));

  sock = socket(((struct sockaddr *)&server)->sa_family, SOCK_STREAM, 0);
  if (sock < 0) {
    Log(LOG_ERR, "ERROR ( default/core/BGP ): thread socket() failed. Terminating thread.\n");
    exit_all(1);
  }

  rc = bind(sock, (struct sockaddr *) &server, slen);
  if (rc < 0) {
    char null_ip_address[] = "0.0.0.0";
    char *ip_address;

    ip_address = config.nfacctd_bgp_ip ? config.nfacctd_bgp_ip : null_ip_address;
    Log(LOG_ERR, "ERROR ( default/core/BGP ): bind() to ip=%s port=%d/tcp failed (errno: %d).\n", ip_address, config.nfacctd_bgp_port, errno);
    exit_all(1);
  }

  rc = listen(sock, config.nfacctd_bgp_max_peers);
  if (rc < 0) {
    Log(LOG_ERR, "ERROR ( default/core/BGP ): listen() failed (errno: %d).\n", errno);
    exit_all(1);
  }

  /* Preparing for syncronous I/O multiplexing */
  select_fd = 0;
  FD_ZERO(&bkp_read_descs);
  FD_SET(sock, &bkp_read_descs);

  for (;;) {
    select_again:
    select_fd = sock;
    for (peers_idx = 0; peers_idx < config.nfacctd_bgp_max_peers; peers_idx++)
      if (select_fd < peers[peers_idx].fd) select_fd = peers[peers_idx].fd; 
    select_fd++;
    memcpy(&read_descs, &bkp_read_descs, sizeof(bkp_read_descs));
    select_num = select(select_fd, &read_descs, NULL, NULL, NULL);
    if (select_num < 0) goto select_again;

    /* New connection is coming in */ 
    if (FD_ISSET(sock, &read_descs)) {
      for (peer = NULL, peers_idx = 0; peers_idx < config.nfacctd_bgp_max_peers; peers_idx++) {
        if (peers[peers_idx].fd == 0) {
          peer = &peers[peers_idx];
          bgp_peer_init(peer);
          break;
        }
      }

      if (!peer) {
        Log(LOG_ERR, "ERROR ( default/core/BGP ): Insufficient number of BGP peers has been configured by 'nfacctd_bgp_max_peers' (%d).\n", config.nfacctd_bgp_max_peers);
        // exit_all(1); 
	goto select_again;
      }
      peer->fd = accept(sock, (struct sockaddr *) &client, &clen);
      FD_SET(peer->fd, &bkp_read_descs);
      peer->addr.family = AF_INET;
      peer->addr.address.ipv4.s_addr = ((struct sockaddr_in *)&client)->sin_addr.s_addr;
      goto select_again; 
    }

    /* We have something coming in: let's lookup which peer is thatl
       XXX: to be optimized */
    for (peer = NULL, peers_idx = 0; peers_idx < config.nfacctd_bgp_max_peers; peers_idx++) {
      if (peers[peers_idx].fd && FD_ISSET(peers[peers_idx].fd, &read_descs)) {
	peer = &peers[peers_idx];
	break;
      }
    } 

    if (!peer) {
      Log(LOG_ERR, "ERROR ( default/core/BGP ): message delivered to an unknown peer (FD bits: %d; FD max: %d)\n", select_num, select_fd);
      goto select_again;
    }
    
    peer->msglen = ret = recv(peer->fd, bgp_packet, BGP_MAX_PACKET_SIZE, 0);

    if (ret <= 0) {
      Log(LOG_INFO, "INFO ( default/core/BGP ): Existing BGP connection was reset (%d). Goto accept()\n", errno);
      FD_CLR(peer->fd, &bkp_read_descs);
      bgp_peer_close(peer);
      goto select_again;
    }
    else if (peer->msglen+peer->buf.truncated_len < BGP_HEADER_SIZE) {
      Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (too short). Goto accept()\n");
      FD_CLR(peer->fd, &bkp_read_descs);
      bgp_peer_close(peer);
      goto select_again;
    }
    else {
      /* Appears a valid peer with a valid BGP message: before
	 continuing let's see if it's time to send a KEEPALIVE
	 back */
      now = time(NULL);
      if (peer->status == Established && ((now - peer->last_keepalive) > (peer->ht / 2))) {
        bgp_reply_pkt_ptr = bgp_reply_pkt;
        bgp_reply_pkt_ptr += bgp_keepalive_msg(bgp_reply_pkt_ptr);
        ret = send(peer->fd, bgp_reply_pkt, bgp_reply_pkt_ptr - bgp_reply_pkt, 0);
	peer->last_keepalive = now;
      } 

      /* BGP payload reassembly if required */
      if (peer->buf.truncated_len) {
	if (peer->buf.truncated_len+peer->msglen > peer->buf.len) {
	  char *newptr;

	  peer->buf.len += peer->buf.truncated_len+peer->msglen;
	  newptr = malloc(peer->buf.len);
	  memcpy(newptr, peer->buf.base, peer->buf.truncated_len);
	  free(peer->buf.base);
	  peer->buf.base = newptr;
	}
	memcpy(peer->buf.base+peer->buf.truncated_len, bgp_packet, peer->msglen);
	peer->msglen += peer->buf.truncated_len;
	peer->buf.truncated_len = 0;

	bgp_packet_ptr = peer->buf.base;
      }
      else {
	if (peer->buf.len > BGP_MAX_PACKET_SIZE) { 
	  realloc(peer->buf.base, BGP_MAX_PACKET_SIZE);
	  memset(peer->buf.base, 0, BGP_MAX_PACKET_SIZE);
	  peer->buf.len = BGP_MAX_PACKET_SIZE;
	}
	bgp_packet_ptr = bgp_packet;
      } 

      memset(&bhdr, 0, sizeof(bhdr));
      for ( ; peer->msglen > 0; peer->msglen -= ntohs(bhdr.bgpo_len), bgp_packet_ptr += ntohs(bhdr.bgpo_len)) { 
	memcpy(&bhdr, bgp_packet_ptr, sizeof(bhdr));

	/* BGP payload fragmentation check */
	if (peer->msglen < BGP_HEADER_SIZE || peer->msglen < ntohs(bhdr.bgpo_len)) {
	  peer->buf.truncated_len = peer->msglen;
	  if (bgp_packet_ptr != peer->buf.base)
	    memcpy(peer->buf.base, bgp_packet_ptr, peer->buf.truncated_len);
	    // goto bgp_recv;
	    goto select_again;
	  }

	  if (!bgp_marker_check(&bhdr, BGP_MARKER_SIZE)) {
            Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (marker check failed). Goto accept()\n");
	    FD_CLR(peer->fd, &bkp_read_descs);
	    bgp_peer_close(peer);
	    goto select_again;
          }

	  memset(bgp_reply_pkt, 0, BGP_MAX_PACKET_SIZE);

	  switch (bhdr.bgpo_type) {
	  case BGP_OPEN:
		  if (peer->status < OpenSent) {
		    peer->status = Active;
		    bopen = (struct bgp_open *) bgp_packet;  

		    if (bopen->bgpo_version == BGP_VERSION4) {
			  char bgp_open_cap_reply[BGP_MAX_PACKET_SIZE-BGP_MIN_OPEN_MSG_SIZE];
			  char *bgp_open_cap_reply_ptr = bgp_open_cap_reply, *bgp_open_cap_ptr;

			  remote_as = ntohs(bopen->bgpo_myas);
			  peer->ht = MAX(5, ntohs(bopen->bgpo_holdtime));
			  peer->id.family = AF_INET; 
			  peer->id.address.ipv4.s_addr = bopen->bgpo_id;

			  /* OPEN options parsing */
			  if (bopen->bgpo_optlen && bopen->bgpo_optlen >= 2) {
			    u_int8_t len, opt_type, opt_len, cap_type;
			    char *ptr;

			    ptr = bgp_packet + BGP_MIN_OPEN_MSG_SIZE;
			    memset(bgp_open_cap_reply, 0, sizeof(bgp_open_cap_reply));

			    for (len = bopen->bgpo_optlen; len > 0; len -= opt_len, ptr += opt_len) {
				  opt_type = (u_int8_t) ptr[0];
				  opt_len = (u_int8_t) ptr[1];

				  if (opt_len > bopen->bgpo_optlen) {
				    Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (option length). Goto accept()\n");
				    FD_CLR(peer->fd, &bkp_read_descs);
				    bgp_peer_close(peer);
				    goto select_again;
				  } 

				  /* 
 				   * If we stumble upon capabilities let's curse through them to find
 				   * some we are forced to support (ie. MP-BGP or 4-bytes AS support)
 				   */
				  if (opt_type == BGP_OPTION_CAPABILITY) {
				    bgp_open_cap_ptr = ptr;
				    ptr += 2;
				    len -=2;

				    cap_type = (u_int8_t) ptr[0];
				    if (cap_type == BGP_CAPABILITY_MULTIPROTOCOL) {
					  char *cap_ptr = ptr+2;
					  struct capability_mp_data cap_data;

					  memcpy(&cap_data, cap_ptr, sizeof(cap_data));
					  
				      Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): Capability: MultiProtocol [%x] AFI [%x] SAFI [%x]\n",
							cap_type, ntohs(cap_data.afi), cap_data.safi);
					  peer->cap_mp = TRUE;
					  memcpy(bgp_open_cap_reply_ptr, bgp_open_cap_ptr, opt_len+2); 
					  bgp_open_cap_reply_ptr += opt_len+2;
				    }
				    else if (cap_type == BGP_CAPABILITY_4_OCTET_AS_NUMBER) {
					  u_int32_t *as4_ptr;
					  u_int8_t cap_len = ptr[1];
					  char *cap_ptr = ptr+2;

				   	  if (cap_len == CAPABILITY_CODE_AS4_LEN && cap_len == (opt_len-2)) {
						struct capability_as4 *cap_data = (struct capability_as4 *) cap_ptr;

				        Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): Capability 4-bytes AS [%x] ASN [%u]\n",
							cap_type, ntohl(cap_data->as4));
						as4_ptr = (u_int32_t *) cap_ptr;
						remote_as4 = ntohl(*as4_ptr);
					    memcpy(bgp_open_cap_reply_ptr, bgp_open_cap_ptr, opt_len+2); 
						peer->cap_4as = bgp_open_cap_reply_ptr+4;
					    bgp_open_cap_reply_ptr += opt_len+2;
					  }
					  else {
					    Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (malformed AS4 option). Goto accept()\n");
						FD_CLR(peer->fd, &bkp_read_descs);
						bgp_peer_close(peer);
						goto select_again;
					  }
				    }
				  }
				  else {
				    ptr += 2;
				    len -= 2;
				  }
			    } 
			  }

			  /* Let's grasp the remote ASN */
			  if (remote_as == BGP_AS_TRANS) {
				if (remote_as4 && remote_as4 != BGP_AS_TRANS)
				  peer->as = remote_as4;
				/* It is not valid to use the transitional ASN in the BGP OPEN and
 				   present an ASN == 0 or ASN == 23456 in the 4AS capability */
				else {
				  Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (invalid AS4 option). Goto accept()\n");
				  FD_CLR(peer->fd, &bkp_read_descs);
				  bgp_peer_close(peer);
				  goto select_again;
				}
			  }
			  else {
				if (remote_as4 == 0 || remote_as4 == remote_as)
				  peer->as = remote_as;
 				/* It is not valid to not use the transitional ASN in the BGP OPEN and
				   present an ASN != remote_as in the 4AS capability */
				else {
				  Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (mismatching AS4 option). Goto accept()\n");
				  FD_CLR(peer->fd, &bkp_read_descs);
				  bgp_peer_close(peer);
				  goto select_again;
				}
			  }

			  Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): BGP_OPEN: Id: %s Asn: %u HoldTime: %u\n", inet_ntoa(peer->id.address.ipv4), peer->as, peer->ht);

			  bgp_reply_pkt_ptr = bgp_reply_pkt;

			  /* Replying to OPEN message */
			  ret = bgp_open_msg(bgp_reply_pkt_ptr, bgp_open_cap_reply, bgp_open_cap_reply_ptr-bgp_open_cap_reply, peer);
			  if (ret > 0) bgp_reply_pkt_ptr += ret;
			  else {
				Log(LOG_INFO, "INFO ( default/core/BGP ): Local peer is 4AS while remote peer is 2AS: unsupported configuration. Goto accept()\n");
				FD_CLR(peer->fd, &bkp_read_descs);
				bgp_peer_close(peer);
				goto select_again;
			  }

			  /* sticking a KEEPALIVE to it */
			  bgp_reply_pkt_ptr += bgp_keepalive_msg(bgp_reply_pkt_ptr);
			  ret = send(peer->fd, bgp_reply_pkt, bgp_reply_pkt_ptr - bgp_reply_pkt, 0);
			  peer->last_keepalive = now;
		    }
		    else {
  			  Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (unsupported version). Goto accept()\n");
			  FD_CLR(peer->fd, &bkp_read_descs);
			  bgp_peer_close(peer);
			  goto select_again;
		    }

			// peer->status = OpenSent;
			peer->status = Established;
	      }
		  /* If we already passed successfully through an BGP OPEN exchange
  			 let's just ignore further BGP OPEN messages */
		  break;
	  case BGP_NOTIFICATION:
		  Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): BGP_NOTIFICATION: Id: %s\n", inet_ntoa(peer->id.address.ipv4));
		  FD_CLR(peer->fd, &bkp_read_descs);
		  bgp_peer_close(peer);
		  goto select_again;
		  break;
	  case BGP_KEEPALIVE:
		  if (peer->status >= OpenSent) {
		    if (peer->status < Established) peer->status = Established;

		    bgp_reply_pkt_ptr = bgp_reply_pkt;
		    bgp_reply_pkt_ptr += bgp_keepalive_msg(bgp_reply_pkt_ptr);
		    ret = send(peer->fd, bgp_reply_pkt, bgp_reply_pkt_ptr - bgp_reply_pkt, 0);
		    peer->last_keepalive = now;

		    Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): BGP_KEEPALIVE: Id: %s\n", inet_ntoa(peer->id.address.ipv4));
		  }
		  /* If we didn't pass through a successful BGP OPEN exchange just yet
  			 let's temporarily discard BGP KEEPALIVEs */
		  break;
	  case BGP_UPDATE:
		  //printf("BGP UPDATE\n");
		  if (peer->status < Established) {
		    Log(LOG_DEBUG, "DEBUG ( default/core/BGP ): BGP UPDATE: Id: %s (no neighbor). Goto accept()\n", inet_ntoa(peer->id.address.ipv4));
			FD_CLR(peer->fd, &bkp_read_descs);
			bgp_peer_close(peer);
			goto select_again;
		  }

		  ret = bgp_update_msg(peer, bgp_packet_ptr);
		  if (ret < 0) Log(LOG_WARNING, "WARN ( default/core/BGP ): BGP UPDATE: malformed (%d)\n", ret);
		  break;
	    default:
	      Log(LOG_INFO, "INFO ( default/core/BGP ): Received malformed BGP packet (unsupported message type). Goto accept()\n");
	      FD_CLR(peer->fd, &bkp_read_descs);
	      bgp_peer_close(peer);
	      goto select_again;
	    }
	  }
	}
  }
}

/* Marker check. */
int bgp_marker_check(struct bgp_header *bhdr, int length)
{
  int i;

  for (i = 0; i < length; i++)
    if (bhdr->bgpo_marker[i] != 0xff)
      return 0;

  return 1;
}

/* write BGP KEEPALIVE msg */
int bgp_keepalive_msg(char *msg)
{
  struct bgp_header bhdr;
	
  memset(&bhdr.bgpo_marker, 0xff, BGP_MARKER_SIZE);
  bhdr.bgpo_type = BGP_KEEPALIVE;
  bhdr.bgpo_len = htons(BGP_HEADER_SIZE);
  memcpy(msg, &bhdr, sizeof(bhdr));

  return BGP_HEADER_SIZE;
}

/* write BGP OPEN msg */
int bgp_open_msg(char *msg, char *cp_msg, int cp_msglen, struct bgp_peer *peer)
{
  struct bgp_open *bopen_reply = (struct bgp_open *) msg;
  char my_id_static[] = "1.2.3.4", *my_id = my_id_static;
  struct host_addr my_id_addr;
  u_int16_t local_as;
  u_int32_t *local_as4;

  memset(bopen_reply->bgpo_marker, 0xff, BGP_MARKER_SIZE);
  bopen_reply->bgpo_type = BGP_OPEN;
  bopen_reply->bgpo_version = BGP_VERSION4;
  bopen_reply->bgpo_holdtime = htons(peer->ht);
  if (config.nfacctd_bgp_myas > BGP_AS_MAX) {
	if (peer->cap_4as) {
	  bopen_reply->bgpo_myas = htons(BGP_AS_TRANS);
	  local_as4 = (u_int32_t *) peer->cap_4as;
	  *local_as4 = htonl(config.nfacctd_bgp_myas);
	}
	/* This is currently an unsupported configuration */
	else return -1;
  }
  else {
	local_as = config.nfacctd_bgp_myas;
	bopen_reply->bgpo_myas = htons(local_as);
	if (peer->cap_4as) {
	  local_as4 = (u_int32_t *) peer->cap_4as;
	  *local_as4 = htonl(config.nfacctd_bgp_myas);
	}
  }

  bopen_reply->bgpo_optlen = cp_msglen;
  bopen_reply->bgpo_len = htons(BGP_MIN_OPEN_MSG_SIZE + bopen_reply->bgpo_optlen);

  if (config.nfacctd_bgp_ip) my_id = config.nfacctd_bgp_ip;
  str_to_addr(my_id, &my_id_addr);
  bopen_reply->bgpo_id = my_id_addr.address.ipv4.s_addr;

  memcpy(msg+BGP_MIN_OPEN_MSG_SIZE, cp_msg, cp_msglen);

  return BGP_MIN_OPEN_MSG_SIZE + cp_msglen;
}

int bgp_update_msg(struct bgp_peer *peer, char *pkt)
{
  struct bgp_header bhdr;
  u_char *startp, *endp;
  struct bgp_attr attr;
  u_int16_t attribute_len;
  u_int16_t update_len;
  u_int16_t withdraw_len;
  u_int16_t end, tmp;
  struct bgp_nlri update;
  struct bgp_nlri withdraw;
  struct bgp_nlri mp_update;
  struct bgp_nlri mp_withdraw;
  int ret;

  /* Set initial values. */
  memset(&attr, 0, sizeof (struct bgp_attr));
  memset(&update, 0, sizeof (struct bgp_nlri));
  memset(&withdraw, 0, sizeof (struct bgp_nlri));
  memset(&mp_update, 0, sizeof (struct bgp_nlri));
  memset(&mp_withdraw, 0, sizeof (struct bgp_nlri));

  memcpy(&bhdr, pkt, sizeof(bhdr));
  end = ntohs(bhdr.bgpo_len);
  end -= BGP_HEADER_SIZE;
  pkt += BGP_HEADER_SIZE;

  /* handling Unfeasible routes */
  memcpy(&tmp, pkt, 2);
  withdraw_len = ntohs(tmp);
  //printf("WITHDRAW_LEN: %u\n", withdraw_len);
  if (withdraw_len > end) return -1;  
  else {
	end -= withdraw_len;
    pkt += 2; end -= 2;
  }

  if (withdraw_len > 0) {
	withdraw.afi = AFI_IP;
	withdraw.safi = SAFI_UNICAST;
	withdraw.nlri = pkt;
	withdraw.length = withdraw_len;
    pkt += withdraw_len;
  }

  /* handling Attributes */
  memcpy(&tmp, pkt, 2);
  attribute_len = ntohs(tmp);
  //printf("ATTRIBUTE_LEN: %u\n", attribute_len);
  if (attribute_len > end) return -1;
  else {
	end -= attribute_len;
	pkt += 2; end -= 2;
  }

  if (attribute_len > 0) {
	ret = bgp_attr_parse(peer, &attr, pkt, attribute_len, &mp_update, &mp_withdraw);
	if (ret < 0) return ret;
    pkt += attribute_len;
  }

  update_len = end; end = 0;
  //printf("UPDATE_LEN: %u\n", update_len);

  if (update_len > 0) {
	update.afi = AFI_IP;
	update.safi = SAFI_UNICAST;
	update.nlri = pkt;
	update.length = update_len;
  }
	
  if (withdraw.length) bgp_nlri_parse(peer, NULL, &withdraw);

  /* NLRI parsing */
  if (update.length) 
	bgp_nlri_parse(peer, &attr, &update);
	
  if (mp_update.length
	  && mp_update.afi == AFI_IP
	  && mp_update.safi == SAFI_UNICAST)
	bgp_nlri_parse(peer, &attr, &mp_update);

  if (mp_withdraw.length
	  && mp_withdraw.afi == AFI_IP
	  && mp_withdraw.safi == SAFI_UNICAST)
	bgp_nlri_parse (peer, NULL, &mp_withdraw);

  if (mp_update.length
	  && mp_update.afi == AFI_IP6
	  && mp_update.safi == SAFI_UNICAST)
	bgp_nlri_parse(peer, &attr, &mp_update);

  if (mp_withdraw.length
	  && mp_withdraw.afi == AFI_IP6
	  && mp_withdraw.safi == SAFI_UNICAST)
	bgp_nlri_parse (peer, NULL, &mp_withdraw);

  /* Receipt of End-of-RIB can be processed here; being a silent
	 BGP receiver only, honestly it doesn't matter to us */

  /* Everything is done.  We unintern temporary structures which
	 interned in bgp_attr_parse(). */
  if (attr.aspath)
	aspath_unintern(attr.aspath);
  if (attr.community)
	community_unintern(attr.community);
  if (attr.ecommunity)
	ecommunity_unintern(attr.ecommunity);

  return 0;
}

/* BGP UPDATE Attribute parsing */
int bgp_attr_parse(struct bgp_peer *peer, struct bgp_attr *attr, char *ptr, int len, struct bgp_nlri *mp_update, struct bgp_nlri *mp_withdraw)
{
  int to_the_end = len, ret;
  u_int8_t flag, type, *tmp, mp_nlri = 0;
  u_int16_t tmp16, attr_len;
  struct aspath *as4_path = NULL;

  while (to_the_end > 0) {
	if (to_the_end < BGP_ATTR_MIN_LEN) return -1;

	tmp = (u_int8_t *) ptr++; to_the_end--; flag = *tmp;
	tmp = (u_int8_t *) ptr++; to_the_end--; type = *tmp;

    /* Attribute length */
	if (flag & BGP_ATTR_FLAG_EXTLEN) {
	  memcpy(&tmp16, ptr, 2); ptr += 2; to_the_end -= 2; attr_len = ntohs(tmp16);
	  if (attr_len > to_the_end) return -1;
	}
	else {
	  tmp = (u_int8_t *) ptr++; to_the_end--; attr_len = *tmp;
	  if (attr_len > to_the_end) return -1;
	}

	switch (type) {
	case BGP_ATTR_AS_PATH:
		// printf("ATTRIBUTE: AS PATH\n");
		ret = bgp_attr_parse_aspath(peer, attr_len, attr, ptr, flag);
		break;
	case BGP_ATTR_AS4_PATH:
		// printf("ATTRIBUTE: AS4 PATH\n");
		ret = bgp_attr_parse_as4path(peer, attr_len, attr, ptr, flag, &as4_path);
		break;
	case BGP_ATTR_NEXT_HOP:
		// printf("ATTRIBUTE: NEXHOP\n");
		ret = bgp_attr_parse_nexthop(peer, attr_len, attr, ptr, flag);
	case BGP_ATTR_COMMUNITIES:
		// printf("ATTRIBUTE: COMMUNITIES\n");
		ret = bgp_attr_parse_community(peer, attr_len, attr, ptr, flag);
		break;
	case BGP_ATTR_EXT_COMMUNITIES:
		// printf("ATTRIBUTE: EXTENDED COMMUNITIES\n");
		ret = bgp_attr_parse_ecommunity(peer, attr_len, attr, ptr, flag);
		break;
	case BGP_ATTR_MULTI_EXIT_DISC:
		// printf("ATTRIBUTE: MED\n");
		ret = bgp_attr_parse_med(peer, attr_len, attr, ptr, flag);
		break;
	case BGP_ATTR_LOCAL_PREF:
		// printf("ATTRIBUTE: LOCAL PREFERENCE\n");
		ret = bgp_attr_parse_local_pref(peer, attr_len, attr, ptr, flag);
		break;
	case BGP_ATTR_MP_REACH_NLRI:
		// printf("ATTRIBUTE: MP REACH NLRI\n");
		ret = bgp_attr_parse_mp_reach(peer, attr_len, attr, ptr, mp_update);
		mp_nlri = TRUE;
		break;
	case BGP_ATTR_MP_UNREACH_NLRI:
		// printf("ATTRIBUTE: MP UNREACH NLRI\n");
		ret = bgp_attr_parse_mp_unreach(peer, attr_len, attr, ptr, mp_withdraw);
		mp_nlri = TRUE;
		break;
	default:
		// printf("ATTRIBUTE: UNKNOWN (%u)\n", type);
		ret = 0;
		break;
	}

	if (ret < 0) return ret; 

	if (!mp_nlri) {
	  ptr += attr_len;
	  to_the_end -= attr_len;
	}
	else {
	  ptr += to_the_end;
	  to_the_end = 0;
	}
  }

  if (as4_path) {
	/* AS_PATH and AS4_PATH merge up */
    ret = bgp_attr_munge_as4path(peer, attr, as4_path);

  /* AS_PATH and AS4_PATH info are now fully merged;
	 hence we can free up temporary structures. */
    aspath_unintern(as4_path);
	
	if (ret < 0) return ret;
  }

  return 0;
}

int bgp_attr_parse_aspath(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_int8_t flag)
{
  u_int8_t cap_4as = peer->cap_4as ? 1 : 0;

  attr->aspath = aspath_parse(ptr, len, cap_4as);

  return 0;
}

int bgp_attr_parse_as4path(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_int8_t flag, struct aspath **aspath4)
{
  *aspath4 = aspath_parse(ptr, len, 1);

  return 0;
}

int bgp_attr_parse_nexthop(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_char flag)
{
  u_int32_t tmp;

  /* Length check. */
  if (len != 4) return -1;

  memcpy(&tmp, ptr, 4);
  attr->nexthop.s_addr = tmp;
  ptr += 4;

  return 0;
}

int bgp_attr_parse_community(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_int8_t flag)
{
  if (len == 0) {
	attr->community = NULL;
	return 0;
  }
  else attr->community = (struct community *) community_parse(ptr, len);

  return 0;
}

int bgp_attr_parse_ecommunity(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_int8_t flag)
{
  if (len == 0) attr->ecommunity = NULL;
  else attr->ecommunity = (struct ecommunity *) ecommunity_parse(ptr, len);

  return 0;
}

/* MED atrribute. */
int bgp_attr_parse_med(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_char flag)
{
  u_int32_t tmp;

  /* Length check. */
  if (len != 4) return -1;

  memcpy(&tmp, ptr, 4);
  attr->med = ntohl(tmp);
  ptr += 4;

  return 0;
}

/* Local preference attribute. */
int bgp_attr_parse_local_pref(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, u_char flag)
{
  u_int32_t tmp;

  /* If it is contained in an UPDATE message that is received from an
	 external peer, then this attribute MUST be ignored by the receiving
	 speaker. */
  if (peer->as != config.nfacctd_bgp_myas) return 0;

  if (len != 4) return -1;

  memcpy(&tmp, ptr, 4);
  attr->local_pref = ntohl(tmp);
  ptr += 4;

  return 0;
}

int bgp_attr_parse_mp_reach(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, struct bgp_nlri *mp_update)
{
  u_int16_t afi, tmp16, mpreachlen, mpnhoplen;
  u_int16_t nlri_len;
  u_char safi;

  /* length check */
#define BGP_MP_REACH_MIN_SIZE 5
  if (len < BGP_MP_REACH_MIN_SIZE) return -1;

  mpreachlen = len;
  memcpy(&tmp16, ptr, 2); afi = ntohs(tmp16); ptr += 2;
  safi = *ptr; ptr++;
  mpnhoplen = *ptr; ptr++;
  mpreachlen -= 4; /* 2+1+1 above */ 
  
  /* IPv4, RD+IPv4, IPv6, IPv6 link-local+IPv6 global */
  if (mpnhoplen == 4 || mpnhoplen == 12 || mpnhoplen == 16 || mpnhoplen == 32) {
	if (mpreachlen > mpnhoplen) {
	  switch (mpnhoplen) {
	  case 4:
	    attr->mp_nexthop.family = AF_INET;
	    memcpy(&attr->mp_nexthop.address.ipv4, ptr, 4); 
	    break;
#if defined ENABLE_IPV6
	  case 16:
	    attr->mp_nexthop.family = AF_INET6;
	    memcpy(&attr->mp_nexthop.address.ipv6, ptr, 16); 
	    break;
#endif
	  default:
	    memset(&attr->mp_nexthop, 0, sizeof(struct host_addr));
	    break;
	  }

	  mpreachlen -= mpnhoplen;
	  ptr += mpnhoplen;

	  /* Skipping SNPA info */
	  mpreachlen--; ptr++;
	}
	else return -1;
  }
  else return -1;

  nlri_len = mpreachlen;

  /* length check once again */
  if (!nlri_len || nlri_len > len) return -1;

  /* XXX: perhaps sanity check (applies to: mp_reach, mp_unreach, update, withdraw) */

  mp_update->afi = afi;
  mp_update->safi = safi;
  mp_update->nlri = ptr;
  mp_update->length = nlri_len;

  return 0;
}

int bgp_attr_parse_mp_unreach(struct bgp_peer *peer, u_int16_t len, struct bgp_attr *attr, char *ptr, struct bgp_nlri *mp_withdraw)
{
  u_int16_t afi, mpunreachlen, tmp16;
  u_int16_t withdraw_len;
  u_char safi;

  /* length check */
#define BGP_MP_UNREACH_MIN_SIZE 3
  if (len < BGP_MP_UNREACH_MIN_SIZE) return -1;

  mpunreachlen = len;
  memcpy(&tmp16, ptr, 2); afi = ntohs(tmp16); ptr += 2;
  safi = *ptr; ptr++;
  mpunreachlen -= 3; /* 2+1 above */

  withdraw_len = mpunreachlen;

  mp_withdraw->afi = afi;
  mp_withdraw->safi = safi;
  mp_withdraw->nlri = ptr;
  mp_withdraw->length = withdraw_len;

  return 0;
}


/* BGP UPDATE NLRI parsing */
int bgp_nlri_parse(struct bgp_peer *peer, void *attr, struct bgp_nlri *info)
{
  u_char *pnt;
  u_char *lim;
  struct prefix p;
  int psize, end;
  int ret;

  memset (&p, 0, sizeof (struct prefix));

  pnt = info->nlri;
  lim = pnt + info->length;
  end = info->length;

  for (; pnt < lim; pnt += psize) {

	memset(&p, 0, sizeof(struct prefix));

	/* Fetch prefix length and cross-check */
	p.prefixlen = *pnt++; end--;
	p.family = bgp_afi2family (info->afi);
	
	if ((info->afi == AFI_IP && p.prefixlen > 32) || (info->afi == AFI_IP6 && p.prefixlen > 128)) return -1;

	psize = ((p.prefixlen+7)/8);
	if (psize > end) return -1;

	/* Fetch prefix from NLRI packet. */
	memcpy(&p.u.prefix, pnt, psize);

	// XXX: check address correctnesss now that we have it?
	
    /* Let's do our job now! */
	if (attr)
	  ret = bgp_process_update(peer, &p, attr, info->afi, info->safi);
	else
	  ret = bgp_process_withdraw(peer, &p, attr, info->afi, info->safi);
  }

  return 0;
}

int bgp_process_update(struct bgp_peer *peer, struct prefix *p, void *attr, afi_t afi, safi_t safi)
{
  struct bgp_node *route;
  struct bgp_info *ri, *new;
  struct bgp_attr *attr_new;

  route = bgp_node_get(peer->rib[afi][safi], p);

  /* Check previously received route. */
  for (ri = route->info; ri; ri = ri->next)
	if (ri->peer == peer && ri->type == afi && ri->sub_type == safi)
	  break;

  attr_new = bgp_attr_intern(attr);

  if (ri) {
	ri->uptime = time(NULL);

	/* Received same information */
	if (attrhash_cmp(ri->attr, attr_new)) {
	  bgp_unlock_node (route);
	  bgp_attr_unintern(attr_new);

	  return 0;
	}
	else {
	  /* Update to new attribute.  */
	  bgp_attr_unintern(ri->attr);
	  ri->attr = attr_new;
	  bgp_unlock_node (route);

	  if (config.nfacctd_bgp_msglog)
		goto log_update;

	  return 0;
	}
  }

  /* Make new BGP info. */
  new = bgp_info_new();
  new->type = afi;
  new->sub_type = safi;
  new->peer = peer;
  new->attr = attr_new;
  new->uptime = time(NULL);

  /* Register new BGP information. */
  bgp_info_add(route, new);

  /* route_node_get lock */
  bgp_unlock_node(route);

  if (config.nfacctd_bgp_msglog)
    goto log_update;

  /* XXX: Impose a maximum number of prefixes allowed */
  // if (bgp_maximum_prefix_overflow(peer, afi, safi, 0))
  // return -1;

  return 0;

log_update:
  {
    char empty[] = "";
	char prefix_str[INET6_ADDRSTRLEN];
	char *aspath, *comm, *ecomm; 

    memset(prefix_str, 0, INET6_ADDRSTRLEN);
	prefix2str(&route->p, prefix_str, INET6_ADDRSTRLEN);

	aspath = attr_new->aspath ? attr_new->aspath->str : empty;
	comm = attr_new->community ? attr_new->community->str : empty;
	ecomm = attr_new->ecommunity ? attr_new->ecommunity->str : empty;

	Log(LOG_INFO, "INFO ( default/core/BGP ): u Prefix: '%s' Path: '%s' Comms: '%s' EComms: '%s'\n", prefix_str, aspath, comm, ecomm);
  }

  return 0;
}

int bgp_process_withdraw(struct bgp_peer *peer, struct prefix *p, void *attr, afi_t afi, safi_t safi)
{
  struct bgp_node *route;
  struct bgp_info *ri;

  /* Lookup node. */
  route = bgp_node_get(peer->rib[afi][safi], p);

  /* Lookup withdrawn route. */
  for (ri = route->info; ri; ri = ri->next)
	if (ri->peer == peer && ri->type == afi && ri->sub_type == safi)
	  break;

  if (ri && config.nfacctd_bgp_msglog) {
	char empty[] = "";
	char prefix_str[INET6_ADDRSTRLEN];
	char *aspath, *comm, *ecomm;

    memset(prefix_str, 0, INET6_ADDRSTRLEN);
	prefix2str(&route->p, prefix_str, INET6_ADDRSTRLEN);

	aspath = ri->attr->aspath ? ri->attr->aspath->str : empty;
	comm = ri->attr->community ? ri->attr->community->str : empty;
	ecomm = ri->attr->ecommunity ? ri->attr->ecommunity->str : empty;

	Log(LOG_INFO, "INFO ( default/core/BGP ): w Prefix: %s Path: '%s' Comms: '%s' EComms: '%s'\n", prefix_str, aspath, comm, ecomm);
  }

  /* Withdraw specified route from routing table. */
  if (ri) bgp_info_delete(route, ri); 

  /* Unlock bgp_node_get() lock. */
  bgp_unlock_node(route);

  return 0;
}

/* BGP Address Famiy Identifier to UNIX Address Family converter. */
int bgp_afi2family (int afi)
{
  if (afi == AFI_IP)
	return AF_INET;
#ifdef ENABLE_IPV6
  else if (afi == AFI_IP6)
	return AF_INET6;
#endif 
  return 0;
}

/* Allocate new bgp info structure. */
struct bgp_info *bgp_info_new()
{
  struct bgp_info *new;

  new = malloc(sizeof(struct bgp_info));
  memset(new, 0, sizeof (struct bgp_info));
  
  return new;
}

void bgp_info_add(struct bgp_node *rn, struct bgp_info *ri)
{
  struct bgp_info *top;

  top = rn->info;

  ri->next = rn->info;
  ri->prev = NULL;
  if (top)
	top->prev = ri;
  rn->info = ri;

  ri->lock++;
  bgp_lock_node(rn);
  ri->peer->lock++;
}

void bgp_info_delete(struct bgp_node *rn, struct bgp_info *ri)
{
  if (ri->next)
	ri->next->prev = ri->prev;
  if (ri->prev)
	ri->prev->next = ri->next;
  else
	rn->info = ri->next;

  assert (ri->lock > 0);

  ri->lock--;
  if (ri->lock == 0) bgp_info_free(ri);

  bgp_unlock_node(rn);
}

/* Free bgp route information. */
void bgp_info_free(struct bgp_info *ri)
{
  if (ri->attr)
	bgp_attr_unintern (ri->attr);

  ri->peer->lock--;
  free(ri);
}

/* Initialization of attributes */
void bgp_attr_init()
{
  aspath_init();
  attrhash_init();
  community_init();
  ecommunity_init();
}

unsigned int attrhash_key_make(void *p)
{
  struct bgp_attr *attr = (struct bgp_attr *) p;
  unsigned int key = 0;

  key += attr->origin;
  key += attr->nexthop.s_addr;
  key += attr->med;
  key += attr->local_pref;
  if (attr->pathlimit.as)
    {
      key += attr->pathlimit.ttl;
      key += attr->pathlimit.as;
    }

  if (attr->aspath)
    key += aspath_key_make(attr->aspath);
  if (attr->community)
    key += community_hash_make(attr->community);
  if (attr->ecommunity)
    key += ecommunity_hash_make(attr->ecommunity);

  return key;
}

int attrhash_cmp(void *p1,void *p2)
{
  struct bgp_attr *attr1 = p1;
  struct bgp_attr *attr2 = p2;

  if (attr1->flag == attr2->flag
      && attr1->origin == attr2->origin
      && attr1->nexthop.s_addr == attr2->nexthop.s_addr
      && attr1->aspath == attr2->aspath
      && attr1->community == attr2->community
      && attr1->ecommunity == attr2->ecommunity
      && attr1->med == attr2->med
      && attr1->local_pref == attr2->local_pref
      && attr1->pathlimit.ttl == attr2->pathlimit.ttl
      && attr1->pathlimit.as == attr2->pathlimit.as)
    return 1;
  else
    return 0;
}

void attrhash_init()
{
  attrhash = (struct hash *) hash_create(attrhash_key_make, attrhash_cmp);
}

/* Internet argument attribute. */
struct bgp_attr *bgp_attr_intern(struct bgp_attr *attr)
{
  struct bgp_attr *find;
 
  /* Intern referenced strucutre. */
  if (attr->aspath) {
    if (! attr->aspath->refcnt)
      attr->aspath = aspath_intern (attr->aspath);
  else
	  attr->aspath->refcnt++;
  }
  if (attr->community) {
	if (! attr->community->refcnt)
	  attr->community = community_intern (attr->community);
	else
	  attr->community->refcnt++;
  }
  if (attr->ecommunity) {
 	if (!attr->ecommunity->refcnt)
	  attr->ecommunity = ecommunity_intern (attr->ecommunity);
  else
	attr->ecommunity->refcnt++;
  }
 
  find = (struct bgp_attr *) hash_get(attrhash, attr, bgp_attr_hash_alloc);
  find->refcnt++;

  return find;
}

/* Free bgp attribute and aspath. */
void bgp_attr_unintern(struct bgp_attr *attr)
{
  struct bgp_attr *ret;
  struct aspath *aspath;
  struct community *community;
  struct ecommunity *ecommunity = NULL;
 
  /* Decrement attribute reference. */
  attr->refcnt--;
  aspath = attr->aspath;
  community = attr->community;
  ecommunity = attr->ecommunity;

  /* If reference becomes zero then free attribute object. */
  if (attr->refcnt == 0) {
	ret = (struct bgp_attr *) hash_release (attrhash, attr);
	assert (ret != NULL);
	if (ret) free(attr);
  }

  /* aspath refcount shoud be decrement. */
  if (aspath)
	aspath_unintern (aspath);
  if (community)
	community_unintern (community);
  if (ecommunity)
	ecommunity_unintern (ecommunity);
}

void *bgp_attr_hash_alloc (void *p)
{
  struct bgp_attr *val = (struct bgp_attr *) p;
  struct bgp_attr *attr;

  attr = malloc(sizeof (struct bgp_attr));
  *attr = *val;
  attr->refcnt = 0;

  return attr;
}

void bgp_peer_init(struct bgp_peer *peer)
{
  afi_t afi;
  safi_t safi;

  memset(peer, 0, sizeof(struct bgp_peer));
  peer->status = Idle;
  peer->buf.len = BGP_MAX_PACKET_SIZE;
  peer->buf.base = malloc(peer->buf.len);
  memset(peer->buf.base, 0, peer->buf.len);

  /* Let's initialize clean RIBs once again */
  for (afi = AFI_IP; afi < AFI_MAX; afi++) {
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++) {
      peer->rib[afi][safi] = bgp_table_init(afi, safi);
    }
  }
}

void bgp_peer_close(struct bgp_peer *peer)
{
  afi_t afi;
  safi_t safi;

  close(peer->fd);
  peer->fd = 0;
  memset(&peer->id, 0, sizeof(peer->id));
  memset(&peer->addr, 0, sizeof(peer->addr));

  /* Let's fully invalidate current RIBs first */
  for (afi = AFI_IP; afi < AFI_MAX; afi++) {
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++) {
      bgp_table_finish(&peer->rib[afi][safi]);
    }
  }

  free(peer->buf.base);
}

int bgp_attr_munge_as4path(struct bgp_peer *peer, struct bgp_attr *attr, struct aspath *as4path)
{
  struct aspath *newpath;

  /* If the BGP peer supports 32bit AS_PATH then we are done */ 
  if (peer->cap_4as) return 0;

  /* pre-requisite for AS4_PATH is AS_PATH indeed */ 
  // XXX if (as4path && !attr->aspath) return -1;

  newpath = aspath_reconcile_as4(attr->aspath, as4path);
  aspath_unintern(attr->aspath);
  attr->aspath = aspath_intern(newpath);
}

void load_comm_patterns(char **stdcomm, char **extcomm)
{
  int idx;
  char *token;

  memset(std_comm_patterns, 0, sizeof(std_comm_patterns));
  memset(ext_comm_patterns, 0, sizeof(ext_comm_patterns));

  if (*stdcomm) {
    idx = 0;
    while ( (token = extract_token(stdcomm, ',')) && idx < MAX_BGP_COMM_PATTERNS ) {
      std_comm_patterns[idx] = token;
      trim_spaces(std_comm_patterns[idx]);
      idx++;
    }
  }
 
  if (*extcomm) {
    idx = 0;
    while ( (token = extract_token(extcomm, ',')) && idx < MAX_BGP_COMM_PATTERNS ) {
      ext_comm_patterns[idx] = token;
      trim_spaces(ext_comm_patterns[idx]);
      idx++;
    }
  }
} 

void evaluate_comm_patterns(char *dst, char *src, char **patterns, int dstlen)
{
  char *ptr, *haystack;
  int idx, i, j, srclen;

  srclen = strlen(src);

  for (idx = 0, j = 0; patterns[idx]; idx++) {
    haystack = src;

    find_again:
    ptr = strstr(haystack, patterns[idx]);

    if (ptr) {
      /* If we have already something on the stack, let's insert a space */
      if (j && j < dstlen) {
	dst[j] = ' ';
	j++;
      }

      /* We should be able to trust this string */
      for (i = 0; ptr[i] != ' ' && ptr[i] != '\0'; i++, j++) {
	if (j < dstlen) dst[j] = ptr[i];
	else break;
      } 

      haystack = &ptr[i];
    }

    /* If we don't have space anymore, let's finish it here */
    if (j >= dstlen) {
      dst[dstlen-1] = '+';
      break;
    }

    /* Trick to find multiple occurrences */ 
    if (ptr) goto find_again;
  }
}

as_t evaluate_last_asn(char *src)
{
  int idx, len = strlen(src); 
  char *endptr;
  u_int32_t asn;
  
  for (idx = len; idx && src[idx] != ' '; idx--);

  asn = strtoul(&src[idx], &endptr, 10);

  return asn;
}

as_t evaluate_first_asn(char *src)
{
  int idx, is_space = FALSE, len = strlen(src);
  char *endptr;
  u_int32_t asn;

  for (idx = 0; idx < len && src[idx] != ' '; idx++);

  /* Mangling the AS_PATH string */
  if (src[idx] == ' ') {
    is_space = TRUE;  
    src[idx] = '\0';
  }

  asn = strtoul(src, &endptr, 10);

  /* Restoring mangled AS_PATH */
  if (is_space) src[idx] = ' '; 

  return asn;
}
