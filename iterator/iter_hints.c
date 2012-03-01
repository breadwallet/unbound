/*
 * iterator/iter_hints.c - iterative resolver module stub and root hints.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains functions to assist the iterator module.
 * Keep track of stub and root hints, and read those from config.
 */
#include "config.h"
#include <ldns/dname.h>
#include <ldns/rr.h>
#include "iterator/iter_hints.h"
#include "iterator/iter_delegpt.h"
#include "util/log.h"
#include "util/config_file.h"
#include "util/net_help.h"
#include "util/data/dname.h"

struct iter_hints* 
hints_create(void)
{
	struct iter_hints* hints = (struct iter_hints*)calloc(1,
		sizeof(struct iter_hints));
	if(!hints)
		return NULL;
	return hints;
}

static void hints_stub_free(struct iter_hints_stub* s)
{
	if(!s) return;
	delegpt_free_mlc(s->dp);
	free(s);
}

static void delhintnode(rbnode_t* n, void* ATTR_UNUSED(arg))
{
	struct iter_hints_stub* node = (struct iter_hints_stub*)n;
	hints_stub_free(node);
}

static void hints_del_tree(struct iter_hints* hints)
{
	traverse_postorder(&hints->tree, &delhintnode, NULL);
}

void 
hints_delete(struct iter_hints* hints)
{
	if(!hints) 
		return;
	hints_del_tree(hints);
	free(hints);
}

/** add hint to delegation hints */
static int
ah(struct delegpt* dp, const char* sv, const char* ip)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	ldns_rdf* rdf = ldns_dname_new_frm_str(sv);
	if(!rdf) {
		log_err("could not parse %s", sv);
		return 0;
	}
	if(!delegpt_add_ns_mlc(dp, ldns_rdf_data(rdf), 0) ||
	   !extstrtoaddr(ip, &addr, &addrlen) ||
	   !delegpt_add_target_mlc(dp, ldns_rdf_data(rdf), ldns_rdf_size(rdf),
		&addr, addrlen, 0, 0)) {
		ldns_rdf_deep_free(rdf);
		return 0;
	}
	ldns_rdf_deep_free(rdf);
	return 1;
}

/** obtain compiletime provided root hints */
static struct delegpt* 
compile_time_root_prime(int do_ip4, int do_ip6)
{
	/* from:
	 ;       This file is made available by InterNIC
	 ;       under anonymous FTP as
	 ;           file                /domain/named.cache
	 ;           on server           FTP.INTERNIC.NET
	 ;       -OR-                    RS.INTERNIC.NET
	 ;
	 ;       related version of root zone:   2010061700
	 */
	struct delegpt* dp = delegpt_create_mlc((uint8_t*)"\000");
	if(!dp)
		return NULL;
	dp->has_parent_side_NS = 1;
      if(do_ip4) {
	if(!ah(dp, "A.ROOT-SERVERS.NET.", "198.41.0.4"))	return 0;
	if(!ah(dp, "B.ROOT-SERVERS.NET.", "192.228.79.201")) return 0;
	if(!ah(dp, "C.ROOT-SERVERS.NET.", "192.33.4.12"))	return 0;
	if(!ah(dp, "D.ROOT-SERVERS.NET.", "128.8.10.90"))	return 0;
	if(!ah(dp, "E.ROOT-SERVERS.NET.", "192.203.230.10")) return 0;
	if(!ah(dp, "F.ROOT-SERVERS.NET.", "192.5.5.241"))	return 0;
	if(!ah(dp, "G.ROOT-SERVERS.NET.", "192.112.36.4"))	return 0;
	if(!ah(dp, "H.ROOT-SERVERS.NET.", "128.63.2.53"))	return 0;
	if(!ah(dp, "I.ROOT-SERVERS.NET.", "192.36.148.17"))	return 0;
	if(!ah(dp, "J.ROOT-SERVERS.NET.", "192.58.128.30"))	return 0;
	if(!ah(dp, "K.ROOT-SERVERS.NET.", "193.0.14.129"))	return 0;
	if(!ah(dp, "L.ROOT-SERVERS.NET.", "199.7.83.42"))	return 0;
	if(!ah(dp, "M.ROOT-SERVERS.NET.", "202.12.27.33"))	return 0;
      }
      if(do_ip6) {
	if(!ah(dp, "A.ROOT-SERVERS.NET.", "2001:503:ba3e::2:30")) return 0;
	if(!ah(dp, "D.ROOT-SERVERS.NET.", "2001:500:2d::d")) return 0;
	if(!ah(dp, "F.ROOT-SERVERS.NET.", "2001:500:2f::f")) return 0;
	if(!ah(dp, "H.ROOT-SERVERS.NET.", "2001:500:1::803f:235")) return 0;
	if(!ah(dp, "I.ROOT-SERVERS.NET.", "2001:7fe::53")) return 0;
	if(!ah(dp, "J.ROOT-SERVERS.NET.", "2001:503:c27::2:30")) return 0;
	if(!ah(dp, "K.ROOT-SERVERS.NET.", "2001:7fd::1")) return 0;
	if(!ah(dp, "L.ROOT-SERVERS.NET.", "2001:500:3::42")) return 0;
	if(!ah(dp, "M.ROOT-SERVERS.NET.", "2001:dc3::35")) return 0;
      }
	return dp;
}

/** insert new hint info into hint structure */
static int
hints_insert(struct iter_hints* hints, uint16_t c, struct delegpt* dp,
	int noprime)
{
	struct iter_hints_stub* node = (struct iter_hints_stub*)malloc(
		sizeof(struct iter_hints_stub));
	if(!node) {
		delegpt_free_mlc(dp);
		return 0;
	}
	node->dp = dp;
	node->noprime = (uint8_t)noprime;
	if(!name_tree_insert(&hints->tree, &node->node, dp->name, dp->namelen,
		dp->namelabs, c)) {
		log_err("second hints ignored.");
		delegpt_free_mlc(dp);
		free(node);
	}
	return 1;
}

/** set stub name */
static struct delegpt* 
read_stubs_name(struct config_stub* s)
{
	struct delegpt* dp;
	ldns_rdf* rdf;
	if(!s->name) {
		log_err("stub zone without a name");
		return NULL;
	}
	rdf = ldns_dname_new_frm_str(s->name);
	if(!rdf) {
		log_err("cannot parse stub zone name %s", s->name);
		return NULL;
	}
	if(!(dp=delegpt_create_mlc(ldns_rdf_data(rdf)))) {
		ldns_rdf_deep_free(rdf);
		log_err("out of memory");
		return NULL;
	}
	ldns_rdf_deep_free(rdf);
	return dp;
}

/** set stub host names */
static int 
read_stubs_host(struct config_stub* s, struct delegpt* dp)
{
	struct config_strlist* p;
	ldns_rdf* rdf;
	for(p = s->hosts; p; p = p->next) {
		log_assert(p->str);
		rdf = ldns_dname_new_frm_str(p->str);
		if(!rdf) {
			log_err("cannot parse stub %s nameserver name: '%s'", 
				s->name, p->str);
			return 0;
		}
		if(!delegpt_add_ns_mlc(dp, ldns_rdf_data(rdf), 0)) {
			ldns_rdf_deep_free(rdf);
			log_err("out of memory");
			return 0;
		}
		ldns_rdf_deep_free(rdf);
	}
	return 1;
}

/** set stub server addresses */
static int 
read_stubs_addr(struct config_stub* s, struct delegpt* dp)
{
	struct config_strlist* p;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	for(p = s->addrs; p; p = p->next) {
		log_assert(p->str);
		if(!extstrtoaddr(p->str, &addr, &addrlen)) {
			log_err("cannot parse stub %s ip address: '%s'", 
				s->name, p->str);
			return 0;
		}
		if(!delegpt_add_addr_mlc(dp, &addr, addrlen, 0, 0)) {
			log_err("out of memory");
			return 0;
		}
	}
	return 1;
}

/** read stubs config */
static int 
read_stubs(struct iter_hints* hints, struct config_file* cfg)
{
	struct config_stub* s;
	struct delegpt* dp;
	for(s = cfg->stubs; s; s = s->next) {
		if(!(dp=read_stubs_name(s)) ||
			!read_stubs_host(s, dp) ||
			!read_stubs_addr(s, dp))
			return 0;
		/* the flag is turned off for 'stub-first' so that the
		 * last resort will ask for parent-side NS record and thus
		 * fallback to the internet name servers on a failure */
		dp->has_parent_side_NS = !s->isfirst;
		if(!hints_insert(hints, LDNS_RR_CLASS_IN, dp, !s->isprime))
			return 0;
		delegpt_log(VERB_QUERY, dp);
	}
	return 1;
}

/** read root hints from file */
static int 
read_root_hints(struct iter_hints* hints, char* fname)
{
	int lineno = 0;
	uint32_t default_ttl = 0;
	ldns_rdf* origin = NULL;
	ldns_rdf* prev_rr = NULL;
	struct delegpt* dp;
	ldns_rr* rr = NULL;
	ldns_status status;
	uint16_t c = LDNS_RR_CLASS_IN;
	FILE* f = fopen(fname, "r");
	if(!f) {
		log_err("could not read root hints %s: %s",
			fname, strerror(errno));
		return 0;
	}
	dp = delegpt_create_mlc(NULL);
	if(!dp) {
		log_err("out of memory reading root hints");
		fclose(f);
		return 0;
	}
	verbose(VERB_QUERY, "Reading root hints from %s", fname);
	dp->has_parent_side_NS = 1;
	while(!feof(f)) {
		status = ldns_rr_new_frm_fp_l(&rr, f, 
			&default_ttl, &origin, &prev_rr, &lineno);
		if(status == LDNS_STATUS_SYNTAX_EMPTY ||
			status == LDNS_STATUS_SYNTAX_TTL ||
			status == LDNS_STATUS_SYNTAX_ORIGIN)
			continue;
		if(status != LDNS_STATUS_OK) {
			log_err("reading root hints %s %d: %s", fname,
				lineno, ldns_get_errorstr_by_id(status));
			goto stop_read;
		}
		if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_NS) {
			if(!delegpt_add_ns_mlc(dp,
				ldns_rdf_data(ldns_rr_rdf(rr, 0)), 0)) {
				log_err("out of memory reading root hints");
				goto stop_read;
			}
			c = ldns_rr_get_class(rr);
			if(!dp->name) {
				if(!delegpt_set_name_mlc(dp,
					ldns_rdf_data(ldns_rr_owner(rr)))){
					log_err("out of memory.");
					goto stop_read;
				}
			}
		} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_A) {
			struct sockaddr_in sa;
			socklen_t len = (socklen_t)sizeof(sa);
			memset(&sa, 0, len);
			sa.sin_family = AF_INET;
			sa.sin_port = (in_port_t)htons(UNBOUND_DNS_PORT);
			memmove(&sa.sin_addr, 
				ldns_rdf_data(ldns_rr_rdf(rr, 0)), INET_SIZE);
			if(!delegpt_add_target_mlc(dp,
					ldns_rdf_data(ldns_rr_owner(rr)),
					ldns_rdf_size(ldns_rr_owner(rr)),
					(struct sockaddr_storage*)&sa, len, 
					0, 0)) {
				log_err("out of memory reading root hints");
				goto stop_read;
			}
		} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_AAAA) {
			struct sockaddr_in6 sa;
			socklen_t len = (socklen_t)sizeof(sa);
			memset(&sa, 0, len);
			sa.sin6_family = AF_INET6;
			sa.sin6_port = (in_port_t)htons(UNBOUND_DNS_PORT);
			memmove(&sa.sin6_addr, 
				ldns_rdf_data(ldns_rr_rdf(rr, 0)), INET6_SIZE);
			if(!delegpt_add_target_mlc(dp,
					ldns_rdf_data(ldns_rr_owner(rr)),
					ldns_rdf_size(ldns_rr_owner(rr)),
					(struct sockaddr_storage*)&sa, len,
					0, 0)) {
				log_err("out of memory reading root hints");
				goto stop_read;
			}
		} else {
			log_warn("root hints %s:%d skipping type %d",
				fname, lineno, ldns_rr_get_type(rr));
		}

		ldns_rr_free(rr);
	}

	if (origin)
		ldns_rdf_deep_free(origin);
	if (prev_rr)
		ldns_rdf_deep_free(prev_rr);
	fclose(f);
	if(!dp->name) {
		log_warn("root hints %s: no NS content", fname);
		delegpt_free_mlc(dp);
		return 1;
	}
	if(!hints_insert(hints, c, dp, 0)) {
		return 0;
	}
	delegpt_log(VERB_QUERY, dp);
	return 1;

stop_read:
	if (origin)
		ldns_rdf_deep_free(origin);
	if (prev_rr)
		ldns_rdf_deep_free(prev_rr);
	delegpt_free_mlc(dp);
	fclose(f);
	return 0;
}

/** read root hints list */
static int 
read_root_hints_list(struct iter_hints* hints, struct config_file* cfg)
{
	struct config_strlist* p;
	for(p = cfg->root_hints; p; p = p->next) {
		log_assert(p->str);
		if(p->str && p->str[0]) {
			char* f = p->str;
			if(cfg->chrootdir && cfg->chrootdir[0] &&
				strncmp(p->str, cfg->chrootdir, 
				strlen(cfg->chrootdir)) == 0)
				f += strlen(cfg->chrootdir);
			if(!read_root_hints(hints, f))
				return 0;
		}
	}
	return 1;
}

int 
hints_apply_cfg(struct iter_hints* hints, struct config_file* cfg)
{
	hints_del_tree(hints);
	name_tree_init(&hints->tree);
	
	/* read root hints */
	if(!read_root_hints_list(hints, cfg))
		return 0;

	/* read stub hints */
	if(!read_stubs(hints, cfg))
		return 0;

	/* use fallback compiletime root hints */
	if(!hints_lookup_root(hints, LDNS_RR_CLASS_IN)) {
		struct delegpt* dp = compile_time_root_prime(cfg->do_ip4,
			cfg->do_ip6);
		verbose(VERB_ALGO, "no config, using builtin root hints.");
		if(!dp) 
			return 0;
		if(!hints_insert(hints, LDNS_RR_CLASS_IN, dp, 0))
			return 0;
	}

	name_tree_init_parents(&hints->tree);
	return 1;
}

struct delegpt* 
hints_lookup_root(struct iter_hints* hints, uint16_t qclass)
{
	uint8_t rootlab = 0;
	struct iter_hints_stub *stub;
	stub = (struct iter_hints_stub*)name_tree_find(&hints->tree,
		&rootlab, 1, 1, qclass);
	if(!stub)
		return NULL;
	return stub->dp;
}

struct iter_hints_stub* 
hints_lookup_stub(struct iter_hints* hints, uint8_t* qname, 
	uint16_t qclass, struct delegpt* cache_dp)
{
	size_t len;
	int labs;
	struct iter_hints_stub *r;

	/* first lookup the stub */
	labs = dname_count_size_labels(qname, &len);
	r = (struct iter_hints_stub*)name_tree_lookup(&hints->tree, qname,
		len, labs, qclass);
	if(!r) return NULL;

	/* If there is no cache (root prime situation) */
	if(cache_dp == NULL) {
		if(r->dp->namelabs != 1)
			return r; /* no cache dp, use any non-root stub */
		return NULL;
	}

	/*
	 * If the stub is same as the delegation we got
	 * And has noprime set, we need to 'prime' to use this stub instead.
	 */
	if(r->noprime && query_dname_compare(cache_dp->name, r->dp->name)==0)
		return r; /* use this stub instead of cached dp */
	
	/* 
	 * If our cached delegation point is above the hint, we need to prime.
	 */
	if(dname_strict_subdomain(r->dp->name, r->dp->namelabs,
		cache_dp->name, cache_dp->namelabs))
		return r; /* need to prime this stub */
	return NULL;
}

int hints_next_root(struct iter_hints* hints, uint16_t* qclass)
{
	return name_tree_next_root(&hints->tree, qclass);
}

size_t 
hints_get_mem(struct iter_hints* hints)
{
	size_t s;
	struct iter_hints_stub* p;
	if(!hints) return 0;
	s = sizeof(*hints);
	RBTREE_FOR(p, struct iter_hints_stub*, &hints->tree) {
		s += sizeof(*p) + delegpt_get_mem(p->dp);
	}
	return s;
}

int 
hints_add_stub(struct iter_hints* hints, uint16_t c, struct delegpt* dp,
	int noprime)
{
	struct iter_hints_stub *z;
	if((z=(struct iter_hints_stub*)name_tree_find(&hints->tree,
		dp->name, dp->namelen, dp->namelabs, c)) != NULL) {
		(void)rbtree_delete(&hints->tree, &z->node);
		hints_stub_free(z);
	}
	if(!hints_insert(hints, c, dp, noprime))
		return 0;
	name_tree_init_parents(&hints->tree);
	return 1;
}

void 
hints_delete_stub(struct iter_hints* hints, uint16_t c, uint8_t* nm)
{
	struct iter_hints_stub *z;
	size_t len;
	int labs = dname_count_size_labels(nm, &len);
	if(!(z=(struct iter_hints_stub*)name_tree_find(&hints->tree,
		nm, len, labs, c)))
		return; /* nothing to do */
	(void)rbtree_delete(&hints->tree, &z->node);
	hints_stub_free(z);
	name_tree_init_parents(&hints->tree);
}

