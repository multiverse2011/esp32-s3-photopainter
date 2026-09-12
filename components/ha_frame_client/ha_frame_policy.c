#include "ha_frame_policy.h"

#include <stddef.h>

uint32_t ha_frame_clamp_retry_after(uint32_t retry_after_seconds)
{
    if (retry_after_seconds < HA_FRAME_MIN_RETRY_AFTER_SECONDS) {
        return HA_FRAME_MIN_RETRY_AFTER_SECONDS;
    }
    if (retry_after_seconds > HA_FRAME_MAX_RETRY_AFTER_SECONDS) {
        return HA_FRAME_MAX_RETRY_AFTER_SECONDS;
    }
    return retry_after_seconds;
}

uint32_t ha_frame_http_cooldown_seconds(int status_code,
                                        uint32_t retry_after_seconds,
                                        uint32_t fallback_seconds,
                                        bool *explicit_cooldown)
{
    bool explicit = false;
    uint32_t delay = fallback_seconds;
    if (status_code == 401 || status_code == 403) {
        delay = HA_FRAME_MAX_RETRY_AFTER_SECONDS;
        explicit = true;
    } else if (status_code == 429) {
        delay = retry_after_seconds != 0u
            ? ha_frame_clamp_retry_after(retry_after_seconds)
            : fallback_seconds;
        explicit = true;
    }
    if (explicit_cooldown != NULL) {
        *explicit_cooldown = explicit;
    }
    return delay;
}

bool ha_frame_http_blocks_wake(int status_code)
{
    return status_code == 401 || status_code == 403 || status_code == 429;
}

bool ha_frame_should_persist_report(bool network_ready, bool pending_report,
                                    bool blocked_this_wake)
{
    return !pending_report && (!network_ready || blocked_this_wake);
}
