/*
 * Micro Communicator for Projection uC
 * a High-Speed Serial communications server
 *
 * Input devices module
 *
 * Copyright (C) 2018 AngeloGioacchino Del Regno <kholk11@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define TOF_STABILIZATION_DEF_RUNS		4
#define TOF_STABILIZATION_WAIT_MS		10
#define TOF_STABILIZATION_HYST_MM		7
#define TOF_STABILIZATION_MATCH_NO		3

struct micro_communicator_vl53l0 {
	int range_mm;
	int distance;
	int range_status;
	int measure_mode;
};

enum {
	THREAD_TOF,
	THREAD_MAX
};

int ucomm_input_tof_read(struct micro_communicator_vl53l0 *stmvl_cur,
	uint16_t want_code);
int ucomm_tof_read_stabilized(
	struct micro_communicator_vl53l0 *stmvl_final,
	int runs, int nmatch, int sleep_ms, int hyst);
int ucomm_tof_thr_read_stabilized(
	struct micro_communicator_vl53l0 *stmvl_final,
	int runs, int nmatch, int sleep_ms, int hyst);
int ucomm_tof_enable(bool enable);
int ucomm_input_threadman(bool start, int threadno);
int ucomm_input_tof_init(void);

