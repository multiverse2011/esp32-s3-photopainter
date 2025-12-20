# Tasks: E-ink Display Design Improvement

**Input**: Design documents from `/specs/002-improve-display-design/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Not included (not explicitly requested in specification)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **ESP-IDF Project**: `components/`, `main/` at repository root
- Based on plan.md structure

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, new component structure, and shared type definitions

- [ ] T001 Create task_service component directory structure: `components/task_service/`, `components/task_service/include/`
- [ ] T002 [P] Create train_service component directory structure: `components/train_service/`, `components/train_service/include/`
- [ ] T003 [P] Create shared display types header in `components/calendar_ui/include/display_types.h` with `display_data_t`, `ui_layout_t` from data-model.md
- [ ] T004 [P] Add Kconfig entries for new services in `components/task_service/Kconfig.projbuild` (TODOIST_API_TOKEN)
- [ ] T005 [P] Add Kconfig entries for train service in `components/train_service/Kconfig.projbuild` (TRAIN_LINE_NAME)
- [ ] T006 Register new components in project CMakeLists.txt

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**WARNING**: No user story work can begin until this phase is complete

- [ ] T007 Define `weather_forecast_t` (hourly) structure in `components/weather_service/include/weather_types.h` - change from daily[4] to hourly[5] per data-model.md
- [ ] T008 [P] Define `task_t` and `task_list_t` structures in `components/task_service/include/task_types.h` per data-model.md
- [ ] T009 [P] Define `train_status_t` and `train_status_code_t` structures in `components/train_service/include/train_types.h` per data-model.md
- [ ] T010 [P] Define UI layout constants (`UI_SIDEBAR_WIDTH`, `UI_CONTENT_WIDTH`, `UI_PADDING`, etc.) in `components/calendar_ui/include/calendar_ui.h`
- [ ] T011 Update `weather_data_t` to use `hourly[5]` instead of `daily[4]` in `components/weather_service/include/weather_types.h`

**Checkpoint**: Foundation ready - data structures defined, user story implementation can begin

---

## Phase 3: User Story 1 - 一目で今日の天気を把握できる (Priority: P1) MVP

**Goal**: Display 5 time-slot weather forecasts based on update time with 3-hour intervals

**Independent Test**: View device from 3 meters and verify each time slot's weather icon and temperature are clearly visible

### Implementation for User Story 1

- [ ] T012 [US1] Implement `weather_service_get_forecast_times()` function to calculate 5 timestamps (base_time + 0/3/6/9/12 hours) in `components/weather_service/weather_service.c`
- [ ] T013 [US1] Modify `weather_service_fetch_hourly()` to use 3-hour forecast API endpoint in `components/weather_service/weather_http.c`
- [ ] T014 [US1] Update weather JSON parser to extract 5 hourly forecasts from API response in `components/weather_service/weather_parser.c`
- [ ] T015 [US1] Implement 2-column base layout function `draw_layout_frame()` in `components/calendar_ui/calendar_ui.c` (200px sidebar + 600px content)
- [ ] T016 [US1] Implement weather section renderer `draw_weather_section()` with 5 columns (120px each) in `components/calendar_ui/calendar_ui.c`
- [ ] T017 [US1] Implement time label rendering (HH:00 format) for each weather column in `components/calendar_ui/calendar_ui.c`
- [ ] T018 [US1] Implement temperature display (large font) in weather columns in `components/calendar_ui/calendar_ui.c`
- [ ] T019 [US1] Implement humidity/wind display (small font) in weather columns in `components/calendar_ui/calendar_ui.c`
- [ ] T020 [US1] Update weather cache functions to handle `hourly[5]` data in `components/weather_service/weather_service.c`

**Checkpoint**: User Story 1 complete - 5 time-slot weather display functional

---

## Phase 4: User Story 2 - 7色E-Paperの特性を活かした表現 (Priority: P2)

**Goal**: Use 7-color E-Paper palette effectively for intuitive weather representation

**Independent Test**: Display each weather type and verify colors match intuition (sun=yellow/orange, rain=blue, etc.)

### Implementation for User Story 2

- [ ] T021 [US2] Define weather-to-color mapping constants (sunny=yellow/orange, rain=blue, cloudy=black outline) in `components/calendar_ui/include/calendar_ui.h`
- [ ] T022 [US2] Implement color-coded sun icon (yellow/orange fill) in `components/calendar_ui/weather_icons.c`
- [ ] T023 [P] [US2] Implement color-coded rain icon (blue rain drops) in `components/calendar_ui/weather_icons.c`
- [ ] T024 [P] [US2] Implement color-coded cloud icon (white fill, black outline) in `components/calendar_ui/weather_icons.c`
- [ ] T025 [P] [US2] Implement color-coded snow icon in `components/calendar_ui/weather_icons.c`
- [ ] T026 [US2] Implement temperature-based color selection function (cold=blue, warm=red/orange) in `components/calendar_ui/calendar_ui.c`
- [ ] T027 [US2] Apply temperature colors to temperature text rendering in `components/calendar_ui/calendar_ui.c`

**Checkpoint**: User Story 2 complete - color-coded weather icons and temperature functional

---

## Phase 5: User Story 3 - 情報密度と余白のバランス (Priority: P2)

**Goal**: Achieve proper spacing and information hierarchy for E-ink readability

**Independent Test**: Verify consistent padding between all elements and text readability at 1 meter

### Implementation for User Story 3

- [ ] T028 [US3] Implement segment-style large digit drawing function `draw_large_digit()` in `components/calendar_ui/calendar_ui.c`
- [ ] T029 [US3] Implement `draw_large_number()` for multi-digit date display in `components/calendar_ui/calendar_ui.c`
- [ ] T030 [US3] Implement sidebar date section with large day number and month name in `components/calendar_ui/calendar_ui.c`
- [ ] T031 [US3] Apply consistent padding (UI_PADDING) to all layout sections in `components/calendar_ui/calendar_ui.c`
- [ ] T032 [US3] Ensure weather column spacing is uniform (UI_WEATHER_COL_WIDTH) in `components/calendar_ui/calendar_ui.c`

**Checkpoint**: User Story 3 complete - balanced layout with large date and consistent spacing

---

## Phase 6: User Story 4 - タスク一覧の確認 (Priority: P2)

**Goal**: Display today's tasks from Todoist API in sidebar Tasks section

**Independent Test**: Verify tasks from Todoist appear in sidebar with bullet points

### Implementation for User Story 4

- [ ] T033 [US4] Implement `task_service_init()` in `components/task_service/task_service.c`
- [ ] T034 [US4] Implement HTTPS request to Todoist API with Bearer token in `components/task_service/task_service.c`
- [ ] T035 [US4] Implement JSON parser for Todoist response in `components/task_service/task_parser.c`
- [ ] T036 [US4] Implement `task_service_fetch()` to populate `task_list_t` (max 5 tasks) in `components/task_service/task_service.c`
- [ ] T037 [US4] Implement task NVS cache functions (`task_service_save_cache`, `task_service_load_cache`) in `components/task_service/task_service.c`
- [ ] T038 [US4] Implement `task_service_deinit()` in `components/task_service/task_service.c`
- [ ] T039 [US4] Implement Tasks section renderer `draw_tasks_section()` with bullet list in `components/calendar_ui/calendar_ui.c`
- [ ] T040 [US4] Handle empty task list case ("No tasks" display) in `components/calendar_ui/calendar_ui.c`

**Checkpoint**: User Story 4 complete - Todoist tasks displayed in sidebar

---

## Phase 7: User Story 5 - 電車遅延情報の確認 (Priority: P2)

**Goal**: Display train delay status from JR East page in Train section

**Independent Test**: Verify Train section shows "Not delayed" or delay info with appropriate colors

### Implementation for User Story 5

- [ ] T041 [US5] Implement `train_service_init()` in `components/train_service/train_service.c`
- [ ] T042 [US5] Implement HTTPS page fetch from JR East URL in `components/train_service/train_service.c`
- [ ] T043 [US5] Implement Shift_JIS to UTF-8 conversion function in `components/train_service/train_service.c`
- [ ] T044 [US5] Implement HTML parser to find target line and status in `components/train_service/train_parser.c`
- [ ] T045 [US5] Implement `train_service_fetch()` to populate `train_status_t` in `components/train_service/train_service.c`
- [ ] T046 [US5] Implement train status NVS cache functions in `components/train_service/train_service.c`
- [ ] T047 [US5] Implement `train_service_deinit()` in `components/train_service/train_service.c`
- [ ] T048 [US5] Implement Train section renderer `draw_train_section()` in `components/calendar_ui/calendar_ui.c`
- [ ] T049 [US5] Implement color-coded train status display (green=normal, red=delayed) in `components/calendar_ui/calendar_ui.c`

**Checkpoint**: User Story 5 complete - train delay info displayed with color coding

---

## Phase 8: User Story 6 - 更新日時の確認 (Priority: P3)

**Goal**: Display update timestamp in bottom-right corner

**Independent Test**: Verify "Updated YYYY/MM/DD HH:MM" appears in bottom-right after refresh

### Implementation for User Story 6

- [ ] T050 [US6] Implement `draw_updated_timestamp()` function in `components/calendar_ui/calendar_ui.c`
- [ ] T051 [US6] Format timestamp as "Updated YYYY/MM/DD HH:MM" in `components/calendar_ui/calendar_ui.c`
- [ ] T052 [US6] Position timestamp in bottom-right of content area in `components/calendar_ui/calendar_ui.c`

**Checkpoint**: User Story 6 complete - update timestamp displayed

---

## Phase 9: Integration & Main Application

**Purpose**: Wire all components together in main application

- [ ] T053 Integrate task_service initialization in `main/main.c`
- [ ] T054 [P] Integrate train_service initialization in `main/main.c`
- [ ] T055 Update data fetch sequence to include tasks and train status in `main/main.c`
- [ ] T056 Populate `display_data_t` aggregate structure with all service data in `main/main.c`
- [ ] T057 Update main display render call to use new `draw_full_layout()` in `main/main.c`
- [ ] T058 Implement graceful degradation when services fail (use cache) in `main/main.c`

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: Final improvements and edge case handling

- [ ] T059 Handle long task names with truncation in `components/calendar_ui/calendar_ui.c`
- [ ] T060 [P] Handle extreme temperatures (-10 to 100) layout in `components/calendar_ui/calendar_ui.c`
- [ ] T061 [P] Add cache staleness indicator for all data types in `components/calendar_ui/calendar_ui.c`
- [ ] T062 Verify display update time remains under 15 seconds
- [ ] T063 Run quickstart.md validation checklist

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-8)**: All depend on Foundational phase completion
  - US1 (Phase 3): Weather - can start immediately after Foundational
  - US2 (Phase 4): Colors - depends on US1 (needs weather icons base)
  - US3 (Phase 5): Layout - can run parallel with US1
  - US4 (Phase 6): Tasks - can run parallel with US1
  - US5 (Phase 7): Train - can run parallel with US1
  - US6 (Phase 8): Timestamp - can run parallel with US1
- **Integration (Phase 9)**: Depends on all user stories being complete
- **Polish (Phase 10)**: Depends on Integration completion

### User Story Dependencies

- **User Story 1 (P1)**: Foundational complete - No other story dependencies
- **User Story 2 (P2)**: Depends on US1 (weather icons exist) - can start after T019
- **User Story 3 (P2)**: Foundational complete - can run parallel with US1
- **User Story 4 (P2)**: Foundational complete - can run parallel with US1
- **User Story 5 (P2)**: Foundational complete - can run parallel with US1
- **User Story 6 (P3)**: Foundational complete - can run parallel with US1

### Parallel Opportunities

- Setup T001-T006: T002-T005 can run in parallel
- Foundational T007-T011: T008, T009, T010 can run in parallel
- US2 weather icons T023-T025 can run in parallel
- US4 and US5 are completely independent and can run in parallel
- Integration T053-T054 can run in parallel

---

## Parallel Example: Setup Phase

```bash
# Launch all parallel setup tasks together:
Task: "Create train_service component directory"
Task: "Create shared display types header"
Task: "Add Kconfig entries for task_service"
Task: "Add Kconfig entries for train_service"
```

## Parallel Example: User Story 4 & 5 (Independent)

```bash
# Developer A works on US4 (Tasks):
T033-T040: Task service implementation

# Developer B works on US5 (Train) simultaneously:
T041-T049: Train service implementation
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL - blocks all stories)
3. Complete Phase 3: User Story 1 (5 time-slot weather)
4. **STOP and VALIDATE**: Test weather display independently
5. Deploy/demo if ready - device shows improved weather layout

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test weather display → Deploy (MVP!)
3. Add User Story 2 → Test color-coded icons → Deploy
4. Add User Story 3 → Test large date layout → Deploy
5. Add User Story 4 → Test tasks display → Deploy
6. Add User Story 5 → Test train info → Deploy
7. Add User Story 6 → Test timestamp → Deploy
8. Complete Integration & Polish → Final deploy

### Single Developer Strategy

Priority order: US1 → US3 → US2 → US4 → US5 → US6

Rationale:
- US1 (weather) is core MVP
- US3 (layout/date) enables visual structure
- US2 (colors) enhances existing weather
- US4 & US5 (tasks/train) add new data sources
- US6 (timestamp) is low priority enhancement

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- ESP-IDF component structure: each component has `include/` for headers
- NVS namespaces: "weather", "tasks", "train" for caching
