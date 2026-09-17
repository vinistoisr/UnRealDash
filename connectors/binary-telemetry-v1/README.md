# Definition pack conversion

Source: the owner's schema XML (path withheld).
Conversion date (UTC): 2026-09-17.
Pack: `vehicle.binary-telemetry-v1` version `0.1.0`, API 1.

Status role: 0xC82, the board status identifier repeated by the relay as a heartbeat.
Its fields publish on every status record with refreshed receive timestamps and unknown age evidence.
Status traffic keeps transport connected, never counts toward acquisition health, and never refreshes telemetry-role signals.
Its fields are held. All other frames have telemetry role.
The acquisition override table is empty pending owner confirmation; other fields default to live.
Sentinel lists are empty pending owner decisions about enum-carrying fields.
The schema permits empty signals arrays; receive frames with no retained fields are emitted.
Signed fields use unsigned width types plus the explicit signed property required by the landed schema.

Emitted identifiers: 0xC80, 0xC81, 0xC82, 0xC83, 0xC84, 0xC85, 0xC86, 0xC87, 0xC88, 0xC89, 0xC8A, 0xC8B, 0xC8C, 0xC8D, 0xC8E, 0xC8F, 0xC91, 0xC93, 0xC94.
Identifiers: 19; fields: 65.

- timeout 0xC80: 3000
- acquisition rpm: live
- acquisition map_kpa: live
- unit-unrepresented engine_load: %
- acquisition engine_load: live
- acquisition iat: live
- timeout 0xC81: 3000
- acquisition boost: live
- acquisition boost_target: live
- acquisition boost_peak: live
- unit-unrepresented n75_duty: %
- acquisition n75_duty: live
- timeout 0xC82: 5000
- acquisition baro: held
- acquisition fault_codes: held
- acquisition reconnects: held
- acquisition sample_rate: held
- timeout 0xC83: 5000
- acquisition coolant_c: live
- acquisition oil_c: live
- unit-unrepresented battery_v: V
- acquisition battery_v: live
- acquisition speed_kmh: live
- timeout 0xC84: 5000
- acquisition ambient_temp: live
- acquisition maf: live
- unit-unrepresented throttle: %
- acquisition throttle: live
- timeout 0xC85: 5000
- unit-unrepresented trim_short: %
- acquisition trim_short: live
- unit-unrepresented trim_long: %
- acquisition trim_long: live
- acquisition charge_air_temp: live
- timeout 0xC88: 5000
- acquisition fuel_rail_pressure: live
- acquisition lambda_commanded: live
- unit-unrepresented pedal_position: %
- acquisition pedal_position: live
- timeout 0xC89: 5000
- unit-unrepresented knock_cyl_1: deg
- acquisition knock_cyl_1: live
- unit-unrepresented knock_cyl_2: deg
- acquisition knock_cyl_2: live
- unit-unrepresented knock_cyl_3: deg
- acquisition knock_cyl_3: live
- unit-unrepresented knock_cyl_4: deg
- acquisition knock_cyl_4: live
- timeout 0xC8A: 10000
- unit-unrepresented odometer: km
- acquisition odometer: live
- acquisition catalyst_temp: live
- acquisition run_time: live
- unit-unrepresented distance_since_clear: km
- acquisition distance_since_clear: live
- timeout 0xC86: 10000
- acquisition fault_1: live
- enum-awaiting-sentinel-decision fault_1
- acquisition fault_2: live
- enum-awaiting-sentinel-decision fault_2
- acquisition fault_3: live
- enum-awaiting-sentinel-decision fault_3
- acquisition fault_4: live
- enum-awaiting-sentinel-decision fault_4
- timeout 0xC87: 10000
- acquisition check_engine: live
- acquisition overtemp_warning: live
- acquisition knock_warning: live
- acquisition boost_deviation_warning: live
- acquisition gear: live
- acquisition steering_counts: live
- unit-unrepresented ride_height_fl: V
- acquisition ride_height_fl: live
- unit-unrepresented ride_height_fr: V
- acquisition ride_height_fr: live
- acquisition damper_1: live
- acquisition damper_2: live
- acquisition damper_3: live
- acquisition damper_4: live
- unit-unrepresented ride_height_rear: V
- acquisition ride_height_rear: live
- acquisition aux_visits: live
- acquisition aux_fails: live
- unit-unrepresented gear_age_s: s
- acquisition gear_age_s: live
- unit-unrepresented dcc_age_s: s
- acquisition dcc_age_s: live
- enum-awaiting-sentinel-decision dcc_age_s
- acquisition dcc_status: live
- enum-awaiting-sentinel-decision dcc_status
- unit-unrepresented fuel_pump_duty: %
- acquisition fuel_pump_duty: live
- acquisition fuel_temp: live
- acquisition rail_spec_bar_abs: live
- acquisition rail_actual_bar_abs: live
- acquisition turn_left: live
- acquisition turn_right: live
- unit-unrepresented steering_deg: deg
- acquisition steering_deg: live
- unit-unrepresented canbox_age: s
- acquisition canbox_age: live
- skipped 0xC92: display-only frame
- timeout 0xC93: 3000
- unit-unrepresented timing_deg: deg
- acquisition timing_deg: live
- acquisition inj_ms: live
- timeout 0xC94: 5000
- empty-frame 0xC94: emitted with empty signals array; schema permits it
- skipped 0xC90: write-direction
