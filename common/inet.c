/* inet.c

   Subroutines to manipulate internet addresses in a safely portable
   way... */

/*
 * Copyright (c) 2004-2005 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: inet.c,v 1.11.60.3 2007/01/31 20:44:55 dhankins Exp $ Copyright (c) 2004-2005 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

/* Return just the network number of an internet address... */

struct iaddr subnet_number (addr, mask)
	struct iaddr addr;
	struct iaddr mask;
{
	int i;
	struct iaddr rv;

	if (addr.len > sizeof(addr.iabuf))
		log_fatal("subnet_number():%s:%d: Invalid addr length.", MDL);
	if (addr.len != mask.len)
		log_fatal("subnet_number():%s:%d: Addr/mask length mismatch.",
			  MDL);

	rv.len = 0;

	/* Both addresses must have the same length... */
	if (addr.len != mask.len)
		return rv;

	rv.len = addr.len;
	for (i = 0; i < rv.len; i++)
		rv.iabuf [i] = addr.iabuf [i] & mask.iabuf [i];
	return rv;
}

/* Combine a network number and a integer to produce an internet address.
   This won't work for subnets with more than 32 bits of host address, but
   maybe this isn't a problem. */

struct iaddr ip_addr (subnet, mask, host_address)
	struct iaddr subnet;
	struct iaddr mask;
	u_int32_t host_address;
{
	int i, j, k;
	u_int32_t swaddr;
	struct iaddr rv;
	unsigned char habuf [sizeof swaddr];

	if (subnet.len > sizeof(subnet.iabuf))
		log_fatal("ip_addr():%s:%d: Invalid addr length.", MDL);
	if (subnet.len != mask.len)
		log_fatal("ip_addr():%s:%d: Addr/mask length mismatch.",
			  MDL);

	swaddr = htonl (host_address);
	memcpy (habuf, &swaddr, sizeof swaddr);

	/* Combine the subnet address and the host address.   If
	   the host address is bigger than can fit in the subnet,
	   return a zero-length iaddr structure. */
	rv = subnet;
	j = rv.len - sizeof habuf;
	for (i = sizeof habuf - 1; i >= 0; i--) {
		if (mask.iabuf [i + j]) {
			if (habuf [i] > (mask.iabuf [i + j] ^ 0xFF)) {
				rv.len = 0;
				return rv;
			}
			for (k = i - 1; k >= 0; k--) {
				if (habuf [k]) {
					rv.len = 0;
					return rv;
				}
			}
			rv.iabuf [i + j] |= habuf [i];
			break;
		} else
			rv.iabuf [i + j] = habuf [i];
	}
		
	return rv;
}

/* Given a subnet number and netmask, return the address on that subnet
   for which the host portion of the address is all ones (the standard
   broadcast address). */

struct iaddr broadcast_addr (subnet, mask)
	struct iaddr subnet;
	struct iaddr mask;
{
	int i, j, k;
	struct iaddr rv;

	if (subnet.len > sizeof(subnet.iabuf))
		log_fatal("broadcast_addr():%s:%d: Invalid addr length.", MDL);
	if (subnet.len != mask.len)
		log_fatal("broadcast_addr():%s:%d: Addr/mask length mismatch.",
			  MDL);

	if (subnet.len != mask.len) {
		rv.len = 0;
		return rv;
	}

	for (i = 0; i < subnet.len; i++) {
		rv.iabuf [i] = subnet.iabuf [i] | (~mask.iabuf [i] & 255);
	}
	rv.len = subnet.len;

	return rv;
}

u_int32_t host_addr (addr, mask)
	struct iaddr addr;
	struct iaddr mask;
{
	int i;
	u_int32_t swaddr;
	struct iaddr rv;

	if (addr.len > sizeof(addr.iabuf))
		log_fatal("host_addr():%s:%d: Invalid addr length.", MDL);
	if (addr.len != mask.len)
		log_fatal("host_addr():%s:%d: Addr/mask length mismatch.",
			  MDL);

	rv.len = 0;

	/* Mask out the network bits... */
	rv.len = addr.len;
	for (i = 0; i < rv.len; i++)
		rv.iabuf [i] = addr.iabuf [i] & ~mask.iabuf [i];

	/* Copy out up to 32 bits... */
	memcpy (&swaddr, &rv.iabuf [rv.len - sizeof swaddr], sizeof swaddr);

	/* Swap it and return it. */
	return ntohl (swaddr);
}

int addr_eq (addr1, addr2)
	struct iaddr addr1, addr2;
{
	if (addr1.len > sizeof(addr1.iabuf))
		log_fatal("addr_eq():%s:%d: Invalid addr length.", MDL);

	if (addr1.len != addr2.len)
		return 0;
	return memcmp (addr1.iabuf, addr2.iabuf, addr1.len) == 0;
}

/* addr_match 
 *
 * compares an IP address against a network/mask combination
 * by ANDing the IP with the mask and seeing whether the result
 * matches the masked network value.
 */
int
addr_match(addr, match)
	struct iaddr *addr;
	struct iaddrmatch *match;
{
        int i;

	if (addr->len != match->addr.len)
		return 0;
	
	i = 0;
	for (i = 0 ; i < addr->len ; i++) {
		if ((addr->iabuf[i] & match->mask.iabuf[i]) !=
							match->addr.iabuf[i])
			return 0;
	}
	return 1;
}

/* piaddr() turns an iaddr structure into a printable address. */
/* XXX: should use a const pointer rather than passing the structure */
const char *
piaddr(const struct iaddr addr) {
	static char
		pbuf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
			 /* "255.255.255.255" */

	/* INSIST((addr.len == 0) || (addr.len == 4) || (addr.len == 16)); */

	if (addr.len == 0) {
		return "<null address>";
	}
	if (addr.len == 4) {
		return inet_ntop(AF_INET, addr.iabuf, pbuf, sizeof(pbuf));
	} 
	if (addr.len == 16) {
		return inet_ntop(AF_INET6, addr.iabuf, pbuf, sizeof(pbuf));
	}

	log_fatal("piaddr():%s:%d: Invalid address length %d.", MDL,
		  addr.len);
	/* quell compiler warnings */
	return NULL;
}

/* piaddrmask takes an iaddr structure mask, determines the bitlength of
 * the mask, and then returns the printable CIDR notation of the two.
 */
char *
piaddrmask(struct iaddr *addr, struct iaddr *mask) {
	int mw;
	unsigned int oct, bit;

	if ((addr->len != 4) && (addr->len != 16))
		log_fatal("piaddrmask():%s:%d: Address length %d invalid",
			  MDL, addr->len);
	if (addr->len != mask->len)
		log_fatal("piaddrmask():%s:%d: Address and mask size mismatch",
			  MDL);

	/* Determine netmask width in bits. */
	for (mw = (mask->len * 8) ; mw > 0 ; ) {
		oct = (mw - 1) / 8;
		bit = 0x80 >> ((mw - 1) % 8);
		if (!mask->iabuf[oct])
			mw -= 8;
		else if (mask->iabuf[oct] & bit)
			break;
		else
			mw--;
	}

	if (mw < 0)
		log_fatal("Impossible condition at %s:%d.", MDL);

	return piaddrcidr(addr, mw);
}

/* Format an address and mask-length into printable CIDR notation. */
char *
piaddrcidr(const struct iaddr *addr, unsigned int bits) {
	static char
	    ret[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255/128")];
		    /* "255.255.255.255/32" */

	/* INSIST(addr != NULL); */
	/* INSIST((addr->len == 4) || (addr->len == 16)); */
	/* INSIST(bits <= (addr->len * 8)); */

	if (bits > (addr->len * 8))
		return NULL;

	sprintf(ret, "%s/%d", piaddr(*addr), bits);

	return ret;
}

