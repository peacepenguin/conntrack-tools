/*
 * (C) 2006-2008 by Pablo Neira Ayuso <pablo@netfilter.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include "network.h"
#include "conntrackd.h"

static inline void *
put_header(struct nethdr *n, int attr, size_t len)
{
	struct netattr *nta = NETHDR_TAIL(n);
	int total_size = NTA_ALIGN(NTA_LENGTH(len));
	int attr_size = NTA_LENGTH(len);
	n->len += total_size;
	nta->nta_attr = htons(attr);
	nta->nta_len = htons(attr_size);
	memset((unsigned char *)nta + attr_size, 0, total_size - attr_size);
	return NTA_DATA(nta);
}

static inline void
addattr(struct nethdr *n, int attr, const void *data, size_t len)
{
	void *ptr = put_header(n, attr, len);
	memcpy(ptr, data, len);
}

static inline void
__build_u8(const struct nf_conntrack *ct, int a, struct nethdr *n, int b)
{
	void *ptr = put_header(n, b, sizeof(uint8_t));
	memcpy(ptr, nfct_get_attr(ct, a), sizeof(uint8_t));
}

static inline void 
__build_u16(const struct nf_conntrack *ct, int a, struct nethdr *n, int b)
{
	uint32_t data = nfct_get_attr_u16(ct, a);
	data = htons(data);
	addattr(n, b, &data, sizeof(uint16_t));
}

static inline void 
__build_u32(const struct nf_conntrack *ct, int a, struct nethdr *n, int b)
{
	uint32_t data = nfct_get_attr_u32(ct, a);
	data = htonl(data);
	addattr(n, b, &data, sizeof(uint32_t));
}

static inline void 
__build_group(const struct nf_conntrack *ct, int a, struct nethdr *n, 
	      int b, int size)
{
	void *ptr = put_header(n, b, size);
	nfct_get_attr_grp(ct, a, ptr);
}

static inline void 
__build_natseqadj(const struct nf_conntrack *ct, struct nethdr *n)
{
	struct nta_attr_natseqadj data = {
		.orig_seq_correction_pos =
		htonl(nfct_get_attr_u32(ct, ATTR_ORIG_NAT_SEQ_CORRECTION_POS)),
		.orig_seq_offset_before = 
		htonl(nfct_get_attr_u32(ct, ATTR_ORIG_NAT_SEQ_OFFSET_BEFORE)),
		.orig_seq_offset_after =
		htonl(nfct_get_attr_u32(ct, ATTR_ORIG_NAT_SEQ_OFFSET_AFTER)),
		.repl_seq_correction_pos = 
		htonl(nfct_get_attr_u32(ct, ATTR_REPL_NAT_SEQ_CORRECTION_POS)),
		.repl_seq_offset_before =
		htonl(nfct_get_attr_u32(ct, ATTR_REPL_NAT_SEQ_OFFSET_BEFORE)),
		.repl_seq_offset_after = 
		htonl(nfct_get_attr_u32(ct, ATTR_REPL_NAT_SEQ_OFFSET_AFTER))
	};
	addattr(n, NTA_NAT_SEQ_ADJ, &data, sizeof(struct nta_attr_natseqadj));
}

static inline void 
__build_sctp(const struct nf_conntrack *ct, struct nethdr *n)
{
	struct nta_attr_sctp data = {
		.state = nfct_get_attr_u8(ct, ATTR_SCTP_STATE),
		.vtag_orig = htonl(nfct_get_attr_u32(ct, ATTR_SCTP_VTAG_ORIG)),
		.vtag_repl = htonl(nfct_get_attr_u32(ct, ATTR_SCTP_VTAG_REPL)),
	};
	addattr(n, NTA_STATE_SCTP, &data, sizeof(struct nta_attr_sctp));
}

static inline void 
__build_dccp(const struct nf_conntrack *ct, struct nethdr *n)
{
	struct nta_attr_dccp data = {
		.state = nfct_get_attr_u8(ct, ATTR_DCCP_STATE),
		.role = nfct_get_attr_u8(ct, ATTR_DCCP_ROLE),
	};
	addattr(n, NTA_STATE_DCCP, &data, sizeof(struct nta_attr_dccp));
}

static enum nf_conntrack_attr nat_type[] =
	{ ATTR_ORIG_NAT_SEQ_CORRECTION_POS, ATTR_ORIG_NAT_SEQ_OFFSET_BEFORE,
	  ATTR_ORIG_NAT_SEQ_OFFSET_AFTER, ATTR_REPL_NAT_SEQ_CORRECTION_POS,
	  ATTR_REPL_NAT_SEQ_OFFSET_BEFORE, ATTR_REPL_NAT_SEQ_OFFSET_AFTER };

/* XXX: ICMP not supported */
void build_payload(const struct nf_conntrack *ct, struct nethdr *n)
{
	if (nfct_attr_grp_is_set(ct, ATTR_GRP_ORIG_IPV4)) {
		__build_group(ct, ATTR_GRP_ORIG_IPV4, n, NTA_IPV4, 
			      sizeof(struct nfct_attr_grp_ipv4));
	} else if (nfct_attr_grp_is_set(ct, ATTR_GRP_ORIG_IPV6)) {
		__build_group(ct, ATTR_GRP_ORIG_IPV6, n, NTA_IPV6, 
			      sizeof(struct nfct_attr_grp_ipv6));
	}

	__build_u8(ct, ATTR_L4PROTO, n, NTA_L4PROTO);
	if (nfct_attr_grp_is_set(ct, ATTR_GRP_ORIG_PORT)) {
		__build_group(ct, ATTR_GRP_ORIG_PORT, n, NTA_PORT,
			      sizeof(struct nfct_attr_grp_port));
	}

	__build_u32(ct, ATTR_STATUS, n, NTA_STATUS); 

	if (nfct_attr_is_set(ct, ATTR_TCP_STATE))
		__build_u8(ct, ATTR_TCP_STATE, n, NTA_STATE_TCP);
	else if (nfct_attr_is_set(ct, ATTR_SCTP_STATE))
		__build_sctp(ct, n);
	else if (nfct_attr_is_set(ct, ATTR_DCCP_STATE))
		__build_dccp(ct, n);

	if (!CONFIG(commit_timeout) && nfct_attr_is_set(ct, ATTR_TIMEOUT))
		__build_u32(ct, ATTR_TIMEOUT, n, NTA_TIMEOUT);
	if (nfct_attr_is_set(ct, ATTR_MARK))
		__build_u32(ct, ATTR_MARK, n, NTA_MARK);

	/* setup the master conntrack */
	if (nfct_attr_grp_is_set(ct, ATTR_GRP_MASTER_IPV4)) {
		__build_group(ct, ATTR_GRP_MASTER_IPV4, n, NTA_MASTER_IPV4,
			      sizeof(struct nfct_attr_grp_ipv4));
		__build_u8(ct, ATTR_MASTER_L4PROTO, n, NTA_MASTER_L4PROTO);
		if (nfct_attr_grp_is_set(ct, ATTR_GRP_MASTER_PORT)) {
			__build_group(ct, ATTR_GRP_MASTER_PORT,
				      n, NTA_MASTER_PORT, 
				      sizeof(struct nfct_attr_grp_port));
		}
	} else if (nfct_attr_grp_is_set(ct, ATTR_GRP_MASTER_IPV6)) {
		__build_group(ct, ATTR_GRP_MASTER_IPV6, n, NTA_MASTER_IPV6,
			      sizeof(struct nfct_attr_grp_ipv6));
		__build_u8(ct, ATTR_MASTER_L4PROTO, n, NTA_MASTER_L4PROTO);
		if (nfct_attr_grp_is_set(ct, ATTR_GRP_MASTER_PORT)) {
			__build_group(ct, ATTR_GRP_MASTER_PORT,
				      n, NTA_MASTER_PORT,
				      sizeof(struct nfct_attr_grp_port));
		}
	}

	/*  NAT */
	if (nfct_getobjopt(ct, NFCT_GOPT_IS_SNAT))
		__build_u32(ct, ATTR_REPL_IPV4_DST, n, NTA_SNAT_IPV4);
	if (nfct_getobjopt(ct, NFCT_GOPT_IS_DNAT))
		__build_u32(ct, ATTR_REPL_IPV4_SRC, n, NTA_DNAT_IPV4);
	if (nfct_getobjopt(ct, NFCT_GOPT_IS_SPAT))
		__build_u16(ct, ATTR_REPL_PORT_DST, n, NTA_SPAT_PORT);
	if (nfct_getobjopt(ct, NFCT_GOPT_IS_DPAT))
		__build_u16(ct, ATTR_REPL_PORT_SRC, n, NTA_DPAT_PORT);

	/* NAT sequence adjustment */
	if (nfct_attr_is_set_array(ct, nat_type, 6))
		__build_natseqadj(ct, n);
}
