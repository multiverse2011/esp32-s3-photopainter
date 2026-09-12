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
target_compile_options(ha_frame_policy_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME ha_frame_policy COMMAND ha_frame_policy_test)
