cmake_minimum_required(VERSION 3.16)
project(photopainter_firmware_host_tests C)

enable_testing()

add_executable(ha_frame_policy_test
    http_policy_test.c
    ../../components/ha_frame_client/ha_frame_policy.c
)
target_include_directories(ha_frame_policy_test PRIVATE
    ../../components/ha_frame_client/include
)
target_compile_features(ha_frame_policy_test PRIVATE c_std_11)
if(MSVC)
    target_compile_options(ha_frame_policy_test PRIVATE /W4 /UNDEBUG)
else()
    target_compile_options(ha_frame_policy_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME ha_frame_policy COMMAND ha_frame_policy_test)

add_executable(wifi_lifecycle_test
    wifi_lifecycle_test.c
    ../../components/wifi_manager/wifi_manager.c
)
target_include_directories(wifi_lifecycle_test PRIVATE
    wifi_stubs
    ../../components/wifi_manager/include
)
target_compile_features(wifi_lifecycle_test PRIVATE c_std_11)
if(MSVC)
    target_compile_options(wifi_lifecycle_test PRIVATE /UNDEBUG)
else()
    target_compile_options(wifi_lifecycle_test PRIVATE -UNDEBUG)
endif()
add_test(NAME wifi_lifecycle COMMAND wifi_lifecycle_test)
