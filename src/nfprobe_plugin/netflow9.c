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

#define __NFPROBE_NETFLOW9_C

#include "common.h"
#include "treetype.h"
#include "nfprobe_plugin.h"
#include "ip_flow.h"
#include "classifier.h"

/* Netflow v.9 */
struct NF9_HEADER {
	u_int16_t version, flows;
	u_int32_t uptime_ms, time_sec;
	u_int32_t package_sequence, source_id;
} __packed;
struct NF9_FLOWSET_HEADER_COMMON {
	u_int16_t flowset_id, length;
} __packed;
struct NF9_TEMPLATE_FLOWSET_HEADER {
	struct NF9_FLOWSET_HEADER_COMMON c;
	u_int16_t template_id, count;
} __packed;
struct NF9_OPTIONS_TEMPLATE_FLOWSET_HEADER {
        struct NF9_FLOWSET_HEADER_COMMON c;
        u_int16_t template_id, scope_len;
        u_int16_t option_len;
} __packed;
struct NF9_TEMPLATE_FLOWSET_RECORD {
	u_int16_t type, length;
} __packed;
struct NF9_DATA_FLOWSET_HEADER {
	struct NF9_FLOWSET_HEADER_COMMON c;
} __packed;
#define NF9_TEMPLATE_FLOWSET_ID		0
#define NF9_OPTIONS_FLOWSET_ID		1
#define NF9_MIN_RECORD_FLOWSET_ID	256

/* Flowset record types the we care about */
#define NF9_IN_BYTES			1
#define NF9_IN_PACKETS			2
#define NF9_FLOWS			3
#define NF9_IN_PROTOCOL			4
#define NF9_SRC_TOS                     5
#define NF9_TCP_FLAGS			6
#define NF9_L4_SRC_PORT			7
#define NF9_IPV4_SRC_ADDR		8
/* ... */
#define NF9_INPUT_SNMP                  10
/* ... */
#define NF9_L4_DST_PORT			11
#define NF9_IPV4_DST_ADDR		12
/* ... */
#define NF9_OUTPUT_SNMP                 14
/* ... */
#define NF9_SRC_AS                      16
#define NF9_DST_AS                      17
/* ... */
#define NF9_LAST_SWITCHED		21
#define NF9_FIRST_SWITCHED		22
/* ... */
#define NF9_IPV6_SRC_ADDR		27
#define NF9_IPV6_DST_ADDR		28
/* ... */
#define NF9_FLOW_SAMPLER_ID             48
#define NF9_FLOW_SAMPLER_MODE           49
#define NF9_FLOW_SAMPLER_INTERVAL       50
#define NF9_SRC_MAC                     56
#define NF9_DST_MAC                     57
#define NF9_SRC_VLAN                    58
/* ... */
#define NF9_IP_PROTOCOL_VERSION		60
/* ... */
#define NF9_MPLS_LABEL_1                70
/* CUSTOM TYPES START HERE */
#define NF9_CUST_CLASS			200
#define NF9_CUST_TAG			201
#define NF9_CUST_TAG2			202
/* CUSTOM TYPES END HERE */

/* OPTION SCOPES */
#define NF9_OPT_SCOPE_SYSTEM            1

/* Stuff pertaining to the templates that softflowd uses */
#define NF9_SOFTFLOWD_TEMPLATE_NRECORDS	30
struct NF9_SOFTFLOWD_TEMPLATE {
	struct NF9_TEMPLATE_FLOWSET_HEADER h;
	struct NF9_TEMPLATE_FLOWSET_RECORD r[NF9_SOFTFLOWD_TEMPLATE_NRECORDS];
	u_int16_t tot_len;
} __packed;

#define NF9_OPTIONS_TEMPLATE_NRECORDS 4
struct NF9_OPTIONS_TEMPLATE {
        struct NF9_OPTIONS_TEMPLATE_FLOWSET_HEADER h;
        struct NF9_TEMPLATE_FLOWSET_RECORD r[NF9_OPTIONS_TEMPLATE_NRECORDS];
        u_int16_t tot_len;
} __packed;

typedef void (*flow_to_flowset_handler) (char *, const struct FLOW *, int, int);
struct NF9_INTERNAL_TEMPLATE_RECORD {
  flow_to_flowset_handler handler;
  u_int16_t length;
};

struct NF9_INTERNAL_TEMPLATE {
	struct NF9_INTERNAL_TEMPLATE_RECORD r[NF9_SOFTFLOWD_TEMPLATE_NRECORDS];
	u_int16_t tot_rec_len;
};

struct NF9_INTERNAL_OPTIONS_TEMPLATE {
        struct NF9_INTERNAL_TEMPLATE_RECORD r[NF9_OPTIONS_TEMPLATE_NRECORDS];
        u_int16_t tot_rec_len;
};

/* softflowd data flowset types */
struct NF9_SOFTFLOWD_DATA_COMMON {
	u_int32_t last_switched, first_switched;
	u_int16_t ifindex_in, ifindex_out;
	u_int32_t bytes, packets, flows;
	u_int16_t src_port, dst_port;
	u_int8_t protocol, tos, tcp_flags, ipproto;
	as_t src_as, dst_as;
	u_int8_t src_mac[6], dst_mac[6];
	u_int16_t vlan;
} __packed;

struct NF9_SOFTFLOWD_DATA_V4 {
	u_int32_t src_addr, dst_addr;
	struct NF9_SOFTFLOWD_DATA_COMMON c;
} __packed;

struct NF9_SOFTFLOWD_DATA_V6 {
	u_int8_t src_addr[16], dst_addr[16];
	struct NF9_SOFTFLOWD_DATA_COMMON c;
} __packed;

/* Local data: templates and counters */
#define NF9_SOFTFLOWD_MAX_PACKET_SIZE	512
#define NF9_SOFTFLOWD_V4_TEMPLATE_ID	1024
#define NF9_SOFTFLOWD_V6_TEMPLATE_ID	2048
#define NF9_OPTIONS_TEMPLATE_ID		4096

#define NF9_DEFAULT_TEMPLATE_INTERVAL	18

static struct NF9_SOFTFLOWD_TEMPLATE v4_template;
static struct NF9_INTERNAL_TEMPLATE v4_int_template;
static struct NF9_SOFTFLOWD_TEMPLATE v6_template;
static struct NF9_INTERNAL_TEMPLATE v6_int_template;
static struct NF9_OPTIONS_TEMPLATE options_template;
static struct NF9_INTERNAL_OPTIONS_TEMPLATE options_int_template;
static char ftoft_buf_0[sizeof(struct NF9_SOFTFLOWD_DATA_V6)];
static char ftoft_buf_1[sizeof(struct NF9_SOFTFLOWD_DATA_V6)];

static int nf9_pkts_until_template = -1;
static u_int8_t send_options = FALSE;

static void
flow_to_flowset_input_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int16_t rec16;

  rec16 = htons(flow->ifindex[idx]);
  memcpy(flowset, &rec16, size);
}

static void
flow_to_flowset_output_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int16_t rec16;

  rec16 = htons(flow->ifindex[idx ^ 1]);
  memcpy(flowset, &rec16, size);
}

static void
flow_to_flowset_flows_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int32_t rec32;

  rec32 = htonl(flow->flows[idx]);
  memcpy(flowset, &rec32, size);
}

static void
flow_to_flowset_src_host_v4_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->addr[idx].v4, size);
}

static void
flow_to_flowset_dst_host_v4_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->addr[idx ^ 1].v4, size);
}

static void
flow_to_flowset_src_host_v6_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->addr[idx].v6, size);
}

static void
flow_to_flowset_dst_host_v6_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->addr[idx ^ 1].v6, size);
}

static void
flow_to_flowset_src_port_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->port[idx], size);
}

static void
flow_to_flowset_dst_port_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->port[idx ^ 1], size);
}

static void
flow_to_flowset_ip_tos_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->tos[idx], size);
}

static void
flow_to_flowset_tcp_flags_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->tcp_flags[idx], size);
}

static void
flow_to_flowset_ip_proto_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->protocol, size);
}

static void
flow_to_flowset_src_as_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->as[idx], size);
}

static void
flow_to_flowset_dst_as_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->as[idx ^ 1], size);
}

static void
flow_to_flowset_src_mac_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->mac[idx], size);
}

static void
flow_to_flowset_dst_mac_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->mac[idx ^ 1], size);
}

static void
flow_to_flowset_vlan_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int16_t rec16;

  rec16 = htons(flow->vlan);
  memcpy(flowset, &rec16, size);
}

static void
flow_to_flowset_mpls_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  memcpy(flowset, &flow->mpls_label[idx], size);
}

static void
flow_to_flowset_class_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  char buf[MAX_PROTOCOL_LEN+1];

  memset(buf, 0, MAX_PROTOCOL_LEN+1);
  if (flow->class && class[flow->class-1].id) {
    strlcpy(buf, class[flow->class-1].protocol, MAX_PROTOCOL_LEN);
    buf[sizeof(buf)-1] = '\0';
  }
  else strlcpy(buf, "unknown", MAX_PROTOCOL_LEN);

  memcpy(flowset, buf, size);
}

static void
flow_to_flowset_tag_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int32_t rec32;

  rec32 = htonl(flow->tag[idx]);
  memcpy(flowset, &rec32, size);
}

static void
flow_to_flowset_tag2_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int32_t rec32;

  rec32 = htonl(flow->tag2[idx]);
  memcpy(flowset, &rec32, size);
}

static void
flow_to_flowset_sampler_id_handler(char *flowset, const struct FLOW *flow, int idx, int size)
{
  u_int8_t rec8;

  rec8 = 1;
  memcpy(flowset, &rec8, size);
}

static void
nf9_init_template(void)
{
	int rcount, idx; 

	/* Let's enforce some defaults; if we are launched without an
	 * aggregation, then let's choose one. If we don't have one or
	 * more flow-distinguishing primitives, then let's add flow
	 * aggregation info to the template */
	if ( ! config.nfprobe_what_to_count ) {
	  config.nfprobe_what_to_count |= COUNT_SRC_HOST;
	  config.nfprobe_what_to_count |= COUNT_DST_HOST;
	  config.nfprobe_what_to_count |= COUNT_SRC_PORT;
	  config.nfprobe_what_to_count |= COUNT_DST_PORT;
	  config.nfprobe_what_to_count |= COUNT_IP_PROTO;
	  config.nfprobe_what_to_count |= COUNT_IP_TOS;
	}
/*
	if ( ! ( config.nfprobe_what_to_count & COUNT_SRC_HOST && 
	  config.nfprobe_what_to_count & COUNT_DST_HOST && 
	  config.nfprobe_what_to_count & COUNT_SRC_PORT && 
	  config.nfprobe_what_to_count & COUNT_DST_PORT && 
	  config.nfprobe_what_to_count & COUNT_IP_PROTO && 
	  config.nfprobe_what_to_count & COUNT_IP_TOS ) ) 
	  config.nfprobe_what_to_count |= COUNT_FLOWS;
*/
	
	rcount = 0;
	bzero(&v4_template, sizeof(v4_template));
	bzero(&v4_int_template, sizeof(v4_int_template));

	v4_template.r[rcount].type = htons(NF9_LAST_SWITCHED);
	v4_template.r[rcount].length = htons(4);
	v4_int_template.r[rcount].length = 4;
	rcount++;
	v4_template.r[rcount].type = htons(NF9_FIRST_SWITCHED);
	v4_template.r[rcount].length = htons(4);
	v4_int_template.r[rcount].length = 4;
	rcount++;
	v4_template.r[rcount].type = htons(NF9_IN_BYTES);
	v4_template.r[rcount].length = htons(4);
	v4_int_template.r[rcount].length = 4;
	rcount++;
	v4_template.r[rcount].type = htons(NF9_IN_PACKETS);
	v4_template.r[rcount].length = htons(4);
	v4_int_template.r[rcount].length = 4;
	rcount++;
	v4_template.r[rcount].type = htons(NF9_IP_PROTOCOL_VERSION);
	v4_template.r[rcount].length = htons(1);
	v4_int_template.r[rcount].length = 1;
	rcount++;
        v4_template.r[rcount].type = htons(NF9_INPUT_SNMP);
        v4_template.r[rcount].length = htons(2);
	v4_int_template.r[rcount].handler = flow_to_flowset_input_handler;
        v4_int_template.r[rcount].length = 2;
        rcount++;
        v4_template.r[rcount].type = htons(NF9_OUTPUT_SNMP);
        v4_template.r[rcount].length = htons(2);
	v4_int_template.r[rcount].handler = flow_to_flowset_output_handler;
        v4_int_template.r[rcount].length = 2;
        rcount++;
	if (config.nfprobe_what_to_count & COUNT_FLOWS) { 
	  v4_template.r[rcount].type = htons(NF9_FLOWS);
	  v4_template.r[rcount].length = htons(4);
	  v4_int_template.r[rcount].handler = flow_to_flowset_flows_handler;
	  v4_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_HOST) { 
	  v4_template.r[rcount].type = htons(NF9_IPV4_SRC_ADDR);
	  v4_template.r[rcount].length = htons(4);
	  v4_int_template.r[rcount].handler = flow_to_flowset_src_host_v4_handler;
	  v4_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_HOST) {
	  v4_template.r[rcount].type = htons(NF9_IPV4_DST_ADDR);
	  v4_template.r[rcount].length = htons(4);
	  v4_int_template.r[rcount].handler = flow_to_flowset_dst_host_v4_handler;
	  v4_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_PORT) {
	  v4_template.r[rcount].type = htons(NF9_L4_SRC_PORT);
	  v4_template.r[rcount].length = htons(2);
	  v4_int_template.r[rcount].handler = flow_to_flowset_src_port_handler;
	  v4_int_template.r[rcount].length = 2;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_PORT) {
	  v4_template.r[rcount].type = htons(NF9_L4_DST_PORT);
	  v4_template.r[rcount].length = htons(2);
	  v4_int_template.r[rcount].handler = flow_to_flowset_dst_port_handler;
	  v4_int_template.r[rcount].length = 2;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & (COUNT_IP_TOS)) {
	  v4_template.r[rcount].type = htons(NF9_SRC_TOS);
	  v4_template.r[rcount].length = htons(1);
	  v4_int_template.r[rcount].handler = flow_to_flowset_ip_tos_handler;
	  v4_int_template.r[rcount].length = 1;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & (COUNT_SRC_PORT|COUNT_DST_PORT)) {
	  v4_template.r[rcount].type = htons(NF9_TCP_FLAGS);
	  v4_template.r[rcount].length = htons(1);
	  v4_int_template.r[rcount].handler = flow_to_flowset_tcp_flags_handler;
	  v4_int_template.r[rcount].length = 1;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_IP_PROTO) {
	  v4_template.r[rcount].type = htons(NF9_IN_PROTOCOL);
	  v4_template.r[rcount].length = htons(1);
	  v4_int_template.r[rcount].handler = flow_to_flowset_ip_proto_handler;
	  v4_int_template.r[rcount].length = 1;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_AS) {
	  v4_template.r[rcount].type = htons(NF9_SRC_AS);
	  v4_template.r[rcount].length = htons(4);
	  v4_int_template.r[rcount].handler = flow_to_flowset_src_as_handler;
	  v4_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_AS) {
	  v4_template.r[rcount].type = htons(NF9_DST_AS);
	  v4_template.r[rcount].length = htons(4);
	  v4_int_template.r[rcount].handler = flow_to_flowset_dst_as_handler;
	  v4_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_MAC) {
	  v4_template.r[rcount].type = htons(NF9_SRC_MAC);
	  v4_template.r[rcount].length = htons(6);
	  v4_int_template.r[rcount].handler = flow_to_flowset_src_mac_handler;
	  v4_int_template.r[rcount].length = 6;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_MAC) {
	  v4_template.r[rcount].type = htons(NF9_DST_MAC);
	  v4_template.r[rcount].length = htons(6);
	  v4_int_template.r[rcount].handler = flow_to_flowset_dst_mac_handler;
	  v4_int_template.r[rcount].length = 6;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_VLAN) {
	  v4_template.r[rcount].type = htons(NF9_SRC_VLAN);
	  v4_template.r[rcount].length = htons(2);
	  v4_int_template.r[rcount].handler = flow_to_flowset_vlan_handler;
	  v4_int_template.r[rcount].length = 2;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_VLAN) {
	  v4_template.r[rcount].type = htons(NF9_MPLS_LABEL_1);
	  v4_template.r[rcount].length = htons(3);
	  v4_int_template.r[rcount].handler = flow_to_flowset_mpls_handler;
	  v4_int_template.r[rcount].length = 3;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_CLASS) {
	  v4_template.r[rcount].type = htons(NF9_CUST_CLASS);
	  v4_template.r[rcount].length = htons(16);
	  v4_int_template.r[rcount].handler = flow_to_flowset_class_handler;
	  v4_int_template.r[rcount].length = 16;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_ID) {
	  v4_template.r[rcount].type = htons(NF9_CUST_TAG);
	  v4_template.r[rcount].length = htons(4);
	  v4_int_template.r[rcount].handler = flow_to_flowset_tag_handler;
	  v4_int_template.r[rcount].length = 4;
	  rcount++;
	}
        if (config.nfprobe_what_to_count & COUNT_ID2) {
          v4_template.r[rcount].type = htons(NF9_CUST_TAG2);
          v4_template.r[rcount].length = htons(4);
          v4_int_template.r[rcount].handler = flow_to_flowset_tag2_handler;
          v4_int_template.r[rcount].length = 4;
          rcount++;
        }
        if (config.sampling_rate || config.ext_sampling_rate) {
          v4_template.r[rcount].type = htons(NF9_FLOW_SAMPLER_ID);
          v4_template.r[rcount].length = htons(1);
          v4_int_template.r[rcount].handler = flow_to_flowset_sampler_id_handler;
          v4_int_template.r[rcount].length = 1;
          rcount++;
        }
	v4_template.h.c.flowset_id = htons(0);
	v4_template.h.c.length = htons( sizeof(struct NF9_TEMPLATE_FLOWSET_HEADER) + (sizeof(struct NF9_TEMPLATE_FLOWSET_RECORD) * rcount) );
	v4_template.h.template_id = htons(NF9_SOFTFLOWD_V4_TEMPLATE_ID + config.nfprobe_id);
	v4_template.h.count = htons(rcount);
	v4_template.tot_len = sizeof(struct NF9_TEMPLATE_FLOWSET_HEADER) + (sizeof(struct NF9_TEMPLATE_FLOWSET_RECORD) * rcount);

	assert(rcount < NF9_SOFTFLOWD_TEMPLATE_NRECORDS);

	for (idx = 0, v4_int_template.tot_rec_len = 0; idx < rcount; idx++)
	  v4_int_template.tot_rec_len += v4_int_template.r[idx].length;

	rcount = 0;
	bzero(&v6_template, sizeof(v6_template));
	bzero(&v6_int_template, sizeof(v6_int_template));

	v6_template.r[rcount].type = htons(NF9_LAST_SWITCHED);
	v6_template.r[rcount].length = htons(4);
	v6_int_template.r[rcount].length = 4;
	rcount++;
	v6_template.r[rcount].type = htons(NF9_FIRST_SWITCHED);
	v6_template.r[rcount].length = htons(4);
	v6_int_template.r[rcount].length = 4;
	rcount++;
	v6_template.r[rcount].type = htons(NF9_IN_BYTES);
	v6_template.r[rcount].length = htons(4);
	v6_int_template.r[rcount].length = 4;
	rcount++;
	v6_template.r[rcount].type = htons(NF9_IN_PACKETS);
	v6_template.r[rcount].length = htons(4);
	v6_int_template.r[rcount].length = 4;
	rcount++;
	v6_template.r[rcount].type = htons(NF9_IP_PROTOCOL_VERSION);
	v6_template.r[rcount].length = htons(1);
	v6_int_template.r[rcount].length = 1;
	rcount++;
        v6_template.r[rcount].type = htons(NF9_INPUT_SNMP);
        v6_template.r[rcount].length = htons(2);
	v6_int_template.r[rcount].handler = flow_to_flowset_input_handler;
        v6_int_template.r[rcount].length = 2;
        rcount++;
        v6_template.r[rcount].type = htons(NF9_OUTPUT_SNMP);
        v6_template.r[rcount].length = htons(2);
	v6_int_template.r[rcount].handler = flow_to_flowset_output_handler;
        v6_int_template.r[rcount].length = 2;
        rcount++;
	if (config.nfprobe_what_to_count & COUNT_FLOWS) { 
	  v6_template.r[rcount].type = htons(NF9_FLOWS);
	  v6_template.r[rcount].length = htons(4);
	  v6_int_template.r[rcount].handler = flow_to_flowset_flows_handler;
	  v6_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_HOST) { 
	  v6_template.r[rcount].type = htons(NF9_IPV6_SRC_ADDR);
	  v6_template.r[rcount].length = htons(16);
	  v6_int_template.r[rcount].handler = flow_to_flowset_src_host_v6_handler;
	  v6_int_template.r[rcount].length = 16;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_HOST) {
	  v6_template.r[rcount].type = htons(NF9_IPV6_DST_ADDR);
	  v6_template.r[rcount].length = htons(16);
	  v6_int_template.r[rcount].handler = flow_to_flowset_dst_host_v6_handler;
	  v6_int_template.r[rcount].length = 16;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & (COUNT_IP_TOS)) {
	  v6_template.r[rcount].type = htons(NF9_SRC_TOS);
	  v6_template.r[rcount].length = htons(1);
	  v6_int_template.r[rcount].handler = flow_to_flowset_ip_tos_handler;
	  v6_int_template.r[rcount].length = 1;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_PORT) {
	  v6_template.r[rcount].type = htons(NF9_L4_SRC_PORT);
	  v6_template.r[rcount].length = htons(2);
	  v6_int_template.r[rcount].handler = flow_to_flowset_src_port_handler;
	  v6_int_template.r[rcount].length = 2;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_PORT) {
	  v6_template.r[rcount].type = htons(NF9_L4_DST_PORT);
	  v6_template.r[rcount].length = htons(2);
	  v6_int_template.r[rcount].handler = flow_to_flowset_dst_port_handler;
	  v6_int_template.r[rcount].length = 2;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & (COUNT_SRC_PORT|COUNT_DST_PORT)) {
	  v6_template.r[rcount].type = htons(NF9_TCP_FLAGS);
	  v6_template.r[rcount].length = htons(1);
	  v6_int_template.r[rcount].handler = flow_to_flowset_tcp_flags_handler;
	  v6_int_template.r[rcount].length = 1;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_IP_PROTO) {
	  v6_template.r[rcount].type = htons(NF9_IN_PROTOCOL);
	  v6_template.r[rcount].length = htons(1);
	  v6_int_template.r[rcount].handler = flow_to_flowset_ip_proto_handler;
	  v6_int_template.r[rcount].length = 1;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_AS) {
	  v6_template.r[rcount].type = htons(NF9_SRC_AS);
	  v6_template.r[rcount].length = htons(4);
	  v6_int_template.r[rcount].handler = flow_to_flowset_src_as_handler;
	  v6_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_AS) {
	  v6_template.r[rcount].type = htons(NF9_DST_AS);
	  v6_template.r[rcount].length = htons(4);
	  v6_int_template.r[rcount].handler = flow_to_flowset_dst_as_handler;
	  v6_int_template.r[rcount].length = 4;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_SRC_MAC) {
	  v6_template.r[rcount].type = htons(NF9_SRC_MAC);
	  v6_template.r[rcount].length = htons(6);
	  v6_int_template.r[rcount].handler = flow_to_flowset_src_mac_handler;
	  v6_int_template.r[rcount].length = 6;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_DST_MAC) {
	  v6_template.r[rcount].type = htons(NF9_DST_MAC);
	  v6_template.r[rcount].length = htons(6);
	  v6_int_template.r[rcount].handler = flow_to_flowset_dst_mac_handler;
	  v6_int_template.r[rcount].length = 6;
	  rcount++;
	}
	if (config.nfprobe_what_to_count & COUNT_VLAN) {
	  v6_template.r[rcount].type = htons(NF9_SRC_VLAN);
	  v6_template.r[rcount].length = htons(2);
	  v6_int_template.r[rcount].handler = flow_to_flowset_vlan_handler;
	  v6_int_template.r[rcount].length = 2;
	  rcount++;
	}
        if (config.nfprobe_what_to_count & COUNT_VLAN) {
	  v6_template.r[rcount].type = htons(NF9_MPLS_LABEL_1);
	  v6_template.r[rcount].length = htons(3);
	  v6_int_template.r[rcount].handler = flow_to_flowset_mpls_handler;
	  v6_int_template.r[rcount].length = 3;
	  rcount++;
	}
        if (config.nfprobe_what_to_count & COUNT_CLASS) {
	  v6_template.r[rcount].type = htons(NF9_CUST_CLASS);
	  v6_template.r[rcount].length = htons(16);
	  v6_int_template.r[rcount].handler = flow_to_flowset_class_handler;
	  v6_int_template.r[rcount].length = 16;
	  rcount++;
	}
        if (config.nfprobe_what_to_count & COUNT_ID) {
	  v6_template.r[rcount].type = htons(NF9_CUST_TAG);
	  v6_template.r[rcount].length = htons(4);
	  v6_int_template.r[rcount].handler = flow_to_flowset_tag_handler;
	  v6_int_template.r[rcount].length = 4;
	  rcount++;
	}
        if (config.nfprobe_what_to_count & COUNT_ID2) {
          v6_template.r[rcount].type = htons(NF9_CUST_TAG2);
          v6_template.r[rcount].length = htons(4);
          v6_int_template.r[rcount].handler = flow_to_flowset_tag2_handler;
          v6_int_template.r[rcount].length = 4;
          rcount++;
        }
        if (config.sampling_rate || config.ext_sampling_rate) {
          v6_template.r[rcount].type = htons(NF9_FLOW_SAMPLER_ID);
          v6_template.r[rcount].length = htons(1);
          v6_int_template.r[rcount].handler = flow_to_flowset_sampler_id_handler;
          v6_int_template.r[rcount].length = 1;
          rcount++;
        }
	v6_template.h.c.flowset_id = htons(0);
	v6_template.h.c.length = htons( sizeof(struct NF9_TEMPLATE_FLOWSET_HEADER) + (sizeof(struct NF9_TEMPLATE_FLOWSET_RECORD) * rcount) );
	v6_template.h.template_id = htons(NF9_SOFTFLOWD_V6_TEMPLATE_ID + config.nfprobe_id);
	v6_template.h.count = htons(rcount);
	v6_template.tot_len = sizeof(struct NF9_TEMPLATE_FLOWSET_HEADER) + (sizeof(struct NF9_TEMPLATE_FLOWSET_RECORD) * rcount);

	assert(rcount < NF9_SOFTFLOWD_TEMPLATE_NRECORDS);

	for (idx = 0, v6_int_template.tot_rec_len = 0; idx < rcount; idx++)
	  v6_int_template.tot_rec_len += v6_int_template.r[idx].length;
}

static void
nf9_init_options_template(void)
{
	int rcount, idx;

        rcount = 0;
        bzero(&options_template, sizeof(options_template));
        bzero(&options_int_template, sizeof(options_int_template));

        options_template.r[rcount].type = htons(NF9_OPT_SCOPE_SYSTEM);
        options_template.r[rcount].length = htons(0);
        options_int_template.r[rcount].length = 0;
        rcount++;
        options_template.r[rcount].type = htons(NF9_FLOW_SAMPLER_ID);
        options_template.r[rcount].length = htons(1);
        options_int_template.r[rcount].length = 1;
        rcount++;
        options_template.r[rcount].type = htons(NF9_FLOW_SAMPLER_MODE);
        options_template.r[rcount].length = htons(1);
        options_int_template.r[rcount].length = 1;
        rcount++;
        options_template.r[rcount].type = htons(NF9_FLOW_SAMPLER_INTERVAL);
        options_template.r[rcount].length = htons(4);
        options_int_template.r[rcount].length = 4;
        rcount++;
        options_template.h.c.flowset_id = htons(1);
        options_template.h.c.length = htons( sizeof(struct NF9_OPTIONS_TEMPLATE_FLOWSET_HEADER) + (sizeof(struct NF9_TEMPLATE_FLOWSET_RECORD) * rcount) );
        options_template.h.template_id = htons(NF9_OPTIONS_TEMPLATE_ID + config.nfprobe_id );
        options_template.h.scope_len = htons(4); /* NF9_OPT_SCOPE_SYSTEM */
        options_template.h.option_len = htons(12); /* NF9_FLOW_SAMPLER_ID + NF9_FLOW_SAMPLER_MODE + NF9_FLOW_SAMPLER_INTERVAL */
        options_template.tot_len = sizeof(struct NF9_OPTIONS_TEMPLATE_FLOWSET_HEADER) + (sizeof(struct NF9_TEMPLATE_FLOWSET_RECORD) * rcount);

        for (idx = 0, options_int_template.tot_rec_len = 0; idx < rcount; idx++)
          options_int_template.tot_rec_len += options_int_template.r[idx].length;
}

static int
nf_flow_to_flowset(const struct FLOW *flow, u_char *packet, u_int len,
    const struct timeval *system_boot_time, u_int *len_used)
{
	u_int freclen, ret_len, nflows, idx;
	u_int32_t rec32;
	u_int8_t rec8;
	char *ftoft_ptr_0 = ftoft_buf_0;
	char *ftoft_ptr_1 = ftoft_buf_1;

	bzero(ftoft_buf_0, sizeof(ftoft_buf_0));
	bzero(ftoft_buf_1, sizeof(ftoft_buf_1));
	*len_used = nflows = ret_len = 0;

	rec32 = htonl(timeval_sub_ms(&flow->flow_last, system_boot_time));
	memcpy(ftoft_ptr_0, &rec32, 4);
	memcpy(ftoft_ptr_1, &rec32, 4);
	ftoft_ptr_0 += 4;
	ftoft_ptr_1 += 4;
	
	rec32 = htonl(timeval_sub_ms(&flow->flow_start, system_boot_time));
	memcpy(ftoft_ptr_0, &rec32, 4);
	memcpy(ftoft_ptr_1, &rec32, 4);
	ftoft_ptr_0 += 4;
	ftoft_ptr_1 += 4;
	
	rec32 = htonl(flow->octets[0]);
	memcpy(ftoft_ptr_0, &rec32, 4);
	rec32 = htonl(flow->octets[1]);
	memcpy(ftoft_ptr_1, &rec32, 4);
	ftoft_ptr_0 += 4;
	ftoft_ptr_1 += 4;
	
	rec32 = htonl(flow->packets[0]);
	memcpy(ftoft_ptr_0, &rec32, 4);
	rec32 = htonl(flow->packets[1]);
	memcpy(ftoft_ptr_1, &rec32, 4);
	ftoft_ptr_0 += 4;
	ftoft_ptr_1 += 4;

	switch (flow->af) {
	case AF_INET:
		rec8 = 4;
		memcpy(ftoft_ptr_0, &rec8, 1);
		memcpy(ftoft_ptr_1, &rec8, 1);
		ftoft_ptr_0 += 1;
		ftoft_ptr_1 += 1;
		for (idx = 5; v4_int_template.r[idx].length; idx++) { 
		  v4_int_template.r[idx].handler(ftoft_ptr_0, flow, 0, v4_int_template.r[idx].length);
		  v4_int_template.r[idx].handler(ftoft_ptr_1, flow, 1, v4_int_template.r[idx].length);
		  ftoft_ptr_0 += v4_int_template.r[idx].length;
		  ftoft_ptr_1 += v4_int_template.r[idx].length;
		}
		freclen = v4_int_template.tot_rec_len;
		break;
	case AF_INET6:
		rec8 = 6;
		memcpy(ftoft_ptr_0, &rec8, 1);
		memcpy(ftoft_ptr_1, &rec8, 1);
		ftoft_ptr_0 += 1;
		ftoft_ptr_1 += 1;
		for (idx = 5; v6_int_template.r[idx].length; idx++) {
		  v6_int_template.r[idx].handler(ftoft_ptr_0, flow, 0, v6_int_template.r[idx].length);
		  v6_int_template.r[idx].handler(ftoft_ptr_1, flow, 1, v6_int_template.r[idx].length);
		  ftoft_ptr_0 += v6_int_template.r[idx].length;
		  ftoft_ptr_1 += v6_int_template.r[idx].length;
		}
		freclen = v6_int_template.tot_rec_len; 
		break;
	default:
		return (-1);
	}

	if (flow->octets[0] > 0) {
		if (ret_len + freclen > len)
			return (-1);
		memcpy(packet + ret_len, ftoft_buf_0, freclen);
		ret_len += freclen;
		nflows++;
	}
	if (flow->octets[1] > 0) {
		if (ret_len + freclen > len)
			return (-1);
		memcpy(packet + ret_len, ftoft_buf_1, freclen);
		ret_len += freclen;
		nflows++;
	}

	*len_used = ret_len;
	return (nflows);
}

static int
nf_options_to_flowset(u_char *packet, u_int len, const struct timeval *system_boot_time, u_int *len_used)
{
        u_int freclen, ret_len, nflows;
        u_int32_t rec32;
        u_int8_t rec8;
        char *ftoft_ptr_0 = ftoft_buf_0;

        bzero(ftoft_buf_0, sizeof(ftoft_buf_0));
        *len_used = nflows = ret_len = 0;

        rec8 = 1; /* NF9_FLOW_SAMPLER_ID */ 
        memcpy(ftoft_ptr_0, &rec8, 1);
        ftoft_ptr_0 += 1;

        rec8 = 0x02; /* NF9_FLOW_SAMPLER_MODE */
        memcpy(ftoft_ptr_0, &rec8, 1);
        ftoft_ptr_0 += 1;

	if (config.sampling_rate)
          rec32 = htonl(config.sampling_rate); /* NF9_FLOW_SAMPLER_INTERVAL */
	else if (config.ext_sampling_rate)
          rec32 = htonl(config.ext_sampling_rate); /* NF9_FLOW_SAMPLER_INTERVAL */
        memcpy(ftoft_ptr_0, &rec32, 4);
        ftoft_ptr_0 += 4;

	freclen = options_int_template.tot_rec_len;

	if (ret_len + freclen > len)
		return (-1);

	memcpy(packet + ret_len, ftoft_buf_0, freclen);
	ret_len += freclen;
	nflows++;

        *len_used = ret_len;
        return (nflows);
}

/*
 * Given an array of expired flows, send netflow v9 report packets
 * Returns number of packets sent or -1 on error
 */
int
send_netflow_v9(struct FLOW **flows, int num_flows, int nfsock,
    u_int64_t *flows_exported, struct timeval *system_boot_time,
    int verbose_flag, u_int8_t engine_type, u_int8_t engine_id)
{
	struct NF9_HEADER *nf9;
	struct NF9_DATA_FLOWSET_HEADER *dh;
	struct timeval now;
	u_int offset, last_af, j, num_packets, inc, last_valid;
	socklen_t errsz;
	int err, r, i;
	u_char packet[NF9_SOFTFLOWD_MAX_PACKET_SIZE];
	u_int8_t *sid_ptr;

	gettimeofday(&now, NULL);

	if (nf9_pkts_until_template == -1) {
		nf9_init_template();
		nf9_init_options_template();
		nf9_pkts_until_template = 0;
	}		

	last_valid = num_packets = 0;
	for (j = 0; j < num_flows;) {
		bzero(packet, sizeof(packet));
		nf9 = (struct NF9_HEADER *)packet;

		nf9->version = htons(9);
		nf9->flows = 0; /* Filled as we go, htons at end */
		nf9->uptime_ms = htonl(timeval_sub_ms(&now, system_boot_time));
		nf9->time_sec = htonl(time(NULL));
		// nf9->package_sequence = htonl(*flows_exported + j);
		nf9->package_sequence = htonl(++(*flows_exported));

		nf9->source_id = 0;
		sid_ptr = (u_int8_t *) &nf9->source_id;
		sid_ptr[2] = engine_type; 
		sid_ptr[3] = engine_id; 

		offset = sizeof(*nf9);

		/* Refresh template headers if we need to */
		if (nf9_pkts_until_template <= 0) {
			memcpy(packet + offset, &v4_template, v4_template.tot_len);
			offset += v4_template.tot_len;
			nf9->flows++;
			memcpy(packet + offset, &v6_template, v6_template.tot_len);
			offset += v6_template.tot_len; 
			nf9->flows++;
			if (config.sampling_rate || config.ext_sampling_rate) {
                          memcpy(packet + offset, &options_template, options_template.tot_len);
                          offset += options_template.tot_len;
			  nf9->flows++;
			  send_options = TRUE;
			}
			nf9_pkts_until_template = NF9_DEFAULT_TEMPLATE_INTERVAL;
		}

		dh = NULL;
		last_af = 0;
		for (i = 0; i + j < num_flows; i++) {
			if (dh == NULL || flows[i + j]->af != last_af) {
				if (dh != NULL) {
					if (offset % 4 != 0) {
						/* Pad to multiple of 4 */
						dh->c.length += 4 - (offset % 4);
						offset += 4 - (offset % 4);
					}
					/* Finalise last header */
					dh->c.length = htons(dh->c.length);
				}
				if (offset + sizeof(*dh) > sizeof(packet)) {
					/* Mark header is finished */
					dh = NULL;
					break;
				}
				dh = (struct NF9_DATA_FLOWSET_HEADER *)
				    (packet + offset);
				if (send_options) {
				  dh->c.flowset_id = options_template.h.template_id;
				  last_af = 0;
				}
				else {
				  dh->c.flowset_id =
				    (flows[i + j]->af == AF_INET) ?
				    v4_template.h.template_id : 
				    v6_template.h.template_id;
				  last_af = flows[i + j]->af;
				}
				last_valid = offset;
				dh->c.length = sizeof(*dh); /* Filled as we go */
				offset += sizeof(*dh);
			}

			if (send_options)
                          r = nf_options_to_flowset(packet + offset,
                            sizeof(packet) - offset, system_boot_time, &inc);
			else 
			  r = nf_flow_to_flowset(flows[i + j], packet + offset,
			    sizeof(packet) - offset, system_boot_time, &inc);
			if (r <= 0) {
				/* yank off data header, if we had to go back */
				if (last_valid)
					offset = last_valid;
				break;
			}
			offset += inc;
			dh->c.length += inc;
			nf9->flows += r;
			last_valid = 0; /* Don't clobber this header now */
			if (verbose_flag) {
				Log(LOG_DEBUG, "Flow %d/%d: "
				    "r %d offset %d type %04x len %d(0x%04x) "
				    "flows %d\n", r, i, j, offset, 
				    dh->c.flowset_id, dh->c.length, 
				    dh->c.length, nf9->flows);
			}

			if (send_options) {
			  send_options = FALSE;
			  i--;
			}
		}
		/* Don't finish header if it has already been done */
		if (dh != NULL) {
			if (offset % 4 != 0) {
				/* Pad to multiple of 4 */
				dh->c.length += 4 - (offset % 4);
				offset += 4 - (offset % 4);
			}
			/* Finalise last header */
			dh->c.length = htons(dh->c.length);
		}
		nf9->flows = htons(nf9->flows);

		if (verbose_flag)
			Log(LOG_DEBUG, "Sending flow packet len = %d\n", offset);
		errsz = sizeof(err);
		/* Clear ICMP errors */
		getsockopt(nfsock, SOL_SOCKET, SO_ERROR, &err, &errsz); 
		if (send(nfsock, packet, (size_t)offset, 0) == -1)
			return (-1);
		num_packets++;
		nf9_pkts_until_template--;

		j += i;
	}

	// *flows_exported += j;
	return (num_packets);
}
