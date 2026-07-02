// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 mp0rta and mqvpn contributors

#ifndef MQVPN_TUN_H
#define MQVPN_TUN_H

#include <stdint.h>
#include <stddef.h>
#include <net/if.h>
#include <netinet/in.h>

/* mqvpn_tun_write() returns this when the kernel buffer is full (EAGAIN). */
#define MQVPN_TUN_EAGAIN (-2)

typedef struct {
    int fd;                         /* TUN 设备文件描述符 */
    char name[IFNAMSIZ];            /* 接口名称（如 "tun0"） */
    struct in_addr addr;            /* 本端 IPv4 地址 */
    struct in_addr peer_addr;       /* 对端 IPv4 地址 */
    struct in6_addr addr6;          /* 本端 IPv6 地址 */
    int has_v6;                     /* 是否启用了 IPv6 */
    int mtu;                        /* 接口 MTU */
} mqvpn_tun_t;

/* Create a TUN device. dev_name may be NULL for auto-naming. */
int mqvpn_tun_create(mqvpn_tun_t *tun, const char *dev_name);

/* Assign point-to-point addresses and prefix length. */
int mqvpn_tun_set_addr(mqvpn_tun_t *tun, const char *addr, const char *peer_addr,
                       int prefix_len);

/* Set MTU on the TUN device. */
int mqvpn_tun_set_mtu(mqvpn_tun_t *tun, int mtu);

/* Bring the TUN interface up. */
int mqvpn_tun_up(mqvpn_tun_t *tun);

/* Read a single IP packet from the TUN device (non-blocking safe). */
int mqvpn_tun_read(mqvpn_tun_t *tun, uint8_t *buf, size_t buf_len);

/* Write a single IP packet to the TUN device. */
int mqvpn_tun_write(mqvpn_tun_t *tun, const uint8_t *buf, size_t len);

/* Assign an IPv6 address to the TUN device. */
int mqvpn_tun_set_addr6(mqvpn_tun_t *tun, const char *addr6, int prefix_len);

/* Close the TUN device. */
void mqvpn_tun_destroy(mqvpn_tun_t *tun);

#endif /* MQVPN_TUN_H */
