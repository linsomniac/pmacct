/*
 * Copyright 2002 Damien Miller <djm@mindrot.org> All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id$ */

#include "common.h"
#include "treetype.h"
#include "nfprobe_plugin.h"

RCSID("$Id$");

/*
 * This is the Cisco Netflow(tm) version 5 packet format
 * Based on:
 * http://www.cisco.com/univercd/cc/td/doc/product/rtrmgmt/nfc/nfc_3_0/nfc_ug/nfcform.htm
 */
struct NF5_HEADER {
	u_int16_t version, flows;
	u_int32_t uptime_ms, time_sec, time_nanosec, flow_sequence;
	u_int8_t engine_type, engine_id;
	u_int16_t sampling;
};
struct NF5_FLOW {
	u_int32_t src_ip, dest_ip, nexthop_ip;
	u_int16_t if_index_in, if_index_out;
	u_int32_t flow_packets, flow_octets;
	u_int32_t flow_start, flow_finish;
	u_int16_t src_port, dest_port;
	u_int8_t pad1;
	u_int8_t tcp_flags, protocol, tos;
	u_int16_t src_as, dest_as;
	u_int8_t src_mask, dst_mask;
	u_int16_t pad2;
};
#define NF5_MAXFLOWS		30
#define NF5_MAXPACKET_SIZE	(sizeof(struct NF5_HEADER) + \
				 (NF5_MAXFLOWS * sizeof(struct NF5_FLOW)))

/*
 * Given an array of expired flows, send netflow v5 report packets
 * Returns number of packets sent or -1 on error
 */
int
send_netflow_v5(struct FLOW **flows, int num_flows, int nfsock,
    u_int64_t *flows_exported, struct timeval *system_boot_time,
    int verbose_flag, u_int8_t engine_type, u_int8_t engine_id)
{
	struct timeval now;
	u_int32_t uptime_ms;
	u_int8_t packet[NF5_MAXPACKET_SIZE];	/* Maximum allowed packet size (24 flows) */
	struct NF5_HEADER *hdr = NULL;
	struct NF5_FLOW *flw = NULL;
	int i, j, offset, num_packets, err;
	socklen_t errsz;
	
	gettimeofday(&now, NULL);
	uptime_ms = timeval_sub_ms(&now, system_boot_time);

	hdr = (struct NF5_HEADER *)packet;
	for(num_packets = offset = j = i = 0; i < num_flows; i++) {
		if (j >= NF5_MAXFLOWS - 1) {
			if (verbose_flag)
				Log(LOG_DEBUG, "Sending flow packet len = %d\n", offset);
			hdr->flows = htons(hdr->flows);
			errsz = sizeof(err);
			getsockopt(nfsock, SOL_SOCKET, SO_ERROR,
			    &err, &errsz); /* Clear ICMP errors */
			if (send(nfsock, packet, (size_t)offset, 0) == -1)
				return (-1);
			*flows_exported += j;
			j = 0;
			num_packets++;
		}
		if (j == 0) {
			memset(&packet, '\0', sizeof(packet));
			hdr->version = htons(5);
			hdr->flows = 0; /* Filled in as we go */
			hdr->uptime_ms = htonl(uptime_ms);
			hdr->time_sec = htonl(now.tv_sec);
			hdr->time_nanosec = htonl(now.tv_usec * 1000);
			hdr->flow_sequence = htonl(*flows_exported);
			hdr->engine_type = engine_type;
			hdr->engine_id = engine_id;
			hdr->sampling = htons(config.sampling_rate & 0x3FFF); 
			if (hdr->sampling) hdr->sampling |= (htons(1) >> 2);
			offset = sizeof(*hdr);
		}		
		flw = (struct NF5_FLOW *)(packet + offset);

		/* NetFlow v.5 doesn't do IPv6 */
		if (flows[i]->af != AF_INET)
			continue;
		if (flows[i]->octets[0] > 0) {
			flw->src_ip = flows[i]->addr[0].v4.s_addr;
			flw->dest_ip = flows[i]->addr[1].v4.s_addr;
			flw->src_port = flows[i]->port[0];
			flw->dest_port = flows[i]->port[1];
			{
			  as_t tmp_as;

			  tmp_as = ntohl(flows[i]->as[0]);
			  if (tmp_as > 65535) flw->src_as = htons(23456);
			  else flw->src_as = tmp_as;

			  tmp_as = ntohl(flows[i]->as[1]);
			  if (tmp_as > 65535) flw->dest_as = htons(23456);
			  else flw->dest_as = htons(tmp_as);
			}
			flw->flow_packets = htonl(flows[i]->packets[0]);
			flw->flow_octets = htonl(flows[i]->octets[0]);
			flw->flow_start =
			    htonl(timeval_sub_ms(&flows[i]->flow_start,
			    system_boot_time));
			flw->flow_finish =
			    htonl(timeval_sub_ms(&flows[i]->flow_last,
			    system_boot_time));
			flw->tcp_flags = flows[i]->tcp_flags[0];
			flw->protocol = flows[i]->protocol;
			flw->tos = flows[i]->tos[0];
			offset += sizeof(*flw);
			j++;
			hdr->flows++;
		}
		flw = (struct NF5_FLOW *)(packet + offset);

		if (flows[i]->octets[1] > 0) {
			flw->src_ip = flows[i]->addr[1].v4.s_addr;
			flw->dest_ip = flows[i]->addr[0].v4.s_addr;
			flw->src_port = flows[i]->port[1];
			flw->dest_port = flows[i]->port[0];
			{
			  as_t tmp_as;
			  
			  tmp_as = ntohl(flows[i]->as[1]);
			  if (tmp_as > 65535) flw->src_as = htons(23456);
			  else flw->src_as = htons(tmp_as);

			  tmp_as = ntohl(flows[i]->as[0]);
			  if (tmp_as > 65535) flw->dest_as = htons(23456);
			  else flw->dest_as = htons(tmp_as);
			}
			flw->flow_packets = htonl(flows[i]->packets[1]);
			flw->flow_octets = htonl(flows[i]->octets[1]);
			flw->flow_start =
			    htonl(timeval_sub_ms(&flows[i]->flow_start,
			    system_boot_time));
			flw->flow_finish =
			    htonl(timeval_sub_ms(&flows[i]->flow_last,
			    system_boot_time));
			flw->tcp_flags = flows[i]->tcp_flags[1];
			flw->protocol = flows[i]->protocol;
			flw->tos = flows[i]->tos[1];
			offset += sizeof(*flw);
			j++;
			hdr->flows++;
		}
	}

	/* Send any leftovers */
	if (j != 0) {
		if (verbose_flag)
			Log(LOG_DEBUG, "Sending v5 flow packet len = %d\n",
			    offset);
		hdr->flows = htons(hdr->flows);
		errsz = sizeof(err);
		getsockopt(nfsock, SOL_SOCKET, SO_ERROR,
		    &err, &errsz); /* Clear ICMP errors */
		if (send(nfsock, packet, (size_t)offset, 0) == -1)
			return (-1);
		num_packets++;
	}

	*flows_exported += j;
	return (num_packets);
}

