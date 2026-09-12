/**
 * Small, platform-independent reliability decisions shared by the HA
 * runtime and host tests.
 */
#ifndef PHOTOPAINTER_HA_FRAME_POLICY_H
#define PHOTOPAINTER_HA_FRAME_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HA_FRAME_MIN_RETRY_AFTER_SECONDS 300u
#define HA_FRAME_MAX_RETRY_AFTER_SECONDS 7200u
/** Clamp a server-provided Retry-After to the bounded sleep policy. */
uint32_t ha_frame_clamp_retry_after(uint32_t retry_after_seconds);

/**
 * Return the delay selected for an HTTP status.  `explicit_cooldown` is set
 * for authentication and rate-limit responses, which must override a
 * manifest-provided wake time.
 */
uint32_t ha_frame_http_cooldown_seconds(int status_code,
                                        uint32_t retry_after_seconds,
                                        uint32_t fallback_seconds,
                                        bool *explicit_cooldown);

/** Authentication/rate-limit responses prohibit another request this wake. */
bool ha_frame_http_blocks_wake(int status_code);

/** Keep a report durable when transport is unavailable or this wake is blocked. */
bool ha_frame_should_persist_report(bool network_ready, bool pending_report,
                                    bool blocked_this_wake);

#ifdef __cplusplus
}
#endif

#endif
