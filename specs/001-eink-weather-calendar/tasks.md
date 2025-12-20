# Tasks: ESP32-S3 E-ink Weather Calendar

**Input**: Design documents from `/specs/001-eink-weather-calendar/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Manual hardware testing approach (ESP-IDF Unity framework available but not required for MVP)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- Include exact file paths in descriptions

## Path Conventions (ESP-IDF Project)

- **main/**: Application entry point and configuration
- **components/**: ESP-IDF components (each with include/, CMakeLists.txt)
- **sdkconfig.defaults**: Build configuration
- **partitions.csv**: Partition table

---

## Phase 1: Setup (Project Infrastructure) ✅ COMPLETE

**Purpose**: ESP-IDF project initialization and component structure

- [X] T001 Update root CMakeLists.txt to remove MINIMAL_BUILD and add components in CMakeLists.txt
- [X] T002 [P] Create sdkconfig.defaults with PSRAM, WiFi, HTTPS settings
- [X] T003 [P] Create partitions.csv with NVS and application partitions
- [X] T004 [P] Create main/Kconfig.projbuild with WiFi and API configuration options
- [X] T005 Create components/wifi_manager/ directory structure with CMakeLists.txt
- [X] T006 [P] Create components/epd_driver/ directory structure with CMakeLists.txt
- [X] T007 [P] Create components/gfx_library/ directory structure with CMakeLists.txt
- [X] T008 [P] Create components/weather_service/ directory structure with CMakeLists.txt
- [X] T009 [P] Create components/calendar_ui/ directory structure with CMakeLists.txt

**Checkpoint**: Project compiles with empty component stubs

---

## Phase 2: Foundational (Core Components) ✅ COMPLETE

**Purpose**: Low-level drivers and libraries that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### WiFi Manager Component

- [X] T010 Create components/wifi_manager/include/wifi_manager.h with API declarations
- [X] T011 Implement WiFi initialization in components/wifi_manager/wifi_manager.c
- [X] T012 Implement WiFi connection with timeout in components/wifi_manager/wifi_manager.c
- [X] T013 Implement SNTP time synchronization in components/wifi_manager/wifi_manager.c
- [X] T014 Implement WiFi disconnect and cleanup in components/wifi_manager/wifi_manager.c

### EPD Driver Component

- [X] T015 [P] Create components/epd_driver/include/epd_driver.h with API declarations and color enum
- [X] T016 Implement SPI initialization in components/epd_driver/epd_spi.c
- [X] T017 Implement GPIO configuration (DC, CS, RST, BUSY) in components/epd_driver/epd_driver.c
- [X] T018 Implement display initialization sequence in components/epd_driver/epd_driver.c
- [X] T019 Implement PSRAM framebuffer allocation in components/epd_driver/epd_driver.c
- [X] T020 Implement buffer-to-display transfer with DMA in components/epd_driver/epd_spi.c
- [X] T021 Implement display refresh with BUSY wait in components/epd_driver/epd_driver.c
- [X] T022 Implement display sleep mode in components/epd_driver/epd_driver.c
- [X] T023 Implement hardware reset function in components/epd_driver/epd_driver.c

### Graphics Library Component

- [X] T024 [P] Create components/gfx_library/include/gfx_paint.h with drawing API declarations
- [X] T025 Implement gfx_init and set_pixel in components/gfx_library/gfx_paint.c
- [X] T026 Implement line drawing (Bresenham) in components/gfx_library/gfx_primitives.c
- [X] T027 Implement rectangle drawing (outline/filled) in components/gfx_library/gfx_primitives.c
- [X] T028 Implement circle drawing (Midpoint) in components/gfx_library/gfx_primitives.c
- [X] T029 [P] Create 16px bitmap font data in components/gfx_library/fonts/font_16.c
- [X] T030 [P] Create 24px bitmap font data in components/gfx_library/fonts/font_24.c
- [X] T031 [P] Create 32px bitmap font data in components/gfx_library/fonts/font_32.c
- [X] T032 Implement character and string drawing in components/gfx_library/gfx_paint.c
- [X] T033 Implement text alignment utilities in components/gfx_library/gfx_paint.c

**Checkpoint**: Foundation ready - display shows solid colors and basic text

---

## Phase 3: User Story 1 - 天気情報の確認 (Priority: P1) 🎯 MVP ✅ COMPLETE

**Goal**: Display 4-day weather forecast with icons, temperatures, humidity, and wind

**Independent Test**: Power on device → WiFi connects → Weather data appears on display with 4 days of forecasts

### Weather Service Component (US1)

- [X] T034 [P] [US1] Create components/weather_service/include/weather_types.h with forecast structures
- [X] T035 [P] [US1] Create components/weather_service/include/weather_service.h with API declarations
- [X] T036 [US1] Implement HTTPS client for OpenWeatherMap in components/weather_service/weather_http.c
- [X] T037 [US1] Implement JSON parsing with cJSON in components/weather_service/weather_parser.c
- [X] T038 [US1] Implement noon data extraction (4 days) in components/weather_service/weather_parser.c
- [X] T039 [US1] Implement weather_service_fetch() combining HTTP and parsing in components/weather_service/weather_http.c

### Calendar UI Component (US1)

- [X] T040 [P] [US1] Create components/calendar_ui/include/calendar_ui.h with UI API declarations
- [X] T041 [US1] Implement sun icon drawing in components/calendar_ui/weather_icons.c
- [X] T042 [US1] Implement cloud icon drawing in components/calendar_ui/weather_icons.c
- [X] T043 [US1] Implement rain icon drawing in components/calendar_ui/weather_icons.c
- [X] T044 [US1] Implement additional icons (snow, thunder, fog) in components/calendar_ui/weather_icons.c
- [X] T045 [US1] Implement icon code to drawing function mapping in components/calendar_ui/weather_icons.c
- [X] T046 [US1] Implement forecast column layout (200px each) in components/calendar_ui/calendar_ui.c
- [X] T047 [US1] Implement temperature display with colors in components/calendar_ui/calendar_ui.c
- [X] T048 [US1] Implement humidity and wind display in components/calendar_ui/calendar_ui.c
- [X] T049 [US1] Implement calendar_ui_draw_forecast() for single day in components/calendar_ui/calendar_ui.c
- [X] T050 [US1] Implement full screen layout with 4 forecast columns in components/calendar_ui/calendar_ui.c

### Main Application Integration (US1)

- [X] T051 [US1] Implement basic app_main() flow in main/main.c: init → wifi → fetch → draw → refresh

**Checkpoint**: Device displays 4-day weather forecast - MVP complete!

---

## Phase 4: User Story 2 - 現在日時の確認 (Priority: P1) ✅ COMPLETE

**Goal**: Display current date and time in header area

**Independent Test**: After SNTP sync, header shows accurate current date and time

### Graphics Extensions (US2)

- [X] T052 [US2] Implement time formatting (HH:MM) in components/gfx_library/gfx_paint.c
- [X] T053 [US2] Implement date formatting (MM/DD, weekday) in components/gfx_library/gfx_paint.c

### Calendar UI Header (US2)

- [X] T054 [US2] Implement header layout (0-80px top) in components/calendar_ui/calendar_ui.c
- [X] T055 [US2] Implement calendar_ui_draw_header() with date/time in components/calendar_ui/calendar_ui.c
- [X] T056 [US2] Integrate header into full calendar_ui_draw() in components/calendar_ui/calendar_ui.c

**Checkpoint**: Device shows date/time in header + 4-day forecast

---

## Phase 5: User Story 3 - 省電力での長期動作 (Priority: P2) ✅ COMPLETE

**Goal**: Implement deep sleep for battery operation

**Independent Test**: Device updates display, then enters deep sleep for configured interval

### Power Management (US3)

- [X] T057 [US3] Implement RTC memory cache structure (RTC_DATA_ATTR) in main/main.c
- [X] T058 [US3] Implement deep sleep timer configuration in main/main.c
- [X] T059 [US3] Implement time-of-day based interval selection (30min/2hr) in main/main.c
- [X] T060 [US3] Implement boot reason detection in main/main.c
- [X] T061 [US3] Implement RTC cache save before sleep in main/main.c
- [X] T062 [US3] Implement RTC cache load on wake in main/main.c

### API Call Throttling (US3)

- [X] T063 [US3] Implement last_api_call tracking in RTC memory in main/main.c
- [X] T064 [US3] Implement 3-hour API call throttle logic in main/main.c
- [X] T065 [US3] Skip API call when cache is fresh (< 3 hours) in main/main.c

### Resource Cleanup (US3)

- [X] T066 [US3] Implement proper WiFi disconnect before sleep in main/main.c
- [X] T067 [US3] Implement display sleep command before deep sleep in main/main.c
- [X] T068 [US3] Implement SPI deinit before deep sleep in main/main.c

**Checkpoint**: Device operates in power-efficient wake-sleep cycle

---

## Phase 6: User Story 4 - オフライン時のフォールバック (Priority: P3) ✅ COMPLETE

**Goal**: Cache weather data and fallback when offline

**Independent Test**: Disable WiFi → device still shows cached weather data with staleness indicator

### NVS Cache (US4)

- [X] T069 [US4] Implement NVS namespace for weather cache in components/weather_service/weather_http.c
- [X] T070 [US4] Implement weather_service_save_cache() in components/weather_service/weather_http.c
- [X] T071 [US4] Implement weather_service_load_cache() in components/weather_service/weather_http.c
- [X] T072 [US4] Implement weather_service_cache_valid() with 24h expiry in components/weather_service/weather_http.c

### Error Handling (US4)

- [X] T073 [US4] Implement WiFi connection retry (3 attempts) in components/wifi_manager/wifi_manager.c
- [X] T074 [US4] Implement HTTP request retry with exponential backoff in components/weather_service/weather_http.c
- [X] T075 [US4] Implement cache fallback on fetch failure in main/main.c

### Error Display (US4)

- [X] T076 [US4] Implement calendar_ui_draw_error() screen in components/calendar_ui/calendar_ui.c
- [X] T077 [US4] Implement cache staleness indicator in header in components/calendar_ui/calendar_ui.c
- [X] T078 [US4] Implement last update timestamp display in components/calendar_ui/calendar_ui.c

### State Machine (US4)

- [X] T079 [US4] Implement app_state_t enum and state machine in main/main.c
- [X] T080 [US4] Implement STATE_ERROR handling with error screen in main/main.c

**Checkpoint**: Device handles network failures gracefully with cached data

---

## Phase 7: Polish & Robustness ✅ COMPLETE

**Purpose**: Improvements that affect multiple user stories

### Display Robustness

- [X] T081 Implement BUSY timeout detection (20s) in components/epd_driver/epd_driver.c
- [X] T082 Implement auto-reset on BUSY timeout in components/epd_driver/epd_driver.c

### Memory Safety

- [X] T083 Add NULL pointer checks throughout all components
- [X] T084 Implement heap usage monitoring with ESP_LOG in main/main.c
- [X] T085 Add buffer bounds checking in graphics library in components/gfx_library/gfx_paint.c

### Logging

- [X] T086 Add comprehensive ESP_LOG statements in all components
- [X] T087 Implement boot time and operation timing logs in main/main.c

### Documentation

- [X] T088 Update README.md with setup instructions
- [X] T089 Validate quickstart.md scenarios work as documented

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1: Setup ──────────────────────────────────────┐
    │                                                 │
    v                                                 │
Phase 2: Foundational (WiFi, EPD, GFX) ◄─────────────┘
    │
    │  ⚠️ BLOCKS ALL USER STORIES
    │
    ├──────────────────┬──────────────────┬──────────────────┐
    v                  v                  v                  v
Phase 3: US1       Phase 4: US2       Phase 5: US3       Phase 6: US4
(Weather Display)  (Date/Time)        (Power Mgmt)       (Offline)
    │                  │                  │                  │
    └──────────────────┴──────────────────┴──────────────────┘
                                │
                                v
                     Phase 7: Polish
```

### User Story Dependencies

| Story | Depends On | Notes |
|-------|------------|-------|
| US1 (P1) | Phase 2 only | Core MVP - can be developed first |
| US2 (P1) | Phase 2 only | Independent of US1, parallel-capable |
| US3 (P2) | Phase 2 + US1 | Requires working main loop from US1 |
| US4 (P3) | Phase 2 + US1 | Extends weather service from US1 |

### Within Each Phase

**Foundational (Phase 2)**:
```
WiFi Manager: T010 → T011 → T012 → T13 → T014
EPD Driver:   T015 → T016 → T017 → T018 → T019 → T020 → T021 → T022 → T023
GFX Library:  T024 → T025 → T026/T027/T028 (parallel) → T029/T030/T031 (parallel) → T032 → T033
```

**User Story 1**:
```
Weather Types: T034, T035 (parallel)
Weather HTTP:  T036 → T037 → T038 → T039
Calendar UI:   T040 → T041/T042/T043/T044 (parallel) → T045 → T046 → T047 → T048 → T049 → T050
Main:          T051 (depends on all above)
```

---

## Parallel Opportunities

### Phase 2 Parallelization

```bash
# These can run in parallel (different components):
Task: "Create components/wifi_manager/include/wifi_manager.h"
Task: "Create components/epd_driver/include/epd_driver.h"
Task: "Create components/gfx_library/include/gfx_paint.h"

# Font files can be created in parallel:
Task: "Create 16px bitmap font data in components/gfx_library/fonts/font_16.c"
Task: "Create 24px bitmap font data in components/gfx_library/fonts/font_24.c"
Task: "Create 32px bitmap font data in components/gfx_library/fonts/font_32.c"
```

### Phase 3 (US1) Parallelization

```bash
# Weather icons can be implemented in parallel:
Task: "Implement sun icon drawing in components/calendar_ui/weather_icons.c"
Task: "Implement cloud icon drawing in components/calendar_ui/weather_icons.c"
Task: "Implement rain icon drawing in components/calendar_ui/weather_icons.c"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. ✅ Complete Phase 1: Setup (T001-T009)
2. ✅ Complete Phase 2: Foundational (T010-T033)
3. ✅ Complete Phase 3: User Story 1 (T034-T051)
4. **STOP and VALIDATE**: Test weather display independently
5. Flash device, verify 4-day forecast displays correctly

### Incremental Delivery

| Increment | Tasks | Deliverable |
|-----------|-------|-------------|
| 1. MVP | Phase 1-3 (T001-T051) | Weather forecast display |
| 2. Date/Time | Phase 4 (T052-T056) | Header with current time |
| 3. Power | Phase 5 (T057-T068) | Battery-efficient operation |
| 4. Offline | Phase 6 (T069-T080) | Graceful network failure handling |
| 5. Polish | Phase 7 (T081-T089) | Production-ready |

### Single Developer Strategy (Recommended)

1. Phase 1 → Phase 2 → Phase 3 (MVP)
2. Validate MVP thoroughly on hardware
3. Phase 4 (quick win, ~5 tasks)
4. Phase 5 (power management)
5. Phase 6 (error handling)
6. Phase 7 (polish)

---

## Task Summary

| Phase | Task Range | Count | Description |
|-------|-----------|-------|-------------|
| 1 | T001-T009 | 9 | Setup |
| 2 | T010-T033 | 24 | Foundational |
| 3 | T034-T051 | 18 | US1: Weather Display |
| 4 | T052-T056 | 5 | US2: Date/Time |
| 5 | T057-T068 | 12 | US3: Power Management |
| 6 | T069-T080 | 12 | US4: Offline Fallback |
| 7 | T081-T089 | 9 | Polish |
| **Total** | T001-T089 | **89** | |

### Tasks per User Story

| Story | Tasks | MVP? |
|-------|-------|------|
| US1 (Weather) | 18 | ✅ |
| US2 (Date/Time) | 5 | ✅ |
| US3 (Power) | 12 | |
| US4 (Offline) | 12 | |
| Shared/Polish | 42 | |

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story
- Validate on hardware after each phase
- ESP_LOG extensively for debugging
- Start SPI at 1MHz, increase to 10MHz after stability
- Test with mock weather data before API integration
- Commit after each logical group of tasks
