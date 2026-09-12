#include "ha_frame_policy.h"

#include <assert.h>
#include <stdint.h>

static void test_retry_after_bounds(void)
{
    assert(ha_frame_clamp_retry_after(0u) == 300u);
    assert(ha_frame_clamp_retry_after(1u) == 300u);
    assert(ha_frame_clamp_retry_after(299u) == 300u);
    assert(ha_frame_clamp_retry_after(300u) == 300u);
    assert(ha_frame_clamp_retry_after(7200u) == 7200u);
    assert(ha_frame_clamp_retry_after(7201u) == 7200u);
    assert(ha_frame_clamp_retry_after(UINT32_MAX) == 7200u);
}

static void test_authentication_blocks_wake(void)
{
    bool explicit_cooldown = false;
    assert(ha_frame_http_blocks_wake(401));
    assert(ha_frame_http_blocks_wake(403));
    assert(ha_frame_http_cooldown_seconds(401, 1u, 1800u, &explicit_cooldown) == 7200u);
    assert(explicit_cooldown);
    explicit_cooldown = false;
    assert(ha_frame_http_cooldown_seconds(403, 7200u, 1800u, &explicit_cooldown) == 7200u);
    assert(explicit_cooldown);
}

static void test_rate_limit_uses_bounded_retry_after(void)
{
    bool explicit_cooldown = false;
    assert(ha_frame_http_blocks_wake(429));
    assert(ha_frame_http_cooldown_seconds(429, 1u, 1800u, &explicit_cooldown) == 300u);
    assert(explicit_cooldown);
    explicit_cooldown = false;
    assert(ha_frame_http_cooldown_seconds(429, 3600u, 1800u, &explicit_cooldown) == 3600u);
    assert(explicit_cooldown);
    explicit_cooldown = false;
    assert(ha_frame_http_cooldown_seconds(429, UINT32_MAX, 1800u, &explicit_cooldown) == 7200u);
    assert(explicit_cooldown);
    explicit_cooldown = false;
    assert(ha_frame_http_cooldown_seconds(429, 0u, 1800u, &explicit_cooldown) == 1800u);
    assert(explicit_cooldown);
}

static void test_normal_status_keeps_manifest_schedule_eligible(void)
{
    bool explicit_cooldown = true;
    assert(!ha_frame_http_blocks_wake(200));
    assert(ha_frame_http_cooldown_seconds(200, 7200u, 1800u, &explicit_cooldown) == 1800u);
    assert(!explicit_cooldown);
    assert(!ha_frame_http_blocks_wake(503));
    assert(ha_frame_http_cooldown_seconds(503, 300u, 1800u, &explicit_cooldown) == 1800u);
    assert(!explicit_cooldown);
}

static void test_blocked_wake_keeps_report_durable(void)
{
    assert(ha_frame_should_persist_report(false, false, false));
    assert(ha_frame_should_persist_report(true, false, true));
    assert(!ha_frame_should_persist_report(true, false, false));
    assert(!ha_frame_should_persist_report(false, true, true));
}

int main(void)
{
    test_retry_after_bounds();
    test_authentication_blocks_wake();
    test_rate_limit_uses_bounded_retry_after();
    test_normal_status_keeps_manifest_schedule_eligible();
    test_blocked_wake_keeps_report_durable();
    return 0;
}
