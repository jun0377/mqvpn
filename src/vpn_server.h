// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 mp0rta and mqvpn contributors

#ifndef MQVPN_VPN_SERVER_H
#define MQVPN_VPN_SERVER_H

#include <stdint.h>
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <netinet/in.h>
#endif

#include "reorder.h" /* mqvpn_reorder_config_t (INI [Reorder] bridge) */

typedef struct mqvpn_server_cfg_s {
    const char *listen_addr;        /* 绑定地址（例如 "0.0.0.0"） bind address (e.g. "0.0.0.0") */
    int listen_port;                /* 绑定端口（例如 443） bind port (e.g. 443) */
    const char *subnet;             /* 客户端 IPv4 地址池 CIDR（例如 "10.0.0.0/24"） client IP pool CIDR (e.g. "10.0.0.0/24") */
    const char *subnet6;            /* 客户端 IPv6 地址池 CIDR（NULL = 禁用） IPv6 client pool CIDR (NULL = disabled) */
    const char *tun_name;           /* TUN 设备名称 TUN device name */
    const char *cert_file;          /* TLS 证书文件路径 TLS certificate path */
    const char *key_file;           /* TLS 私钥文件路径 TLS private key path */
    int log_level;                  /* 日志级别（mqvpn_log_level_t 枚举值） mqvpn_log_level_t */
    int scheduler;                  /* 调度器：0=minrtt, 1=wlb（默认）, 2=backup_fec, 3=wlb_udp_pin 0=minrtt, 1=wlb (default), 2=backup_fec, 3=wlb_udp_pin */
    const char *auth_key;           /* 客户端认证预共享密钥（NULL = 不启用认证） PSK for client authentication (NULL = no auth) */
    const char *user_names[64];
    const char *user_keys[64];
    int n_users;
    int max_clients;                /* 最大并发客户端数（默认 64） max concurrent clients (default 64) */
    const char *control_addr;       /* JSON 控制 API 绑定地址（默认 127.0.0.1） bind address for JSON control API (default 127.0.0.1) */
    int control_port;               /* JSON 控制 API TCP 端口（0 = 禁用）TCP port for JSON control API (0 = disabled) */
    uint64_t init_max_path_id;      /* draft-21 §4.6 传输参数上限，0=使用 xquic 默认值 8 draft-21 §4.6 TP cap, 0=use xquic default 8 */
    int tun_mtu;                    /* MTU：0=自动（启动时 1382），>0=手动指定（最小 1280） 0=auto (1382 at startup), >0=override (floor 1280) */
    int cc;                         /* 拥塞控制算法（mqvpn_cc_t 枚举值） mqvpn_cc_t: congestion control algorithm */
    mqvpn_reorder_config_t reorder;  /* INI [Reorder]/[ReorderRule] 配置（默认关闭） NI [Reorder]/[ReorderRule] (mode OFF by default) */
} mqvpn_server_cfg_t;

#endif /* MQVPN_VPN_SERVER_H */
