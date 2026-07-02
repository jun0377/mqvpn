// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 mp0rta and mqvpn contributors

/*
 * mqvpn_conn_settings.c — implementation. See mqvpn_conn_settings.h for
 * the contract; tests/test_conn_settings.c pins the asymmetric fields.
 */

#include "mqvpn_conn_settings.h"

#include "libmqvpn.h"
#include "mqvpn_scheduler.h"

#include <string.h>

#include <xquic/xquic.h>

/* Send-queue cap. Same value the previous inline blocks used; kept here
 * so adding new mqvpn-wide xquic-tuning knobs lives next to the builder. */
#define XQC_SNDQ_MAX_PKTS 16384

void
mqvpn_apply_scheduler(xqc_conn_settings_t *cs, mqvpn_scheduler_t sched) // 根据调度器枚举设置 xquic 连接级回调与 FEC 参数
{
    // 调度器类型
    switch (sched) {
    // 加权负载均衡 — 穿透到下一个 case
    case MQVPN_SCHED_WLB:
    // WLB + UDP 流固定：共用同一个 WLB 回调；流粘性由 flow_hash_pkt 提供 hint
    case MQVPN_SCHED_WLB_UDP_PIN: cs->scheduler_callback = xqc_wlb_scheduler_cb; break;

    // 主路径发包 + 备用路径发送 FEC 冗余修复包
    case MQVPN_SCHED_BACKUP_FEC:
#if defined(XQC_ENABLE_FEC) && defined(XQC_ENABLE_XOR)              // 编译期检查 FEC 可用性
        cs->scheduler_callback = xqc_backup_fec_scheduler_cb;       // 注册 xquic 内置的 backup+FEC 调度器回调
        cs->enable_encode_fec = 1;                                  // 开启 FEC 编码（发送端生成修复包）
        cs->enable_decode_fec = 1;                                  // 开启 FEC 解码（接收端利用修复包恢复丢包）
        cs->fec_params.fec_encoder_schemes_num = 1;                 // 仅注册一种 FEC 编码方案
        cs->fec_params.fec_encoder_schemes[0] = MQVPN_FEC_SCHEME;   // 使用 XOR 编码（低开销、低延迟）
        cs->fec_params.fec_decoder_schemes_num = 1;                 // 仅注册一种 FEC 解码方案
        cs->fec_params.fec_decoder_schemes[0] = MQVPN_FEC_SCHEME;   // 解码侧同样使用 XOR
        cs->fec_params.fec_code_rate = MQVPN_FEC_CODE_RATE;         // 修复包与源包比率（0.1→10% 冗余）
        cs->fec_params.fec_max_symbol_num_per_block = MQVPN_FEC_BLOCK_SIZE; // 每个 FEC block 最多 3 个源符号
        cs->fec_params.fec_mp_mode = XQC_FEC_MP_USE_STB;                    // 多路径模式：修复包走备用(standby)路径
        /* fec_callback intentionally left zero — xqc_set_valid_*_scheme_cb()
           fills it after FEC scheme negotiation completes. */
#else
        /* Built without FEC — silently degrade to MINRTT. main.c parser
           also rejects "backup_fec" at the CLI surface in this case, so this
           branch only protects against direct API callers. */
        cs->scheduler_callback = xqc_minrtt_scheduler_cb;                   // 无 FEC 支持时降级为最小 RTT 调度器
#endif
        break;
    case MQVPN_SCHED_MINRTT: // 最小 RTT：每包选延迟最低的路径
    default: cs->scheduler_callback = xqc_minrtt_scheduler_cb; break;       // 默认兜底策略：最小 RTT 调度器
    }
}

void
mqvpn_build_conn_settings(const mqvpn_conn_settings_input_t *in, xqc_conn_settings_t *out)
{
    memset(out, 0, sizeof(*out));

    /* --- shared hardcoded fields --- */
    out->max_datagram_frame_size = 65535;
    out->proto_version = XQC_VERSION_V1;
    out->pacing_on = 1;
    out->max_pkt_out_size = 1400;
    out->sndq_packets_used_max = XQC_SNDQ_MAX_PKTS;
    out->so_sndbuf = 8 * 1024 * 1024;
    out->idle_time_out = 120000;
    out->init_idle_time_out = 10000;

    /* --- congestion control --- */
    switch (in->cc) {
    case MQVPN_CC_BBR: out->cong_ctrl_callback = xqc_bbr_cb; break;
    case MQVPN_CC_CUBIC: out->cong_ctrl_callback = xqc_cubic_cb; break;
#ifdef XQC_ENABLE_UNLIMITED
    case MQVPN_CC_NONE: out->cong_ctrl_callback = xqc_unlimited_cc_cb; break;
#endif
    case MQVPN_CC_BBR2:
    default:
        out->cong_ctrl_callback = xqc_bbr2_cb;
        out->cc_params.cc_optimization_flags =
            XQC_BBR2_FLAG_RTTVAR_COMPENSATION | XQC_BBR2_FLAG_FAST_CONVERGENCE;
        break;
    }

    /* --- intentional client/server asymmetry (4) ---
     * Each branch documents why these fields differ between sides. Do not
     * collapse without revisiting tests/test_conn_settings.c
     * test_asymmetry_server_vs_client. */
    if (in->is_server) {
        /* server allows multipath unconditionally; client side gates on
         * cfg->multipath. draft-21 §3.2.1 ¶7 PATHS_BLOCKED auto-grant
         * (max_path_id_grant_max_value) is server-only because mqvpn's
         * client is the active path creator. */
        out->enable_multipath = 1;
        out->mp_ping_on = 1;
        out->max_path_id_grant_max_value = 128;
    } else {
        /* client carries the keep-alive role (ping_on=1) and gates MP on
         * cfg->multipath. */
        out->enable_multipath = in->enable_multipath ? 1 : 0;
        out->mp_ping_on = in->enable_multipath ? 1 : 0;
        out->ping_on = 1;
    }

    /* --- scheduler / FEC params --- */
    mqvpn_apply_scheduler(out, in->scheduler);

    /* --- init_max_path_id: 0 = keep xquic default (XQC_DEFAULT_INIT_MAX_PATH_ID=8) ---
     */
    if (in->init_max_path_id > 0) {
        out->init_max_path_id = in->init_max_path_id;
    }
}
