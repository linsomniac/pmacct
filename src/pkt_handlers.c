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
    along with this program; if no, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define __PKT_HANDLERS_C

/* includes */
#include "pmacct.h"
#include "pmacct-data.h"
#include "nfacctd.h"
#include "sflow.h"
#include "sfacctd.h"
#include "plugin_hooks.h"
#include "pkt_handlers.h"
#include "addr.h"
#include "bgp/bgp.h"

/* functions */
void evaluate_packet_handlers()
{
  int primitives, index = 0;

  while (channels_list[index].aggregation) { 
    primitives = 0;
    memset(&channels_list[index].phandler, 0, N_PRIMITIVES);

#if defined (HAVE_L2)
    if (channels_list[index].aggregation & (COUNT_SRC_MAC|COUNT_SUM_MAC)) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = src_mac_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_src_mac_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_src_mac_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & (COUNT_DST_MAC|COUNT_SUM_MAC)) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = dst_mac_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_dst_mac_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_dst_mac_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_VLAN) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = vlan_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_vlan_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_vlan_handler;
      primitives++;
    }
#endif

    if (channels_list[index].aggregation & (COUNT_SRC_HOST|COUNT_SRC_NET|COUNT_SUM_HOST|COUNT_SUM_NET)) {
      if (channels_list[index].plugin->cfg.nfacctd_net == NF_NET_BGP && channels_list[index].aggregation & (COUNT_SRC_NET|COUNT_SUM_NET))
	channels_list[index].phandler[primitives] = bgp_src_net_handler;
      else {
        if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = src_host_handler;
        else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_src_host_handler;
        else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_src_host_handler;
      }
      primitives++;
    }

    if (channels_list[index].aggregation & (COUNT_DST_HOST|COUNT_DST_NET|COUNT_SUM_HOST|COUNT_SUM_NET)) {
      if (channels_list[index].plugin->cfg.nfacctd_net == NF_NET_BGP && channels_list[index].aggregation & (COUNT_DST_NET|COUNT_SUM_NET))
        channels_list[index].phandler[primitives] = bgp_dst_net_handler;
      else { 
        if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = dst_host_handler;
        else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_dst_host_handler;
        else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_dst_host_handler;
      }
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_SRC_NMASK) {
      if (channels_list[index].plugin->cfg.nfacctd_net == NF_NET_BGP)
        channels_list[index].phandler[primitives] = bgp_src_nmask_handler;
      else if (channels_list[index].plugin->cfg.nfacctd_net == NF_NET_KEEP) {
        if (config.acct_type == ACCT_PM) primitives--; /* This is handled elsewhere */
        else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_src_nmask_handler;
        else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_src_nmask_handler;
      }
      else {
	primitives--; /* This is handled elsewhere */
      }
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_DST_NMASK) {
      if (channels_list[index].plugin->cfg.nfacctd_net == NF_NET_BGP)
        channels_list[index].phandler[primitives] = bgp_dst_nmask_handler;
      else if (channels_list[index].plugin->cfg.nfacctd_net == NF_NET_KEEP) {
        if (config.acct_type == ACCT_PM) primitives--; /* This is handled elsewhere */
        else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_dst_nmask_handler;
        else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_dst_nmask_handler;
      }
      else {
        primitives--; /* This is handled elsewhere */
      }
      primitives++;
    }
    
    if (channels_list[index].aggregation & (COUNT_SRC_AS|COUNT_SUM_AS)) {
      if (config.acct_type == ACCT_PM) {
	if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = src_host_handler;
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = src_host_handler;
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      else if (config.acct_type == ACCT_NF) {
	if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = NF_src_as_handler;
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = NF_src_host_handler;
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      else if (config.acct_type == ACCT_SF) {
        if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = SF_src_as_handler;
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = SF_src_host_handler;
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      primitives++;
    }

    if (channels_list[index].aggregation & (COUNT_DST_AS|COUNT_SUM_AS)) {
      if (config.acct_type == ACCT_PM) {
	if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = dst_host_handler;
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = dst_host_handler;
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      else if (config.acct_type == ACCT_NF) {
	if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = NF_dst_as_handler; 
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = NF_dst_host_handler;
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      else if (config.acct_type == ACCT_SF) {
	if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = SF_dst_as_handler;
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = SF_dst_host_handler;
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_PEER_SRC_IP) {
      if (config.acct_type == ACCT_PM) primitives--;
      else if (config.acct_type == ACCT_NF) {
	if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = NF_peer_src_ip_handler;
	else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = NF_peer_src_ip_handler; 
	else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      else if (config.acct_type == ACCT_SF) {
        if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = SF_peer_src_ip_handler;
        else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = SF_peer_src_ip_handler; 
        else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_PEER_DST_IP) {
      if (config.acct_type == ACCT_PM) primitives--;
      else if (config.acct_type == ACCT_NF) {
        if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = NF_peer_dst_ip_handler;
        else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = NF_peer_dst_ip_handler;
        else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      else if (config.acct_type == ACCT_SF) {
        if (config.nfacctd_as == NF_AS_KEEP) channels_list[index].phandler[primitives] = SF_peer_dst_ip_handler;
        else if (config.nfacctd_as == NF_AS_NEW) channels_list[index].phandler[primitives] = SF_peer_dst_ip_handler; 
        else if (config.nfacctd_as == NF_AS_BGP) primitives--; /* This is handled elsewhere */
      }
      primitives++;
    }

    if (channels_list[index].aggregation & (COUNT_STD_COMM|COUNT_EXT_COMM|COUNT_LOCAL_PREF|COUNT_MED|
                                            COUNT_AS_PATH|COUNT_PEER_DST_AS| COUNT_SRC_AS_PATH|COUNT_SRC_STD_COMM|
					    COUNT_SRC_EXT_COMM|COUNT_SRC_MED|COUNT_SRC_LOCAL_PREF)
      || (channels_list[index].aggregation & (COUNT_SRC_AS|COUNT_DST_AS|COUNT_PEER_SRC_IP|COUNT_PEER_DST_IP) &&
	  config.nfacctd_as == NF_AS_BGP)) {
      if (config.acct_type != ACCT_PM && channels_list[index].plugin->type.id == PLUGIN_ID_SFPROBE) {
	/* Let's basically get out of here this case - XXX: silently */
      } 
      else if (config.acct_type == ACCT_PM && config.nfacctd_bgp) {
	if (channels_list[index].plugin->type.id == PLUGIN_ID_SFPROBE) { 
	  channels_list[index].phandler[primitives] = sfprobe_bgp_ext_handler;
	}
        else if (channels_list[index].plugin->type.id == PLUGIN_ID_NFPROBE) {
          channels_list[index].phandler[primitives] = nfprobe_bgp_ext_handler;
        }
	else {
          channels_list[index].phandler[primitives] = bgp_ext_handler;
	}
        primitives++;
      }
      else if (config.acct_type == ACCT_NF && config.nfacctd_bgp) {
        channels_list[index].phandler[primitives] = bgp_ext_handler;
        primitives++;
      }
      else if (config.acct_type == ACCT_SF && config.nfacctd_bgp) {
        channels_list[index].phandler[primitives] = bgp_ext_handler;
        primitives++;
      }
    }

    if (channels_list[index].aggregation & COUNT_PEER_SRC_AS) {
      if (config.acct_type == ACCT_PM && config.nfacctd_bgp) {
	if (config.nfacctd_bgp_peer_as_src_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_peer_src_as_frommap_handler;
          primitives++;
	}
      }
      else if (config.acct_type == ACCT_NF && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_peer_as_src_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_peer_src_as_frommap_handler;
          primitives++;
        }
      }
      else if (config.acct_type == ACCT_SF && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_peer_as_src_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_peer_src_as_frommap_handler;
          primitives++;
        }
      }
    }

    if (channels_list[index].aggregation & COUNT_SRC_LOCAL_PREF) {
      if (config.acct_type == ACCT_PM && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_src_local_pref_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_src_local_pref_frommap_handler;
          primitives++;
        }
      }
      else if (config.acct_type == ACCT_NF && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_src_local_pref_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_src_local_pref_frommap_handler;
          primitives++;
        }
      }
      else if (config.acct_type == ACCT_SF && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_src_local_pref_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_src_local_pref_frommap_handler;
          primitives++;
        }
      }
    }

    if (channels_list[index].aggregation & COUNT_SRC_MED) {
      if (config.acct_type == ACCT_PM && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_src_med_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_src_med_frommap_handler;
          primitives++;
        }
      }
      else if (config.acct_type == ACCT_NF && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_src_med_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_src_med_frommap_handler;
          primitives++;
        }
      }
      else if (config.acct_type == ACCT_SF && config.nfacctd_bgp) {
        if (config.nfacctd_bgp_src_med_type == BGP_SRC_PRIMITIVES_MAP) {
          channels_list[index].phandler[primitives] = bgp_src_med_frommap_handler;
          primitives++;
        }
      }
    }

    if (channels_list[index].aggregation & COUNT_IS_SYMMETRIC) {
      if (config.nfacctd_bgp && config.nfacctd_bgp_is_symmetric_map) {
        channels_list[index].phandler[primitives] = bgp_is_symmetric_frommap_handler;
        primitives++;
      }
    }

    if (channels_list[index].aggregation & (COUNT_SRC_PORT|COUNT_SUM_PORT)) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = src_port_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_src_port_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_src_port_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & (COUNT_DST_PORT|COUNT_SUM_PORT)) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = dst_port_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_dst_port_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_dst_port_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_IP_TOS) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = ip_tos_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_ip_tos_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_ip_tos_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_IP_PROTO) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = ip_proto_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_ip_proto_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_ip_proto_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_TCPFLAGS) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = tcp_flags_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_tcp_flags_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_tcp_flags_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_FLOWS) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = flows_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_flows_handler;
      else if (config.acct_type == ACCT_SF) primitives--; /* NO flows handling for sFlow */
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_CLASS) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = class_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_class_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_class_handler; 
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_IN_IFACE) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = in_iface_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_in_iface_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_in_iface_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_OUT_IFACE) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = out_iface_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_out_iface_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_out_iface_handler;
      primitives++;
    }

    if (channels_list[index].aggregation & COUNT_COUNTERS) {
      if (config.acct_type == ACCT_PM) {
	channels_list[index].phandler[primitives] = counters_handler;
	if (config.sfacctd_renormalize && config.ext_sampling_rate) {
	  primitives++;
	  channels_list[index].phandler[primitives] = counters_renormalize_handler;
	}
      }
      else if (config.acct_type == ACCT_NF) {
	if (config.nfacctd_time == NF_TIME_SECS) channels_list[index].phandler[primitives] = NF_counters_secs_handler;
	else if (config.nfacctd_time == NF_TIME_NEW) channels_list[index].phandler[primitives] = NF_counters_new_handler;
	else channels_list[index].phandler[primitives] = NF_counters_msecs_handler; /* default */
	if (config.sfacctd_renormalize) {
	  primitives++;
	  if (config.ext_sampling_rate) channels_list[index].phandler[primitives] = counters_renormalize_handler;
	  else channels_list[index].phandler[primitives] = NF_counters_renormalize_handler;
	}
      }
      else if (config.acct_type == ACCT_SF) {
	channels_list[index].phandler[primitives] = SF_counters_new_handler;
	if (config.sfacctd_renormalize) {
	  primitives++;
	  if (config.ext_sampling_rate) channels_list[index].phandler[primitives] = counters_renormalize_handler;
	  else channels_list[index].phandler[primitives] = SF_counters_renormalize_handler;
	}
      }
      primitives++;
    }

    if (channels_list[index].plugin->type.id == PLUGIN_ID_NFPROBE) {
      if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = nfprobe_extras_handler;
      else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_nfprobe_extras_handler;
      else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_nfprobe_extras_handler;
      primitives++;
    }

    if (config.acct_type == ACCT_PM || config.acct_type == ACCT_NF || config.acct_type == ACCT_SF) {
      if (channels_list[index].aggregation & COUNT_ID) {
	/* we infer 'pre_tag_map' from configuration because it's global */
        if (config.pre_tag_map) {
	  channels_list[index].phandler[primitives] = ptag_id_handler;
	  primitives++;
	}
	else {
	  if (config.acct_type == ACCT_NF) {
	    channels_list[index].phandler[primitives] = NF_id_handler;
	    primitives++;
	  }
	  else if (config.acct_type == ACCT_SF) {
	    channels_list[index].phandler[primitives] = SF_id_handler;
	    primitives++;
	  }
	}

	if (channels_list[index].id) { 
	  channels_list[index].phandler[primitives] = id_handler; 
	  primitives++;
	}
      }
    }

    if (config.acct_type == ACCT_PM || config.acct_type == ACCT_NF || config.acct_type == ACCT_SF) {
      if (channels_list[index].aggregation & COUNT_ID2) {
        if (config.pre_tag_map) {
          channels_list[index].phandler[primitives] = ptag_id2_handler;
          primitives++;
        }
        else {
          if (config.acct_type == ACCT_NF) {
            channels_list[index].phandler[primitives] = NF_id2_handler;
            primitives++;
          }
          else if (config.acct_type == ACCT_SF) {
            channels_list[index].phandler[primitives] = SF_id2_handler;
            primitives++;
          }
        }
      }
    }

    /* Better these two sfprobe-related functions to stay last due
       to different structure put on the pipe; ie. id/id2 handlers
       were writing in the middle of the payload */
    if (channels_list[index].aggregation & COUNT_PAYLOAD) {
      if (channels_list[index].plugin->type.id == PLUGIN_ID_SFPROBE) {
        if (config.acct_type == ACCT_PM) channels_list[index].phandler[primitives] = sfprobe_payload_handler;
        else if (config.acct_type == ACCT_NF) channels_list[index].phandler[primitives] = NF_sfprobe_payload_handler;
        else if (config.acct_type == ACCT_SF) channels_list[index].phandler[primitives] = SF_sfprobe_payload_handler;
      }
      primitives++;
    }

    if (channels_list[index].s.rate) {
      if (channels_list[index].plugin->type.id == PLUGIN_ID_SFPROBE)
        channels_list[index].phandler[primitives] = sfprobe_sampling_handler;
      else channels_list[index].phandler[primitives] = sampling_handler;
      primitives++;
    }
    index++;
  }

  assert(primitives < N_PRIMITIVES);
}

#if defined (HAVE_L2)
void src_mac_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->mac_ptr) memcpy(pdata->primitives.eth_shost, (pptrs->mac_ptr+ETH_ADDR_LEN), ETH_ADDR_LEN); 
}

void dst_mac_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->mac_ptr) memcpy(pdata->primitives.eth_dhost, pptrs->mac_ptr, ETH_ADDR_LEN);
}

void vlan_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  u_int16_t vlan_id = 0;
  
  if (pptrs->vlan_ptr) {
    memcpy(&vlan_id, pptrs->vlan_ptr, 2);
    pdata->primitives.vlan_id = ntohs(vlan_id);
  }
}
#endif

void bgp_src_net_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct bgp_node *ret = (struct bgp_node *) pptrs->bgp_src;

  if (pptrs->l3_proto == ETHERTYPE_IP) {
    if (ret) {
      memcpy(&pdata->primitives.src_ip.address.ipv4, &ret->p.u.prefix4, 4);
      pdata->primitives.src_ip.family = AF_INET;
    }
  }
#if defined ENABLE_IPV6
  else if (pptrs->l3_proto == ETHERTYPE_IPV6) {
    if (ret) {
      memcpy(&pdata->primitives.src_ip.address.ipv6, &ret->p.u.prefix6, 16);
      pdata->primitives.src_ip.family = AF_INET6;
    }
  }
#endif
}

void bgp_dst_net_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct bgp_node *ret = (struct bgp_node *) pptrs->bgp_dst;

  if (pptrs->l3_proto == ETHERTYPE_IP) {
    if (ret) {
      memcpy(&pdata->primitives.dst_ip.address.ipv4, &ret->p.u.prefix4, 4);
      pdata->primitives.dst_ip.family = AF_INET;
    }
  }
#if defined ENABLE_IPV6
  else if (pptrs->l3_proto == ETHERTYPE_IPV6) {
    if (ret) {
      memcpy(&pdata->primitives.dst_ip.address.ipv6, &ret->p.u.prefix6, 16);
      pdata->primitives.dst_ip.family = AF_INET6;
    }
  }
#endif
}

void bgp_src_nmask_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct bgp_node *ret = (struct bgp_node *) pptrs->bgp_src;

  if (ret) pdata->primitives.src_nmask = ret->p.prefixlen;
}

void bgp_dst_nmask_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct bgp_node *ret = (struct bgp_node *) pptrs->bgp_dst;

  if (ret) pdata->primitives.dst_nmask = ret->p.prefixlen;
}

void src_host_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->l3_proto == ETHERTYPE_IP) {
    pdata->primitives.src_ip.address.ipv4.s_addr = ((struct my_iphdr *) pptrs->iph_ptr)->ip_src.s_addr;
    pdata->primitives.src_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6 
  else if (pptrs->l3_proto == ETHERTYPE_IPV6) {
    memcpy(&pdata->primitives.src_ip.address.ipv6, &((struct ip6_hdr *)pptrs->iph_ptr)->ip6_src, IP6AddrSz); 
    pdata->primitives.src_ip.family = AF_INET6;
  }
#endif
}

void dst_host_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->l3_proto == ETHERTYPE_IP) {
    pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct my_iphdr *) pptrs->iph_ptr)->ip_dst.s_addr;
    pdata->primitives.dst_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6 
  else if (pptrs->l3_proto == ETHERTYPE_IPV6) {
    memcpy(&pdata->primitives.dst_ip.address.ipv6, &((struct ip6_hdr *)pptrs->iph_ptr)->ip6_dst, IP6AddrSz);
    pdata->primitives.dst_ip.family = AF_INET6;
  }
#endif
}

void src_port_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->l4_proto == IPPROTO_UDP || pptrs->l4_proto == IPPROTO_TCP)
    pdata->primitives.src_port = ntohs(((struct my_tlhdr *) pptrs->tlh_ptr)->src_port);
  else pdata->primitives.src_port = 0;
}

void dst_port_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->l4_proto == IPPROTO_UDP || pptrs->l4_proto == IPPROTO_TCP)
    pdata->primitives.dst_port = ntohs(((struct my_tlhdr *) pptrs->tlh_ptr)->dst_port);
  else pdata->primitives.dst_port = 0;
}

void ip_tos_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  u_int32_t tos = 0;

  if (pptrs->l3_proto == ETHERTYPE_IP) {
    pdata->primitives.tos = ((struct my_iphdr *) pptrs->iph_ptr)->ip_tos;
  }
#if defined ENABLE_IPV6
  else if (pptrs->l3_proto == ETHERTYPE_IPV6) {
    tos = ntohl(((struct ip6_hdr *) pptrs->iph_ptr)->ip6_flow);
    tos = ((tos & 0x0ff00000) >> 20);
    pdata->primitives.tos = tos; 
  }
#endif
}

void ip_proto_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  
  pdata->primitives.proto = pptrs->l4_proto;
}

void tcp_flags_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->l4_proto == IPPROTO_TCP) pdata->tcp_flags = pptrs->tcp_flags;
}

void counters_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->l3_proto == ETHERTYPE_IP) pdata->pkt_len = ntohs(((struct my_iphdr *) pptrs->iph_ptr)->ip_len);
#if defined ENABLE_IPV6
  else if (pptrs->l3_proto == ETHERTYPE_IPV6) pdata->pkt_len = ntohs(((struct ip6_hdr *) pptrs->iph_ptr)->ip6_plen)+IP6HdrSz;
#endif
  if (pptrs->pf) {
    pdata->pkt_num = pptrs->pf+1;
    pptrs->pf = 0;
  }
  else pdata->pkt_num = 1; 
  pdata->time_start = ((struct pcap_pkthdr *)pptrs->pkthdr)->ts.tv_sec;
  pdata->time_end = 0;
}

void counters_renormalize_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  pdata->pkt_len = pdata->pkt_len*config.ext_sampling_rate;
  pdata->pkt_num = pdata->pkt_num*config.ext_sampling_rate; 
}

void id_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  pdata->primitives.id = chptr->id;
}

void flows_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->new_flow) pdata->flo_num = 1;
}

void class_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  pdata->primitives.class = pptrs->class;
  pdata->cst.ba = pptrs->cst.ba;
  pdata->cst.pa = pptrs->cst.pa;
  if (chptr->aggregation & COUNT_FLOWS)
    pdata->cst.fa = pptrs->cst.fa;
  pdata->cst.stamp.tv_sec = pptrs->cst.stamp.tv_sec;
  pdata->cst.stamp.tv_usec = pptrs->cst.stamp.tv_usec;
  pdata->cst.tentatives = pptrs->cst.tentatives;
}

void sfprobe_payload_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_payload *payload = (struct pkt_payload *) *data;
  struct pkt_data tmp;
  struct eth_header eh;
  char *buf = (char *) *data, *tmpp = (char *) &tmp;
  int space = (chptr->bufend - chptr->bufptr) - PpayloadSz;
  int ethHdrLen = 0;

  memset(&tmp, 0, sizeof(tmp));

  if (chptr->plugin->cfg.nfacctd_as == NF_AS_NEW ||
      chptr->plugin->cfg.nfacctd_net == NF_NET_NEW) {
    src_host_handler(chptr, pptrs, &tmpp);
    dst_host_handler(chptr, pptrs, &tmpp);
    memcpy(&payload->src_ip, &tmp.primitives.src_ip, HostAddrSz);
    memcpy(&payload->dst_ip, &tmp.primitives.dst_ip, HostAddrSz);
  }

  if (chptr->plugin->cfg.nfacctd_net == NF_NET_BGP) {
    bgp_src_nmask_handler(chptr, pptrs, &tmpp);
    bgp_dst_nmask_handler(chptr, pptrs, &tmpp);
    payload->src_nmask = tmp.primitives.src_nmask;
    payload->dst_nmask = tmp.primitives.dst_nmask;
  }

  payload->cap_len = ((struct pcap_pkthdr *)pptrs->pkthdr)->caplen;
  payload->pkt_len = ((struct pcap_pkthdr *)pptrs->pkthdr)->len;
  payload->pkt_num = 1; 
  payload->time_start = ((struct pcap_pkthdr *)pptrs->pkthdr)->ts.tv_sec;
  payload->class = pptrs->class;
  payload->tag = pptrs->tag;
  payload->tag2 = pptrs->tag2;
  if (pptrs->ifindex_in > 0)  payload->ifindex_in  = pptrs->ifindex_in;
  if (pptrs->ifindex_out > 0) payload->ifindex_out = pptrs->ifindex_out;

  /* Typically don't have L2 info under ULOG */
  if (!pptrs->mac_ptr) {
    ethHdrLen = sizeof(struct eth_header);
    memset(&eh, 0, ethHdrLen);
    eh.ether_type = htons(pptrs->l3_proto);
    payload->cap_len += ethHdrLen;
    payload->pkt_len += ethHdrLen;
  }

  /* We could be capturing the entire packet; DEFAULT_PLOAD_SIZE is our cut-off point */
  if (payload->cap_len > DEFAULT_PLOAD_SIZE) payload->cap_len = DEFAULT_PLOAD_SIZE;

  if (space >= payload->cap_len) {
    buf += PpayloadSz;
    if (!pptrs->mac_ptr) {
      memcpy(buf, &eh, ethHdrLen);
      buf += ethHdrLen;
    }
    memcpy(buf, pptrs->packet_ptr, payload->cap_len-ethHdrLen);
    chptr->bufptr += payload->cap_len; /* don't count pkt_payload here */ 
#if NEED_ALIGN
    while (chptr->bufptr % 4 != 0) chptr->bufptr++; /* Don't worry, it's harmless increasing here */
#endif
  }
  else {
    chptr->bufptr += space;
    chptr->reprocess = TRUE;
  }
}

void nfprobe_extras_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_extras *pextras = (struct pkt_extras *) ++pdata;

  if (pptrs->mpls_ptr) memcpy(&pextras->mpls_top_label, pptrs->mpls_ptr, 4);
  if (pptrs->l4_proto == IPPROTO_TCP) pextras->tcp_flags = pptrs->tcp_flags;
}

void in_iface_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->ifindex_in > 0)  pdata->primitives.ifindex_in  = pptrs->ifindex_in;
}

void out_iface_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  if (pptrs->ifindex_out > 0) pdata->primitives.ifindex_out = pptrs->ifindex_out;
}

#if defined (HAVE_L2)
void NF_src_mac_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.eth_shost, pptrs->f_data+tpl->tpl[NF9_SRC_MAC].off, MIN(tpl->tpl[NF9_SRC_MAC].len, 6));
    break;
  default:
    break;
  }
}

void NF_dst_mac_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.eth_dhost, pptrs->f_data+tpl->tpl[NF9_DST_MAC].off, MIN(tpl->tpl[NF9_DST_MAC].len, 6));
    break;
  default:
    break;
  }
}

void NF_vlan_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.vlan_id, pptrs->f_data+tpl->tpl[NF9_SRC_VLAN].off, MIN(tpl->tpl[NF9_SRC_VLAN].len, 2));
    pdata->primitives.vlan_id = ntohs(pdata->primitives.vlan_id);
    break;
  default:
    break;
  }
}
#endif

void NF_src_host_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int8_t src_mask = 0;

  switch(hdr->version) {
  case 9:
    if (pptrs->l3_proto == ETHERTYPE_IP) {
      memcpy(&pdata->primitives.src_ip.address.ipv4, pptrs->f_data+tpl->tpl[NF9_IPV4_SRC_ADDR].off, MIN(tpl->tpl[NF9_IPV4_SRC_ADDR].len, 4)); 
      memcpy(&src_mask, pptrs->f_data+tpl->tpl[NF9_SRC_MASK].off, tpl->tpl[NF9_SRC_MASK].len); 
      pdata->primitives.src_ip.family = AF_INET;
      break;
    }
#if defined ENABLE_IPV6
    if (pptrs->l3_proto == ETHERTYPE_IPV6) {
      memcpy(&pdata->primitives.src_ip.address.ipv6, pptrs->f_data+tpl->tpl[NF9_IPV6_SRC_ADDR].off, MIN(tpl->tpl[NF9_IPV6_SRC_ADDR].len, 16));
      memcpy(&src_mask, pptrs->f_data+tpl->tpl[NF9_IPV6_SRC_MASK].off, tpl->tpl[NF9_IPV6_SRC_MASK].len); 
      pdata->primitives.src_ip.family = AF_INET6;
      break;
    }
#endif
    break;
  case 8:
    switch(hdr->aggregation) {
    case 3:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_3 *) pptrs->f_data)->src_prefix;
      src_mask = ((struct struct_export_v8_3 *) pptrs->f_data)->src_mask;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    case 5:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_5 *) pptrs->f_data)->src_prefix;
      src_mask = ((struct struct_export_v8_5 *) pptrs->f_data)->src_mask;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    case 7:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_7 *) pptrs->f_data)->srcaddr;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    case 8:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_8 *) pptrs->f_data)->srcaddr;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    case 11:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_11 *) pptrs->f_data)->src_prefix;
      src_mask = ((struct struct_export_v8_11 *) pptrs->f_data)->src_mask;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    case 13:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_13 *) pptrs->f_data)->src_prefix;
      src_mask = ((struct struct_export_v8_13 *) pptrs->f_data)->src_mask;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    case 14:
      pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v8_14 *) pptrs->f_data)->src_prefix;
      src_mask = ((struct struct_export_v8_14 *) pptrs->f_data)->src_mask;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    default:
      pdata->primitives.src_ip.address.ipv4.s_addr = 0;
      pdata->primitives.src_ip.family = AF_INET;
      break;
    }  
    break;
  default:
    pdata->primitives.src_ip.address.ipv4.s_addr = ((struct struct_export_v5 *) pptrs->f_data)->srcaddr.s_addr;
    src_mask = ((struct struct_export_v5 *) pptrs->f_data)->src_mask;
    pdata->primitives.src_ip.family = AF_INET;
    break;
  }

  if ((chptr->aggregation & (COUNT_SRC_NET|COUNT_SUM_NET)) && chptr->plugin->cfg.nfacctd_net == NF_NET_KEEP) {
    u_int32_t maskbits[4], addrh[4];
    u_int8_t j;

    memset(maskbits, 0,sizeof(maskbits));
    for (j = 0; j < 4 && src_mask >= 32; j++, src_mask -= 32) maskbits[j] = 0xffffffffU;
    if (j < 4 && src_mask) maskbits[j] = ~(0xffffffffU >> src_mask);

    if (pdata->primitives.src_ip.family == AF_INET) {
      addrh[0] = ntohl(pdata->primitives.src_ip.address.ipv4.s_addr);
      addrh[0] &= maskbits[0];
      pdata->primitives.src_ip.address.ipv4.s_addr = htonl(addrh[0]);
    }
#if defined ENABLE_IPV6
    else if (pdata->primitives.src_ip.family == AF_INET6) {
      memcpy(&addrh, (void *) pm_ntohl6(&pdata->primitives.src_ip.address.ipv6), IP6AddrSz);
      for (j = 0; j < 4; j++) addrh[j] &= maskbits[j];
      memcpy(&pdata->primitives.src_ip.address.ipv6, (void *) pm_htonl6(addrh), IP6AddrSz);
    }
#endif
  }
}

void NF_dst_host_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int8_t dst_mask = 0;

  switch(hdr->version) {
  case 9:
    if (pptrs->l3_proto == ETHERTYPE_IP) {
      memcpy(&pdata->primitives.dst_ip.address.ipv4, pptrs->f_data+tpl->tpl[NF9_IPV4_DST_ADDR].off, MIN(tpl->tpl[NF9_IPV4_DST_ADDR].len, 4));
      memcpy(&dst_mask, pptrs->f_data+tpl->tpl[NF9_DST_MASK].off, tpl->tpl[NF9_DST_MASK].len);
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    }
#if defined ENABLE_IPV6
    if (pptrs->l3_proto == ETHERTYPE_IPV6) {
      memcpy(&pdata->primitives.dst_ip.address.ipv6, pptrs->f_data+tpl->tpl[NF9_IPV6_DST_ADDR].off, MIN(tpl->tpl[NF9_IPV6_DST_ADDR].len, 16));
      memcpy(&dst_mask, pptrs->f_data+tpl->tpl[NF9_IPV6_DST_MASK].off, tpl->tpl[NF9_IPV6_DST_MASK].len);
      pdata->primitives.dst_ip.family = AF_INET6;
      break;
    }
#endif
    break;
  case 8:
    switch(hdr->aggregation) {
    case 4:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_4 *) pptrs->f_data)->dst_prefix;
      dst_mask = ((struct struct_export_v8_4 *) pptrs->f_data)->dst_mask;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 5:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_5 *) pptrs->f_data)->dst_prefix;
      dst_mask = ((struct struct_export_v8_5 *) pptrs->f_data)->dst_mask;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 6:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_6 *) pptrs->f_data)->dstaddr;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 7:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_7 *) pptrs->f_data)->dstaddr;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 8:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_8 *) pptrs->f_data)->dstaddr;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 12:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_12 *) pptrs->f_data)->dst_prefix;
      dst_mask = ((struct struct_export_v8_12 *) pptrs->f_data)->dst_mask;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 13:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_13 *) pptrs->f_data)->dst_prefix;
      dst_mask = ((struct struct_export_v8_13 *) pptrs->f_data)->dst_mask;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    case 14:
      pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v8_14 *) pptrs->f_data)->dst_prefix;
      dst_mask = ((struct struct_export_v8_14 *) pptrs->f_data)->dst_mask;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    default:
      pdata->primitives.dst_ip.address.ipv4.s_addr = 0;
      pdata->primitives.dst_ip.family = AF_INET;
      break;
    }
    break;
  default:
    pdata->primitives.dst_ip.address.ipv4.s_addr = ((struct struct_export_v5 *) pptrs->f_data)->dstaddr.s_addr;
    dst_mask = ((struct struct_export_v5 *) pptrs->f_data)->dst_mask;
    pdata->primitives.dst_ip.family = AF_INET;
    break;
  }

  if ((chptr->aggregation & (COUNT_DST_NET|COUNT_SUM_NET)) && chptr->plugin->cfg.nfacctd_net == NF_NET_KEEP) {
    u_int32_t maskbits[4], addrh[4];
    u_int8_t j;

    memset(maskbits, 0, sizeof(maskbits));
    for (j = 0; j < 4 && dst_mask >= 32; j++, dst_mask -= 32) maskbits[j] = 0xffffffffU;
    if (j < 4 && dst_mask) maskbits[j] = ~(0xffffffffU >> dst_mask);

    if (pdata->primitives.dst_ip.family == AF_INET) {
      addrh[0] = ntohl(pdata->primitives.dst_ip.address.ipv4.s_addr);
      addrh[0] &= maskbits[0];
      pdata->primitives.dst_ip.address.ipv4.s_addr = htonl(addrh[0]);
    }
#if defined ENABLE_IPV6
    else if (pdata->primitives.dst_ip.family == AF_INET6) {
      memcpy(&addrh, (void *) pm_ntohl6(&pdata->primitives.dst_ip.address.ipv6), IP6AddrSz);
      for (j = 0; j < 4; j++) addrh[j] &= maskbits[j];
      memcpy(&pdata->primitives.dst_ip.address.ipv6, (void *) pm_htonl6(addrh), IP6AddrSz);
    }
#endif
  }
}

void NF_src_nmask_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.src_nmask, pptrs->f_data+tpl->tpl[NF9_SRC_MASK].off, tpl->tpl[NF9_SRC_MASK].len); 
    break;
  case 8:
    switch(hdr->aggregation) {
    case 3:
      pdata->primitives.src_nmask = ((struct struct_export_v8_3 *) pptrs->f_data)->src_mask;
      break;
    case 5:
      pdata->primitives.src_nmask = ((struct struct_export_v8_5 *) pptrs->f_data)->src_mask;
      break;
    case 11:
      pdata->primitives.src_nmask = ((struct struct_export_v8_11 *) pptrs->f_data)->src_mask;
      break;
    case 13:
      pdata->primitives.src_nmask = ((struct struct_export_v8_13 *) pptrs->f_data)->src_mask;
      break;
    case 14:
      pdata->primitives.src_nmask = ((struct struct_export_v8_14 *) pptrs->f_data)->src_mask;
      break;
    default:
      pdata->primitives.src_nmask = 0;
      break;
    }  
    break;
  default:
    pdata->primitives.src_nmask = ((struct struct_export_v5 *) pptrs->f_data)->src_mask;
    break;
  }
}

void NF_dst_nmask_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.dst_nmask, pptrs->f_data+tpl->tpl[NF9_DST_MASK].off, tpl->tpl[NF9_DST_MASK].len);
    break;
  case 8:
    switch(hdr->aggregation) {
    case 4:
      pdata->primitives.dst_nmask = ((struct struct_export_v8_4 *) pptrs->f_data)->dst_mask;
      break;
    case 5:
      pdata->primitives.dst_nmask = ((struct struct_export_v8_5 *) pptrs->f_data)->dst_mask;
      break;
    case 12:
      pdata->primitives.dst_nmask = ((struct struct_export_v8_12 *) pptrs->f_data)->dst_mask;
      break;
    case 13:
      pdata->primitives.dst_nmask = ((struct struct_export_v8_13 *) pptrs->f_data)->dst_mask;
      break;
    case 14:
      pdata->primitives.dst_nmask = ((struct struct_export_v8_14 *) pptrs->f_data)->dst_mask;
      break;
    default:
      pdata->primitives.dst_nmask = 0;
      break;
    }
    break;
  default:
    pdata->primitives.dst_nmask = ((struct struct_export_v5 *) pptrs->f_data)->dst_mask;
    break;
  }
}

void NF_src_as_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int16_t asn16 = 0;
  u_int32_t asn32 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_SRC_AS].len == 2) {
      memcpy(&asn16, pptrs->f_data+tpl->tpl[NF9_SRC_AS].off, 2);
      pdata->primitives.src_as = ntohs(asn16);
    }
    else if (tpl->tpl[NF9_SRC_AS].len == 4) {
      memcpy(&asn32, pptrs->f_data+tpl->tpl[NF9_SRC_AS].off, 4); 
      pdata->primitives.src_as = ntohl(asn32); 
    }
    break;
  case 8:
    switch(hdr->aggregation) {
    case 1:
      pdata->primitives.src_as = ntohs(((struct struct_export_v8_1 *) pptrs->f_data)->src_as);
      break;
    case 3:
      pdata->primitives.src_as = ntohs(((struct struct_export_v8_3 *) pptrs->f_data)->src_as);
      break;
    case 5:
      pdata->primitives.src_as = ntohs(((struct struct_export_v8_5 *) pptrs->f_data)->src_as);
      break;
    case 9:
      pdata->primitives.src_as = ntohs(((struct struct_export_v8_9 *) pptrs->f_data)->src_as);
      break;
    case 11:
      pdata->primitives.src_as = ntohs(((struct struct_export_v8_11 *) pptrs->f_data)->src_as);
      break;
    case 13:
      pdata->primitives.src_as = ntohs(((struct struct_export_v8_13 *) pptrs->f_data)->src_as);
      break;
    default:
      pdata->primitives.src_as = 0;
      break;
    }
    break;
  default:
    pdata->primitives.src_as = ntohs(((struct struct_export_v5 *) pptrs->f_data)->src_as);
    break;
  }
}

void NF_dst_as_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int16_t asn16 = 0;
  u_int32_t asn32 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_DST_AS].len == 2) {
      memcpy(&asn16, pptrs->f_data+tpl->tpl[NF9_DST_AS].off, 2); 
      pdata->primitives.dst_as = ntohs(asn16);
    }
    else if (tpl->tpl[NF9_DST_AS].len == 4) {
      memcpy(&asn32, pptrs->f_data+tpl->tpl[NF9_DST_AS].off, 4);
      pdata->primitives.dst_as = ntohl(asn32); 
    }
    break;
  case 8:
    switch(hdr->aggregation) {
    case 1:
      pdata->primitives.dst_as = ntohs(((struct struct_export_v8_1 *) pptrs->f_data)->dst_as);
      break;
    case 4:
      pdata->primitives.dst_as = ntohs(((struct struct_export_v8_4 *) pptrs->f_data)->dst_as);
      break;
    case 5:
      pdata->primitives.dst_as = ntohs(((struct struct_export_v8_5 *) pptrs->f_data)->dst_as);
      break;
    case 9:
      pdata->primitives.dst_as = ntohs(((struct struct_export_v8_9 *) pptrs->f_data)->dst_as);
      break;
    case 12:
      pdata->primitives.dst_as = ntohs(((struct struct_export_v8_12 *) pptrs->f_data)->dst_as);
      break;
    case 13:
      pdata->primitives.dst_as = ntohs(((struct struct_export_v8_13 *) pptrs->f_data)->dst_as);
      break;
    default:
      pdata->primitives.dst_as = 0;
      break;
    }
    break;
  default:
    pdata->primitives.dst_as = ntohs(((struct struct_export_v5 *) pptrs->f_data)->dst_as);
    break;
  }
}

void NF_peer_src_ip_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  struct sockaddr *sa = (struct sockaddr *) pptrs->f_agent;

  --pdata; /* Bringing back to original place */

  if (sa->sa_family == AF_INET) {
    pbgp->peer_src_ip.address.ipv4.s_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
    pbgp->peer_src_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6
  else if (sa->sa_family == AF_INET6) {
    memcpy(&pbgp->peer_src_ip.address.ipv6, &((struct sockaddr_in6 *)sa)->sin6_addr, IP6AddrSz);
    pbgp->peer_src_ip.family = AF_INET6;
  }
#endif
}

void NF_peer_dst_ip_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;

  --pdata; /* Bringing back to original place */

  switch(hdr->version) {
  case 9:
    if (pptrs->l3_proto == ETHERTYPE_IP) {
      memcpy(&pbgp->peer_dst_ip.address.ipv4, pptrs->f_data+tpl->tpl[NF9_BGP_IPV4_NEXT_HOP].off, MIN(tpl->tpl[NF9_BGP_IPV4_NEXT_HOP].len, 4));
      pbgp->peer_dst_ip.family = AF_INET;
      break;
    }
#if defined ENABLE_IPV6
    if (pptrs->l3_proto == ETHERTYPE_IPV6) {
      memcpy(&pbgp->peer_dst_ip.address.ipv6, pptrs->f_data+tpl->tpl[NF9_BGP_IPV6_NEXT_HOP].off, MIN(tpl->tpl[NF9_BGP_IPV6_NEXT_HOP].len, 16));
      pbgp->peer_dst_ip.family = AF_INET6;
      break;
    }
#endif
    break;
  case 8:
  default:
    break;
  }
}

void NF_src_port_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  
  switch(hdr->version) {
  case 9:
    if (((u_int8_t)*(pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off) == IPPROTO_UDP) ||
        ((u_int8_t)*(pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off) == IPPROTO_TCP)) {
      memcpy(&pdata->primitives.src_port, pptrs->f_data+tpl->tpl[NF9_L4_SRC_PORT].off, MIN(tpl->tpl[NF9_L4_SRC_PORT].len, 2));
      pdata->primitives.src_port = ntohs(pdata->primitives.src_port);
    }
    else pdata->primitives.src_port = 0;
    break;
  case 8:
    switch(hdr->aggregation) {
    case 2:
      if ((((struct struct_export_v8_2 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_2 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.src_port = ntohs(((struct struct_export_v8_2 *) pptrs->f_data)->srcport);
      break;
    case 8:
      if ((((struct struct_export_v8_8 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_8 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.src_port = ntohs(((struct struct_export_v8_8 *) pptrs->f_data)->srcport);
      break;
    case 10:
      if ((((struct struct_export_v8_10 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_10 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.src_port = ntohs(((struct struct_export_v8_10 *) pptrs->f_data)->srcport);
      break;
    case 14:
      if ((((struct struct_export_v8_14 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_14 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.src_port = ntohs(((struct struct_export_v8_14 *) pptrs->f_data)->srcport);
      break;
    default:
      pdata->primitives.src_port = 0; 
      break;
    }
    break;
  default:
    if ((((struct struct_export_v5 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
        ((struct struct_export_v5 *) pptrs->f_data)->prot == IPPROTO_TCP) {
      pdata->primitives.src_port = ntohs(((struct struct_export_v5 *) pptrs->f_data)->srcport);
    }
    else pdata->primitives.src_port = 0;
    break;
  }
}

void NF_dst_port_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    if (((u_int8_t)*(pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off) == IPPROTO_UDP) ||
        ((u_int8_t)*(pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off) == IPPROTO_TCP)) {
      memcpy(&pdata->primitives.dst_port, pptrs->f_data+tpl->tpl[NF9_L4_DST_PORT].off, MIN(tpl->tpl[NF9_L4_DST_PORT].len, 2));
      pdata->primitives.dst_port = ntohs(pdata->primitives.dst_port);
    }
    else pdata->primitives.dst_port = 0;
    break;
  case 8:
    switch(hdr->aggregation) {
    case 2:
      if ((((struct struct_export_v8_2 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_2 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.dst_port = ntohs(((struct struct_export_v8_2 *) pptrs->f_data)->dstport);
      break;
    case 8:
      if ((((struct struct_export_v8_8 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_8 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.dst_port = ntohs(((struct struct_export_v8_8 *) pptrs->f_data)->dstport);
      break;
    case 10:
      if ((((struct struct_export_v8_10 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_10 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.dst_port = ntohs(((struct struct_export_v8_10 *) pptrs->f_data)->dstport);
      break;
    case 14:
      if ((((struct struct_export_v8_14 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
          ((struct struct_export_v8_14 *) pptrs->f_data)->prot == IPPROTO_TCP) 
        pdata->primitives.dst_port = ntohs(((struct struct_export_v8_14 *) pptrs->f_data)->dstport);
      break;
    default:
      pdata->primitives.dst_port = 0;
      break;
    }
    break;
  default:
    if ((((struct struct_export_v5 *) pptrs->f_data)->prot == IPPROTO_UDP) ||
        ((struct struct_export_v5 *) pptrs->f_data)->prot == IPPROTO_TCP) 
      pdata->primitives.dst_port = ntohs(((struct struct_export_v5 *) pptrs->f_data)->dstport);
    else pdata->primitives.dst_port = 0;
    break;
  }
}

void NF_ip_tos_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.tos, pptrs->f_data+tpl->tpl[NF9_SRC_TOS].off, MIN(tpl->tpl[NF9_SRC_TOS].len, 1));
    break;
  case 8:
    switch(hdr->aggregation) {
    case 6:
      pdata->primitives.tos = ((struct struct_export_v8_6 *) pptrs->f_data)->tos;
      break;
    case 7:
      pdata->primitives.tos = ((struct struct_export_v8_7 *) pptrs->f_data)->tos;
      break;
    case 8:
      pdata->primitives.tos = ((struct struct_export_v8_8 *) pptrs->f_data)->tos;
      break;
    case 9:
      pdata->primitives.tos = ((struct struct_export_v8_9 *) pptrs->f_data)->tos;
      break;
    case 10:
      pdata->primitives.tos = ((struct struct_export_v8_10 *) pptrs->f_data)->tos;
      break;
    case 11:
      pdata->primitives.tos = ((struct struct_export_v8_11 *) pptrs->f_data)->tos;
      break;
    case 12:
      pdata->primitives.tos = ((struct struct_export_v8_12 *) pptrs->f_data)->tos;
      break;
    case 13:
      pdata->primitives.tos = ((struct struct_export_v8_13 *) pptrs->f_data)->tos;
      break;
    case 14:
      pdata->primitives.tos = ((struct struct_export_v8_14 *) pptrs->f_data)->tos;
      break;
    default:
      pdata->primitives.tos = 0;
      break;
    }
    break;
  default:
    pdata->primitives.tos = ((struct struct_export_v5 *) pptrs->f_data)->tos;
    break;
  }
}

void NF_ip_proto_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    memcpy(&pdata->primitives.proto, pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off, MIN(tpl->tpl[NF9_L4_PROTOCOL].len, 1));
    break;
  case 8:
    switch(hdr->aggregation) {
    case 2:
      pdata->primitives.proto = ((struct struct_export_v8_2 *) pptrs->f_data)->prot;
      break;
    case 8:
      pdata->primitives.proto = ((struct struct_export_v8_8 *) pptrs->f_data)->prot;
      break;
    case 10:
      pdata->primitives.proto = ((struct struct_export_v8_10 *) pptrs->f_data)->prot;
      break;
    case 14:
      pdata->primitives.proto = ((struct struct_export_v8_14 *) pptrs->f_data)->prot;
      break;
    default:
      pdata->primitives.proto = 0;
      break;
    }
    break;
  default:
    pdata->primitives.proto = ((struct struct_export_v5 *) pptrs->f_data)->prot;
    break;
  }
}

void NF_tcp_flags_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int8_t tcp_flags = 0;

  switch(hdr->version) {
  case 9:
    if ((u_int8_t)*(pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off) == IPPROTO_TCP) {
      memcpy(&tcp_flags, pptrs->f_data+tpl->tpl[NF9_TCP_FLAGS].off, MIN(tpl->tpl[NF9_TCP_FLAGS].len, 1));
      pdata->tcp_flags = tcp_flags;
    }
    break;
  default:
    if (((struct struct_export_v5 *) pptrs->f_data)->prot == IPPROTO_TCP && hdr->version == 5)
      pdata->tcp_flags = ((struct struct_export_v5 *) pptrs->f_data)->tcp_flags;
    break;
  }
}

/* times from the netflow engine are in msecs */
void NF_counters_msecs_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  time_t fstime = 0;
  u_int32_t t32 = 0;
  u_int64_t t64 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_IN_BYTES].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_IN_BYTES].off, 4);
      pdata->pkt_len = ntohl(t32);
    }
    else if (tpl->tpl[NF9_IN_BYTES].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_IN_BYTES].off, 8);
      pdata->pkt_len = pm_ntohll(t64);
    }
    if (tpl->tpl[NF9_IN_PACKETS].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_IN_PACKETS].off, 4);
      pdata->pkt_num = ntohl(t32);
    }
    else if (tpl->tpl[NF9_IN_PACKETS].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_IN_PACKETS].off, 8);
      pdata->pkt_num = pm_ntohll(t64);
    }
    
    memcpy(&fstime, pptrs->f_data+tpl->tpl[NF9_FIRST_SWITCHED].off, tpl->tpl[NF9_FIRST_SWITCHED].len);
    pdata->time_start = ntohl(((struct struct_header_v9 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v9 *) pptrs->f_header)->SysUptime)-ntohl(fstime))/1000);
    memcpy(&fstime, pptrs->f_data+tpl->tpl[NF9_LAST_SWITCHED].off, tpl->tpl[NF9_LAST_SWITCHED].len);
    pdata->time_end = ntohl(((struct struct_header_v9 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v9 *) pptrs->f_header)->SysUptime)-ntohl(fstime))/1000);
    break;
  case 8:
    switch(hdr->aggregation) {
    case 6:
      pdata->pkt_len = ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->First))/1000);
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->Last))/1000);
      break;
    case 7:
      pdata->pkt_len = ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->First))/1000);
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->Last))/1000);
      break;
    case 8:
      pdata->pkt_len = ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->First))/1000);
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->Last))/1000);
      break;
    default:
      pdata->pkt_len = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->First))/1000);
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->Last))/1000);
      break;
    }
    break;
  default:
    pdata->pkt_len = ntohl(((struct struct_export_v5 *) pptrs->f_data)->dOctets);
    pdata->pkt_num = ntohl(((struct struct_export_v5 *) pptrs->f_data)->dPkts);
    pdata->time_start = ntohl(((struct struct_header_v5 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v5 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v5 *) pptrs->f_data)->First))/1000); 
    pdata->time_end = ntohl(((struct struct_header_v5 *) pptrs->f_header)->unix_secs)-
      ((ntohl(((struct struct_header_v5 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v5 *) pptrs->f_data)->Last))/1000); 
    break;
  }
}

/* times from the netflow engine are in secs */
void NF_counters_secs_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  time_t fstime = 0;
  u_int32_t t32 = 0;
  u_int64_t t64 = 0;
  
  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_IN_BYTES].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_IN_BYTES].off, 4);
      pdata->pkt_len = ntohl(t32);
    }
    else if (tpl->tpl[NF9_IN_BYTES].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_IN_BYTES].off, 8);
      pdata->pkt_len = pm_ntohll(t64);
    }
    if (tpl->tpl[NF9_IN_PACKETS].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_IN_PACKETS].off, 4);
      pdata->pkt_num = ntohl(t32);
    }
    else if (tpl->tpl[NF9_IN_PACKETS].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_IN_PACKETS].off, 8);
      pdata->pkt_num = pm_ntohll(t64);
    }

    memcpy(&fstime, pptrs->f_data+tpl->tpl[NF9_FIRST_SWITCHED].off, tpl->tpl[NF9_FIRST_SWITCHED].len);
    pdata->time_start = ntohl(((struct struct_header_v9 *) pptrs->f_header)->unix_secs)-
      (ntohl(((struct struct_header_v9 *) pptrs->f_header)->SysUptime)-ntohl(fstime));
    memcpy(&fstime, pptrs->f_data+tpl->tpl[NF9_LAST_SWITCHED].off, tpl->tpl[NF9_LAST_SWITCHED].len);
    pdata->time_end = ntohl(((struct struct_header_v9 *) pptrs->f_header)->unix_secs)-
      (ntohl(((struct struct_header_v9 *) pptrs->f_header)->SysUptime)-ntohl(fstime));
    break;
  case 8:
    switch(hdr->aggregation) {
    case 6:
      pdata->pkt_len = ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->First));
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->Last));
      break;
    case 7:
      pdata->pkt_len = ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->First));
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->Last));
      break;
    case 8:
      pdata->pkt_len = ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->First));
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->Last));
      break;
    default:
      pdata->pkt_len = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dPkts);
      pdata->time_start = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->First));
      pdata->time_end = ntohl(((struct struct_header_v8 *) pptrs->f_header)->unix_secs)-
       (ntohl(((struct struct_header_v8 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->Last));
      break;
    }
    break;
  default:
    pdata->pkt_len = ntohl(((struct struct_export_v5 *) pptrs->f_data)->dOctets);
    pdata->pkt_num = ntohl(((struct struct_export_v5 *) pptrs->f_data)->dPkts);
    pdata->time_start = ntohl(((struct struct_header_v5 *) pptrs->f_header)->unix_secs)-
      (ntohl(((struct struct_header_v5 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v5 *) pptrs->f_data)->First));
    pdata->time_end = ntohl(((struct struct_header_v5 *) pptrs->f_header)->unix_secs)-
      (ntohl(((struct struct_header_v5 *) pptrs->f_header)->SysUptime)-ntohl(((struct struct_export_v5 *) pptrs->f_data)->Last));
    break;
  }
}

/* ignore netflow engine times and generate new ones */
void NF_counters_new_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int32_t t32 = 0;
  u_int64_t t64 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_IN_BYTES].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_IN_BYTES].off, 4);
      pdata->pkt_len = ntohl(t32);
    }
    else if (tpl->tpl[NF9_IN_BYTES].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_IN_BYTES].off, 8);
      pdata->pkt_len = pm_ntohll(t64);
    }
    if (tpl->tpl[NF9_IN_PACKETS].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_IN_PACKETS].off, 4);
      pdata->pkt_num = ntohl(t32);
    }
    else if (tpl->tpl[NF9_IN_PACKETS].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_IN_PACKETS].off, 8);
      pdata->pkt_num = pm_ntohll(t64);
    }

    pdata->time_start = 0;
    pdata->time_end = 0;
    break;
  case 8:
    switch(hdr->aggregation) {
    case 6:
      pdata->pkt_len = ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_6 *) pptrs->f_data)->dPkts);
      break;
    case 7:
      pdata->pkt_len = ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_7 *) pptrs->f_data)->dPkts);
      break;
    case 8:
      pdata->pkt_len = ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_8 *) pptrs->f_data)->dPkts);
      break;
    default:
      pdata->pkt_len = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dOctets);
      pdata->pkt_num = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dPkts);
      break;
    }
    pdata->time_start = 0;
    pdata->time_end = 0;
    break;
  default:
    pdata->pkt_len = ntohl(((struct struct_export_v5 *) pptrs->f_data)->dOctets);
    pdata->pkt_num = ntohl(((struct struct_export_v5 *) pptrs->f_data)->dPkts);
    pdata->time_start = 0;
    pdata->time_end = 0;
    break;
  }
}

void ptag_id_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  pdata->primitives.id = pptrs->tag;
}

void ptag_id2_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;

  pdata->primitives.id2 = pptrs->tag2;
}

void NF_flows_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int32_t t32 = 0;
  u_int64_t t64 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_FLOWS].len == 4) {
      memcpy(&t32, pptrs->f_data+tpl->tpl[NF9_FLOWS].off, 4);
      pdata->flo_num = ntohl(t32); 
    }
    else if (tpl->tpl[NF9_FLOWS].len == 8) {
      memcpy(&t64, pptrs->f_data+tpl->tpl[NF9_FLOWS].off, 8);
      pdata->flo_num = pm_ntohll(t64); 
    }
    if (!pdata->flo_num) pdata->flo_num = 1;
    break;
  case 8:
    switch(hdr->aggregation) {
    case 6:
    case 7:
    case 8:
      break;
    default:
      pdata->flo_num = ntohl(((struct struct_export_v8_1 *) pptrs->f_data)->dFlows);
      break;
    }
    break;
  default:
    pdata->flo_num = 1;
    break;
  }
}

void NF_sfprobe_payload_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_payload *payload = (struct pkt_payload *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  struct pkt_data tmp;
  char *buf = (char *) *data, *tmpp = (char *) &tmp;
  int space = (chptr->bufend - chptr->bufptr) - PpayloadSz;

  NF_counters_msecs_handler(chptr, pptrs, &tmpp);
  NF_class_handler(chptr, pptrs, &tmpp);
  NF_id_handler(chptr, pptrs, &tmpp); /* XXX: conditional? */
  NF_id2_handler(chptr, pptrs, &tmpp); /* XXX: conditional? */
  NF_src_host_handler(chptr, pptrs, &tmpp);
  NF_dst_host_handler(chptr, pptrs, &tmpp);

  payload->cap_len = ((struct pcap_pkthdr *)pptrs->pkthdr)->caplen; /* XXX */
  payload->pkt_len = tmp.pkt_len;
  payload->pkt_num = tmp.pkt_num;
  payload->time_start = tmp.time_start;
  payload->class = tmp.primitives.class;
  payload->tag = tmp.primitives.id;
  payload->tag2 = tmp.primitives.id2;

  if (chptr->plugin->cfg.nfacctd_as == NF_AS_NEW) {
    memcpy(&payload->src_ip, &tmp.primitives.src_ip, HostAddrSz);
    memcpy(&payload->dst_ip, &tmp.primitives.dst_ip, HostAddrSz);
  }

  if (space >= payload->cap_len) {
    buf += PpayloadSz;
    memcpy(buf, pptrs->packet_ptr, payload->cap_len);
    chptr->bufptr += payload->cap_len; /* don't count pkt_payload here */
  }
  else {
    chptr->bufptr += space;
    chptr->reprocess = TRUE;
  }
}

void NF_nfprobe_extras_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_extras *pextras = (struct pkt_extras *) ++pdata;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_MPLS_LABEL_1].len)
      memcpy(&pextras->mpls_top_label, pptrs->f_data+tpl->tpl[NF9_MPLS_LABEL_1].off, MIN(tpl->tpl[NF9_MPLS_LABEL_1].len, 4));
    if ((u_int8_t)*(pptrs->f_data+tpl->tpl[NF9_L4_PROTOCOL].off) == IPPROTO_TCP)
      memcpy(&pextras->tcp_flags, pptrs->f_data+tpl->tpl[NF9_TCP_FLAGS].off, tpl->tpl[NF9_TCP_FLAGS].len);
    break;
  default:
    pextras->mpls_top_label = 0;
    if (((struct struct_export_v5 *) pptrs->f_data)->prot == IPPROTO_TCP && hdr->version == 5)
      pextras->tcp_flags = ((struct struct_export_v5 *) pptrs->f_data)->tcp_flags;
    break;
  }
}

void NF_in_iface_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int16_t iface16 = 0;
  u_int32_t iface32 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_INPUT_SNMP].len == 2) {
      memcpy(&iface16, pptrs->f_data+tpl->tpl[NF9_INPUT_SNMP].off, 2);
      pdata->primitives.ifindex_in = ntohs(iface16);
    }
    else if (tpl->tpl[NF9_INPUT_SNMP].len == 4) {
      memcpy(&iface32, pptrs->f_data+tpl->tpl[NF9_INPUT_SNMP].off, 4);
      pdata->primitives.ifindex_in = ntohl(iface32);
    }
    break;
  case 8:
    switch(hdr->aggregation) {
    case 1:
      iface16 = ntohs(((struct struct_export_v8_1 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 3:
      iface16 = ntohs(((struct struct_export_v8_3 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 5:
      iface16 = ntohs(((struct struct_export_v8_5 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 7:
      iface16 = ntohs(((struct struct_export_v8_7 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 8:
      iface16 = ntohs(((struct struct_export_v8_8 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 9:
      iface16 = ntohs(((struct struct_export_v8_9 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 10:
      iface16 = ntohs(((struct struct_export_v8_10 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 11:
      iface16 = ntohs(((struct struct_export_v8_11 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 13:
      iface16 = ntohs(((struct struct_export_v8_13 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    case 14:
      iface16 = ntohs(((struct struct_export_v8_14 *) pptrs->f_data)->input);
      pdata->primitives.ifindex_in = iface16;
      break;
    default:
      pdata->primitives.ifindex_in = 0;
      break;
    }
    break;
  default:
    iface16 = ntohs(((struct struct_export_v5 *) pptrs->f_data)->input);
    pdata->primitives.ifindex_in = iface16;
    break;
  }
}

void NF_out_iface_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int16_t iface16 = 0;
  u_int32_t iface32 = 0;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_OUTPUT_SNMP].len == 2) {
      memcpy(&iface16, pptrs->f_data+tpl->tpl[NF9_OUTPUT_SNMP].off, 2);
      pdata->primitives.ifindex_out = ntohs(iface16);
    }
    else if (tpl->tpl[NF9_OUTPUT_SNMP].len == 4) {
      memcpy(&iface32, pptrs->f_data+tpl->tpl[NF9_OUTPUT_SNMP].off, 4);
      pdata->primitives.ifindex_out = ntohl(iface32);
    }
    break;
  case 8:
    switch(hdr->aggregation) {
    case 1:
      iface16 = ntohs(((struct struct_export_v8_1 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 4:
      iface16 = ntohs(((struct struct_export_v8_4 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 5:
      iface16 = ntohs(((struct struct_export_v8_5 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 6:
      iface16 = ntohs(((struct struct_export_v8_6 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 7:
      iface16 = ntohs(((struct struct_export_v8_7 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 8:
      iface16 = ntohs(((struct struct_export_v8_8 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 9:
      iface16 = ntohs(((struct struct_export_v8_9 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 10:
      iface16 = ntohs(((struct struct_export_v8_10 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 12:
      iface16 = ntohs(((struct struct_export_v8_12 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 13:
      iface16 = ntohs(((struct struct_export_v8_13 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    case 14:
      iface16 = ntohs(((struct struct_export_v8_14 *) pptrs->f_data)->output);
      pdata->primitives.ifindex_out = iface16;
      break;
    default:
      pdata->primitives.ifindex_out = 0;
      break;
    }
    break;
  default:
    iface16 = ntohs(((struct struct_export_v5 *) pptrs->f_data)->output);
    pdata->primitives.ifindex_out = iface16;
    break;
  }
}

void NF_class_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  time_t fstime;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_CUST_CLASS].len) { 
      pdata->primitives.class = pptrs->class; 
      pdata->cst.ba = 0; 
      pdata->cst.pa = 0; 
      pdata->cst.fa = 0; 

      if (tpl->tpl[NF9_FIRST_SWITCHED].len) {
        memcpy(&fstime, pptrs->f_data+tpl->tpl[NF9_FIRST_SWITCHED].off, tpl->tpl[NF9_FIRST_SWITCHED].len);
        pdata->cst.stamp.tv_sec = ntohl(((struct struct_header_v9 *) pptrs->f_header)->unix_secs)-
           ((ntohl(((struct struct_header_v9 *) pptrs->f_header)->SysUptime)-ntohl(fstime))/1000);
      }
      else pdata->cst.stamp.tv_sec = time(NULL);
      pdata->cst.stamp.tv_usec = 0; 
    }
    break;
  default:
    break;
  }
}

void NF_id_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_CUST_TAG].len) {
      memcpy(&pdata->primitives.id, pptrs->f_data+tpl->tpl[NF9_CUST_TAG].off, tpl->tpl[NF9_CUST_TAG].len);
      pdata->primitives.id = ntohl(pdata->primitives.id);
    }
    break;
  default:
    break;
  }
}

void NF_id2_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;

  switch(hdr->version) {
  case 9:
    if (tpl->tpl[NF9_CUST_TAG2].len) {
      memcpy(&pdata->primitives.id2, pptrs->f_data+tpl->tpl[NF9_CUST_TAG2].off, tpl->tpl[NF9_CUST_TAG2].len);
      pdata->primitives.id2 = ntohl(pdata->primitives.id2);
    }
    break;
  default:
    break;
  }
}

void NF_counters_renormalize_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct xflow_status_entry *entry = (struct xflow_status_entry *) pptrs->f_status;
  struct xflow_status_entry_sampling *sentry;
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct struct_header_v8 *hdr = (struct struct_header_v8 *) pptrs->f_header;
  struct struct_header_v5 *hdr5 = (struct struct_header_v5 *) pptrs->f_header;
  struct template_cache_entry *tpl = (struct template_cache_entry *) pptrs->f_tpl;
  u_int16_t srate = 0, is_sampled = 0;
  u_int8_t sampler_id = 0;

  switch (hdr->version) {
  case 9:
    memcpy(&sampler_id, pptrs->f_data+tpl->tpl[NF9_FLOW_SAMPLER_ID].off, MIN(tpl->tpl[NF9_FLOW_SAMPLER_ID].len, 1));
    sentry = search_smp_id_status_table(entry->sampling, sampler_id);
    if (sentry) {
      pdata->pkt_len = pdata->pkt_len * sentry->sample_pool;
      pdata->pkt_num = pdata->pkt_num * sentry->sample_pool;
    }
    break;
  case 5:
    hdr5 = (struct struct_header_v5 *) pptrs->f_header;
    is_sampled = ( ntohs(hdr5->sampling) & 0xC000 );
    srate = ( ntohs(hdr5->sampling) & 0x3FFF );
    /* XXX: checking srate value instead of is_sampled as Sampling
       Mode seems not to be a mandatory field. */
    if (srate) {
      pdata->pkt_len = pdata->pkt_len * srate;
      pdata->pkt_num = pdata->pkt_num * srate;
    }
    break;
  default:
    break;
  }
}

void bgp_ext_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src; 
  struct bgp_node *dst_ret = (struct bgp_node *) pptrs->bgp_dst;
  struct bgp_peer *peer = (struct bgp_peer *) pptrs->bgp_peer;
  struct bgp_info *info;

  --pdata; /* Bringing back to original place */

  if (src_ret) {
    info = (struct bgp_info *) src_ret->info;
    if (info && info->attr) {
      if (config.nfacctd_as == NF_AS_BGP) {
	if (chptr->aggregation & COUNT_SRC_AS && info->attr->aspath) {
	  pdata->primitives.src_as = evaluate_last_asn(info->attr->aspath);

	  if (!pdata->primitives.src_as && config.nfacctd_bgp_stdcomm_pattern_to_asn) {
	    char tmp_stdcomms[MAX_BGP_STD_COMMS];

	    if (info->attr->community && info->attr->community->str) {
	      evaluate_comm_patterns(tmp_stdcomms, info->attr->community->str, std_comm_patterns_to_asn, MAX_BGP_STD_COMMS);
	      copy_stdcomm_to_asn(tmp_stdcomms, &pdata->primitives.src_as, TRUE);
	    }
	  }
	}
      }
      if (chptr->aggregation & COUNT_SRC_AS_PATH && config.nfacctd_bgp_src_as_path_type == BGP_SRC_PRIMITIVES_BGP && info->attr->aspath && info->attr->aspath->str) {
	strlcpy(pbgp->src_as_path, info->attr->aspath->str, MAX_BGP_ASPATH);
	if (strlen(info->attr->aspath->str) >= MAX_BGP_ASPATH)
	  pbgp->src_as_path[MAX_BGP_ASPATH-1] = '+';
	if (config.nfacctd_bgp_aspath_radius)
	  evaluate_bgp_aspath_radius(pbgp->src_as_path, MAX_BGP_ASPATH, config.nfacctd_bgp_aspath_radius);
      }
      if (chptr->aggregation & COUNT_SRC_STD_COMM && config.nfacctd_bgp_src_std_comm_type == BGP_SRC_PRIMITIVES_BGP && info->attr->community && info->attr->community->str) {
	if (config.nfacctd_bgp_stdcomm_pattern)
	  evaluate_comm_patterns(pbgp->src_std_comms, info->attr->community->str, std_comm_patterns, MAX_BGP_STD_COMMS);
	else {
	  strlcpy(pbgp->src_std_comms, info->attr->community->str, MAX_BGP_STD_COMMS);
	  if (strlen(info->attr->community->str) >= MAX_BGP_STD_COMMS)
	    pbgp->src_std_comms[MAX_BGP_STD_COMMS-1] = '+';
	}
      }
      if (chptr->aggregation & COUNT_SRC_EXT_COMM && config.nfacctd_bgp_src_ext_comm_type == BGP_SRC_PRIMITIVES_BGP && info->attr->ecommunity && info->attr->ecommunity->str) {
        if (config.nfacctd_bgp_extcomm_pattern)
          evaluate_comm_patterns(pbgp->src_ext_comms, info->attr->ecommunity->str, ext_comm_patterns, MAX_BGP_EXT_COMMS);
        else {
          strlcpy(pbgp->src_ext_comms, info->attr->ecommunity->str, MAX_BGP_EXT_COMMS);
          if (strlen(info->attr->ecommunity->str) >= MAX_BGP_EXT_COMMS)
            pbgp->src_ext_comms[MAX_BGP_EXT_COMMS-1] = '+';
        }
      }
      if (chptr->aggregation & COUNT_SRC_LOCAL_PREF && config.nfacctd_bgp_src_local_pref_type == BGP_SRC_PRIMITIVES_BGP)
	pbgp->src_local_pref = info->attr->local_pref;

      if (chptr->aggregation & COUNT_SRC_MED && config.nfacctd_bgp_src_med_type == BGP_SRC_PRIMITIVES_BGP)
	pbgp->src_med = info->attr->med;

      if (chptr->aggregation & COUNT_PEER_SRC_AS && config.nfacctd_bgp_peer_as_src_type == BGP_SRC_PRIMITIVES_BGP && info->attr->aspath && info->attr->aspath->str) {
        pbgp->peer_src_as = evaluate_first_asn(info->attr->aspath->str);

        if (!pbgp->peer_src_as && config.nfacctd_bgp_stdcomm_pattern_to_asn) {
          char tmp_stdcomms[MAX_BGP_STD_COMMS];

          if (info->attr->community && info->attr->community->str) {
            evaluate_comm_patterns(tmp_stdcomms, info->attr->community->str, std_comm_patterns_to_asn, MAX_BGP_STD_COMMS);
            copy_stdcomm_to_asn(tmp_stdcomms, &pbgp->peer_src_as, FALSE);
          }
        }
      }
    }
  }

  if (dst_ret) {
    info = (struct bgp_info *) dst_ret->info;
    if (info && info->attr) {
      if (chptr->aggregation & COUNT_STD_COMM && info->attr->community && info->attr->community->str) {
	if (config.nfacctd_bgp_stdcomm_pattern)
	  evaluate_comm_patterns(pbgp->std_comms, info->attr->community->str, std_comm_patterns, MAX_BGP_STD_COMMS);
	else {
          strlcpy(pbgp->std_comms, info->attr->community->str, MAX_BGP_STD_COMMS);
	  if (strlen(info->attr->community->str) >= MAX_BGP_STD_COMMS)
	    pbgp->std_comms[MAX_BGP_STD_COMMS-1] = '+';
	}
      }
      if (chptr->aggregation & COUNT_EXT_COMM && info->attr->ecommunity && info->attr->ecommunity->str) {
	if (config.nfacctd_bgp_extcomm_pattern)
	  evaluate_comm_patterns(pbgp->ext_comms, info->attr->ecommunity->str, ext_comm_patterns, MAX_BGP_EXT_COMMS);
	else {
          strlcpy(pbgp->ext_comms, info->attr->ecommunity->str, MAX_BGP_EXT_COMMS);
	  if (strlen(info->attr->ecommunity->str) >= MAX_BGP_EXT_COMMS)
	    pbgp->ext_comms[MAX_BGP_EXT_COMMS-1] = '+';
	}
      }
      if (chptr->aggregation & COUNT_AS_PATH && info->attr->aspath && info->attr->aspath->str) {
        strlcpy(pbgp->as_path, info->attr->aspath->str, MAX_BGP_ASPATH);
	if (strlen(info->attr->aspath->str) >= MAX_BGP_ASPATH)
	  pbgp->as_path[MAX_BGP_ASPATH-1] = '+';
	if (config.nfacctd_bgp_aspath_radius)
	  evaluate_bgp_aspath_radius(pbgp->as_path, MAX_BGP_ASPATH, config.nfacctd_bgp_aspath_radius);
      }
      if (config.nfacctd_as == NF_AS_BGP) {
        if (chptr->aggregation & COUNT_DST_AS && info->attr->aspath) {
          pdata->primitives.dst_as = evaluate_last_asn(info->attr->aspath);

          if (!pdata->primitives.dst_as && config.nfacctd_bgp_stdcomm_pattern_to_asn) {
            char tmp_stdcomms[MAX_BGP_STD_COMMS];

            if (info->attr->community && info->attr->community->str) {
              evaluate_comm_patterns(tmp_stdcomms, info->attr->community->str, std_comm_patterns_to_asn, MAX_BGP_STD_COMMS);
              copy_stdcomm_to_asn(tmp_stdcomms, &pdata->primitives.dst_as, TRUE);
            }
	  }
        }
      }

      if (chptr->aggregation & COUNT_LOCAL_PREF) pbgp->local_pref = info->attr->local_pref;

      if (chptr->aggregation & COUNT_MED) pbgp->med = info->attr->med;

      if (chptr->aggregation & COUNT_PEER_DST_AS && info->attr->aspath && info->attr->aspath->str) {
        pbgp->peer_dst_as = evaluate_first_asn(info->attr->aspath->str);

        if (!pbgp->peer_dst_as && config.nfacctd_bgp_stdcomm_pattern_to_asn) {
          char tmp_stdcomms[MAX_BGP_STD_COMMS];

          if (info->attr->community && info->attr->community->str) {
            evaluate_comm_patterns(tmp_stdcomms, info->attr->community->str, std_comm_patterns_to_asn, MAX_BGP_STD_COMMS);
            copy_stdcomm_to_asn(tmp_stdcomms, &pbgp->peer_dst_as, FALSE);
          }
        }
      }
      if (chptr->aggregation & COUNT_PEER_DST_IP) {
	struct bgp_node *nh;
	struct bgp_info *nh_info;

	if (config.nfacctd_as == NF_AS_BGP) {
	  if (pptrs->bgp_nexthop) { 
	    nh = (struct bgp_node *) pptrs->bgp_nexthop;
	    nh_info = nh->info;
	  }
	  else 
	    nh_info = info;

	  if (nh_info->attr->mp_nexthop.family == AF_INET) {
	    pbgp->peer_dst_ip.family = AF_INET;
	    memcpy(&pbgp->peer_dst_ip.address.ipv4, &nh_info->attr->mp_nexthop.address.ipv4, 4);
	  }
#if defined ENABLE_IPV6
	  else if (nh_info->attr->mp_nexthop.family == AF_INET6) {
	    pbgp->peer_dst_ip.family = AF_INET6;
	    memcpy(&pbgp->peer_dst_ip.address.ipv6, &nh_info->attr->mp_nexthop.address.ipv6, 16);
	  }
#endif
	  else {
	    pbgp->peer_dst_ip.family = AF_INET; 
	    pbgp->peer_dst_ip.address.ipv4.s_addr = nh_info->attr->nexthop.s_addr;
	  }
	}
      }
    }
  }

  if (chptr->aggregation & COUNT_PEER_SRC_IP && peer) {
    if (config.nfacctd_as == NF_AS_BGP) {
      if (!pptrs->bta) memcpy(&pbgp->peer_src_ip, &peer->addr, sizeof(struct host_addr)); 
      else {
        struct sockaddr *sa = (struct sockaddr *) pptrs->f_agent;

        pbgp->peer_src_ip.family = sa->sa_family;
        if (sa->sa_family == AF_INET) pbgp->peer_src_ip.address.ipv4.s_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
#if defined ENABLE_IPV6
        else if (sa->sa_family == AF_INET6) memcpy(&pbgp->peer_src_ip.address.ipv6, &((struct sockaddr_in6 *)sa)->sin6_addr, 16); 
#endif
      }
    }
  }
}

void sfprobe_bgp_ext_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_payload *payload = (struct pkt_payload *) *data;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src; 
  struct bgp_node *dst_ret = (struct bgp_node *) pptrs->bgp_dst;
  struct bgp_peer *peer = (struct bgp_peer *) pptrs->bgp_peer;
  struct bgp_info *info;

  if (src_ret) {
    info = (struct bgp_info *) src_ret->info;
    if (info && info->attr) {
      if (config.nfacctd_as == NF_AS_BGP) {
	if (chptr->aggregation & COUNT_SRC_AS && info->attr->aspath) {
	  if (!chptr->plugin->cfg.nfprobe_peer_as)
	    payload->src_as = evaluate_last_asn(info->attr->aspath);
	  else
            payload->src_as = evaluate_first_asn(info->attr->aspath->str);
	}
      }
    }
  }

  if (dst_ret) {
    info = (struct bgp_info *) dst_ret->info;
    if (info && info->attr) {
      if (config.nfacctd_as == NF_AS_BGP) {
        if (chptr->aggregation & COUNT_DST_AS && info->attr->aspath) {
	  if (!chptr->plugin->cfg.nfprobe_peer_as)
            payload->dst_as = evaluate_last_asn(info->attr->aspath);
          else
	    payload->dst_as = evaluate_first_asn(info->attr->aspath->str);
	}
      }
    }
  }
}

void nfprobe_bgp_ext_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src;
  struct bgp_node *dst_ret = (struct bgp_node *) pptrs->bgp_dst;
  struct bgp_peer *peer = (struct bgp_peer *) pptrs->bgp_peer;
  struct bgp_info *info;

  if (src_ret) {
    info = (struct bgp_info *) src_ret->info;
    if (info && info->attr) {
      if (config.nfacctd_as == NF_AS_BGP) {
        if (chptr->aggregation & COUNT_SRC_AS && info->attr->aspath) {
          if (!chptr->plugin->cfg.nfprobe_peer_as)
            pdata->primitives.src_as = evaluate_last_asn(info->attr->aspath);
          else
            pdata->primitives.src_as = evaluate_first_asn(info->attr->aspath->str);
        }
      }
    }
  }

  if (dst_ret) {
    info = (struct bgp_info *) dst_ret->info;
    if (info && info->attr) {
      if (config.nfacctd_as == NF_AS_BGP) {
        if (chptr->aggregation & COUNT_DST_AS && info->attr->aspath) {
          if (!chptr->plugin->cfg.nfprobe_peer_as)
            pdata->primitives.dst_as = evaluate_last_asn(info->attr->aspath);
          else
            pdata->primitives.dst_as = evaluate_first_asn(info->attr->aspath->str);
        }
      }
    }
  }
}

void bgp_peer_src_as_frommap_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src;
  struct bgp_info *info;

  --pdata; /* Bringing back to original place */

  pbgp->peer_src_as = pptrs->bpas;

  /* XXX: extra check: was src_as written by copy_stdcomm_to_asn() ? */

  if (!pbgp->peer_src_as && config.nfacctd_bgp_stdcomm_pattern_to_asn) {
    if (src_ret) {
      char tmp_stdcomms[MAX_BGP_STD_COMMS];

      info = (struct bgp_info *) src_ret->info;

      if (info && info->attr && info->attr->community && info->attr->community->str) {
        evaluate_comm_patterns(tmp_stdcomms, info->attr->community->str, std_comm_patterns_to_asn, MAX_BGP_STD_COMMS);
        copy_stdcomm_to_asn(tmp_stdcomms, &pbgp->peer_src_as, FALSE);
      }
    }
  }
}

void bgp_src_local_pref_frommap_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src;
  struct bgp_info *info;

  --pdata; /* Bringing back to original place */

  pbgp->src_local_pref = pptrs->blp;
}

void bgp_src_med_frommap_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src;
  struct bgp_info *info;

  --pdata; /* Bringing back to original place */

  pbgp->src_med = pptrs->bmed;
}

void bgp_is_symmetric_frommap_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  struct bgp_node *src_ret = (struct bgp_node *) pptrs->bgp_src;
  struct bgp_info *info;

  --pdata; /* Bringing back to original place */

  pbgp->is_symmetric = pptrs->biss;
}

#if defined (HAVE_L2)
void SF_src_mac_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  memcpy(pdata->primitives.eth_shost, sample->eth_src, ETH_ADDR_LEN);
}

void SF_dst_mac_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  memcpy(pdata->primitives.eth_dhost, sample->eth_dst, ETH_ADDR_LEN);
}

void SF_vlan_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;
  
  pdata->primitives.vlan_id = sample->in_vlan;
}
#endif

void SF_src_host_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;
  SFLAddress *addr = &sample->ipsrc;
  u_int8_t src_mask = sample->srcMask;

  if (sample->gotIPV4) {
    pdata->primitives.src_ip.address.ipv4.s_addr = sample->dcd_srcIP.s_addr;
    pdata->primitives.src_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6
  else if (sample->gotIPV6) { 
    memcpy(&pdata->primitives.src_ip.address.ipv6, &addr->address.ip_v6, IP6AddrSz);
    pdata->primitives.src_ip.family = AF_INET6;
  }
#endif

  if ((chptr->aggregation & (COUNT_SRC_NET|COUNT_SUM_NET)) && chptr->plugin->cfg.nfacctd_net == NF_NET_KEEP) {
    u_int32_t maskbits[4], addrh[4];
    u_int8_t j;

    memset(maskbits, 0, sizeof(maskbits));
    for (j = 0; j < 4 && src_mask >= 32; j++, src_mask -= 32) maskbits[j] = 0xffffffffU;
    if (j < 4 && src_mask) maskbits[j] = ~(0xffffffffU >> src_mask);

    if (pdata->primitives.src_ip.family == AF_INET) {
      addrh[0] = ntohl(pdata->primitives.src_ip.address.ipv4.s_addr);
      addrh[0] &= maskbits[0];
      pdata->primitives.src_ip.address.ipv4.s_addr = htonl(addrh[0]);
    }
#if defined ENABLE_IPV6
    else if (pdata->primitives.src_ip.family == AF_INET6) {
      memcpy(&addrh, (void *) pm_ntohl6(&pdata->primitives.src_ip.address.ipv6), IP6AddrSz);
      for (j = 0; j < 4; j++) addrh[j] &= maskbits[j];
      memcpy(&pdata->primitives.src_ip.address.ipv6, (void *) pm_htonl6(addrh), IP6AddrSz);
    }
#endif
  }
}

void SF_dst_host_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;
  SFLAddress *addr = &sample->ipdst;
  u_int8_t dst_mask = sample->dstMask;

  if (sample->gotIPV4) { 
    pdata->primitives.dst_ip.address.ipv4.s_addr = sample->dcd_dstIP.s_addr; 
    pdata->primitives.dst_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6
  else if (sample->gotIPV6) { 
    memcpy(&pdata->primitives.dst_ip.address.ipv6, &addr->address.ip_v6, IP6AddrSz);
    pdata->primitives.dst_ip.family = AF_INET6;
  }
#endif

  if ((chptr->aggregation & (COUNT_DST_NET|COUNT_SUM_NET)) && chptr->plugin->cfg.nfacctd_net == NF_NET_KEEP) {
    u_int32_t maskbits[4], addrh[4];
    u_int8_t j;

    memset(maskbits, 0, sizeof(maskbits));
    for (j = 0; j < 4 && dst_mask >= 32; j++, dst_mask -= 32) maskbits[j] = 0xffffffffU;
    if (j < 4 && dst_mask) maskbits[j] = ~(0xffffffffU >> dst_mask);

    if (pdata->primitives.dst_ip.family == AF_INET) {
      addrh[0] = ntohl(pdata->primitives.dst_ip.address.ipv4.s_addr);
      addrh[0] &= maskbits[0];
      pdata->primitives.dst_ip.address.ipv4.s_addr = htonl(addrh[0]);
    }
#if defined ENABLE_IPV6
    else if (pdata->primitives.dst_ip.family == AF_INET6) {
      memcpy(&addrh, (void *) pm_ntohl6(&pdata->primitives.dst_ip.address.ipv6), IP6AddrSz);
      for (j = 0; j < 4; j++) addrh[j] &= maskbits[j];
      memcpy(&pdata->primitives.dst_ip.address.ipv6, (void *) pm_htonl6(addrh), IP6AddrSz);
    }
#endif
  }
}

void SF_src_nmask_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.src_nmask = sample->srcMask;
}

void SF_dst_nmask_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.dst_nmask = sample->dstMask;
}

void SF_src_port_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  if (sample->dcd_ipProtocol == IPPROTO_UDP || sample->dcd_ipProtocol == IPPROTO_TCP)
    pdata->primitives.src_port = sample->dcd_sport; 
  else pdata->primitives.src_port = 0;
}

void SF_dst_port_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  if (sample->dcd_ipProtocol == IPPROTO_UDP || sample->dcd_ipProtocol == IPPROTO_TCP)
    pdata->primitives.dst_port = sample->dcd_dport;
  else pdata->primitives.dst_port = 0;
}

void SF_ip_tos_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.tos = sample->dcd_ipTos;
}

void SF_ip_proto_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.proto = sample->dcd_ipProtocol; 
}

void SF_tcp_flags_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->tcp_flags = sample->dcd_tcpFlags; 
}

void SF_counters_new_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->pkt_len = sample->sampledPacketSize;
  pdata->pkt_num = 1;
  pdata->time_start = 0;
  pdata->time_end = 0;

  /* XXX: fragment handling */
}

void SF_counters_renormalize_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct xflow_status_entry *entry = (struct xflow_status_entry *) pptrs->f_status;
  struct xflow_status_entry_sampling *sentry;
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;
  u_int32_t eff_srate = 0;

  sentry = search_smp_if_status_table(entry->sampling, (sample->ds_class << 24 | sample->ds_index));
  if (sentry) { 
    /* flow sequence number is strictly increasing; however we need a) to avoid
       a division-by-zero by checking the last value and the new one and b) to
       deal with out-of-order datagrams */
    if (sample->samplesGenerated > sentry->seqno && sample->samplePool > sentry->sample_pool) {
      eff_srate = (sample->samplePool-sentry->sample_pool) / (sample->samplesGenerated-sentry->seqno);
      pdata->pkt_len = pdata->pkt_len * eff_srate;
      pdata->pkt_num = pdata->pkt_num * eff_srate;

      sentry->sample_pool = sample->samplePool;
      sentry->seqno = sample->samplesGenerated;

      return;
    }
    /* Let's handle long positive/negative jumps as resets */ 
    else if (MAX(sample->samplesGenerated, sentry->seqno) >
	    (MIN(sample->samplesGenerated, sentry->seqno)+XFLOW_RESET_BOUNDARY)) {
      sentry->sample_pool = sample->samplePool;
      sentry->seqno = sample->samplesGenerated;
    }
  }
  else {
    sentry = create_smp_entry_status_table(entry);
    if (sentry) {
      sentry->interface = (sample->ds_class << 24 | sample->ds_index);
      sentry->sample_pool = sample->samplePool;
      sentry->seqno = sample->samplesGenerated; 
    }
  }

  pdata->pkt_len = pdata->pkt_len * sample->meanSkipCount;
  pdata->pkt_num = pdata->pkt_num * sample->meanSkipCount;
}

void SF_src_as_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;
  
  pdata->primitives.src_as = sample->src_as;
}

void SF_dst_as_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.dst_as = sample->dst_as;
}

void SF_peer_src_ip_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  SFSample *sample = (SFSample *) pptrs->f_data;

  --pdata; /* Bringing back to original place */

  if (sample->agent_addr.type == SFLADDRESSTYPE_IP_V4) {
    pbgp->peer_src_ip.address.ipv4.s_addr = sample->agent_addr.address.ip_v4.s_addr;
    pbgp->peer_src_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6
  else if (sample->agent_addr.type == SFLADDRESSTYPE_IP_V6) {
    memcpy(&pbgp->peer_src_ip.address.ipv6, &sample->agent_addr.address.ip_v6, IP6AddrSz);
    pbgp->peer_src_ip.family = AF_INET6;
  }
#endif
}

void SF_peer_dst_ip_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;
  SFSample *sample = (SFSample *) pptrs->f_data;

  --pdata; /* Bringing back to original place */

  if (sample->bgp_nextHop.type == SFLADDRESSTYPE_IP_V4) {
    pbgp->peer_dst_ip.address.ipv4.s_addr = sample->bgp_nextHop.address.ip_v4.s_addr;
    pbgp->peer_dst_ip.family = AF_INET;
  }
#if defined ENABLE_IPV6
  else if (sample->bgp_nextHop.type == SFLADDRESSTYPE_IP_V6) {
    memcpy(&pbgp->peer_dst_ip.address.ipv6, &sample->bgp_nextHop.address.ip_v6, IP6AddrSz);
    pbgp->peer_dst_ip.family = AF_INET6;
  }
#endif
}

void SF_sfprobe_payload_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_payload *payload = (struct pkt_payload *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;
  struct pkt_data tmp;
  char *buf = (char *) *data, *tmpp = (char *) &tmp;
  int space = (chptr->bufend - chptr->bufptr) - PpayloadSz;

  if (chptr->plugin->cfg.nfacctd_as == NF_AS_NEW) {
    SF_src_host_handler(chptr, pptrs, &tmpp);
    SF_dst_host_handler(chptr, pptrs, &tmpp);
    memcpy(&payload->src_ip, &tmp.primitives.src_ip, HostAddrSz);
    memcpy(&payload->dst_ip, &tmp.primitives.dst_ip, HostAddrSz);
  }

  payload->cap_len = sample->headerLen;
  payload->pkt_len = sample->sampledPacketSize;
  payload->pkt_len = 1; 
  payload->time_start = time(NULL); /* XXX */
  payload->class = sample->class;
  payload->tag = sample->tag;
  payload->tag2 = sample->tag2;

  if (space >= payload->cap_len) {
    buf += PpayloadSz;
    memcpy(buf, sample->header, payload->cap_len);
    chptr->bufptr += payload->cap_len; /* don't count pkt_payload here */
  }
  else {
    chptr->bufptr += space;
    chptr->reprocess = TRUE;
  }
}

void SF_nfprobe_extras_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_extras *pextras = (struct pkt_extras *) ++pdata;
  SFSample *sample = (SFSample *) pptrs->f_data;

  if (sample->lstk.depth) memcpy(&pextras->mpls_top_label, &sample->lstk.stack[0], 4);
  if (sample->dcd_ipProtocol == IPPROTO_TCP) pextras->tcp_flags = sample->dcd_tcpFlags;
}

void SF_in_iface_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.ifindex_in = sample->inputPort;
}

void SF_out_iface_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.ifindex_out = sample->outputPort;
}

void SF_class_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.class = sample->class;
  pdata->cst.ba = 0;
  pdata->cst.pa = 0;
  pdata->cst.fa = 0;

  pdata->cst.stamp.tv_sec = time(NULL); /* XXX */
  pdata->cst.stamp.tv_usec = 0;
}

void SF_id_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.id = sample->tag;
}

void SF_id2_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  SFSample *sample = (SFSample *) pptrs->f_data;

  pdata->primitives.id2 = sample->tag2;
}

void sampling_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  pm_counter_t sample_pool = 0;

  evaluate_sampling(&chptr->s, &pdata->pkt_len, &pdata->pkt_num, &sample_pool);
}

void sfprobe_sampling_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_payload *payload = (struct pkt_payload *) *data;

  evaluate_sampling(&chptr->s, &payload->pkt_len, &payload->pkt_num, &payload->sample_pool);
}

void SF_bgp_peer_src_as_fromstd_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;

  pbgp->peer_src_as = 0;

  // XXX: fill this in
}

void SF_bgp_peer_src_as_fromext_handler(struct channels_list_entry *chptr, struct packet_ptrs *pptrs, char **data)
{
  struct pkt_data *pdata = (struct pkt_data *) *data;
  struct pkt_bgp_primitives *pbgp = (struct pkt_bgp_primitives *) ++pdata;

  pbgp->peer_src_as = 0;

  // XXX: fill this in
}
