/**
  vpn.c

  Copyright (C) 2015 clowwindy

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "shadowvpn.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#ifndef TARGET_WIN32
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

int nat_init(nat_ctx_t *ctx, shadowvpn_args_t *args) {
  bzero(ctx, sizeof(nat_ctx_t));
  for (int i = 0; i < args->user_tokens_len; i++) {
    client_info_t *client = malloc(sizeof(client_info_t));
    bzero(client, sizeof(client_info_t));

    memcpy(client->user_token, args->user_tokens[i], SHADOWVPN_USERTOKEN_LEN);

    // test only, assign 10.9.0.x
    client->output_tun_ip[0] = 10;
    client->output_tun_ip[1] = 9;
    client->output_tun_ip[0] = 0;
    client->output_tun_ip[0] = 2 + i;

    // add to hash: ctx->token_to_clients[user_token] = client
    HASH_ADD(hh, ctx->token_to_clients, user_token, SHADOWVPN_USERTOKEN_LEN, client);
  }
  return 0;
}

/*
   RFC791
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |Version|  IHL  |Type of Service|          Total Length         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         Identification        |Flags|      Fragment Offset    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Time to Live |    Protocol   |         Header Checksum       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                       Source Address                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Destination Address                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Options                    |    Padding    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

int nat_fix_upstream(nat_ctx_t *ctx, unsigned char *buf, size_t buflen,
                     const struct sockaddr *addr, socklen_t addrlen) {
  if (buflen < SHADOWVPN_USERTOKEN_LEN + 20) {
    errf("nat: packet too short");
    return -1;
  }
  unsigned char *iphdr = buf + SHADOWVPN_USERTOKEN_LEN;
  if ((iphdr[0] & 0xf0) != 0x40) {
    // check header, currently IPv4 only
    // bypass IPv6
    return 0;
  }

  print_hex_memory(buf, SHADOWVPN_USERTOKEN_LEN);
  client_info_t *client = NULL;
  HASH_FIND(hh, ctx->token_to_clients, buf, SHADOWVPN_USERTOKEN_LEN, client);
  if (client == NULL) {
    errf("nat: client not found for given user token");
    return -1;
  }
  print_hex_memory(iphdr, buflen);

  // save source address
  client->source_addr.addrlen =  addrlen;
  memcpy(&client->source_addr.addr, addr, addrlen);
  // old checksum
  uint16_t old_checksum = (iphdr[10] << 8) + iphdr[11];
  // save tun input ip to client
  memcpy(client->input_tun_ip, iphdr + 12, 4);
  // overwrite IP
  memcpy(iphdr + 12, client->output_tun_ip, 4);
  // TODO update checksum
  return 0;
}

int nat_fix_downstream(nat_ctx_t *ctx, unsigned char *buf, size_t buflen,
                       struct sockaddr *addr, socklen_t *addrlen) {
  return 0;
}

