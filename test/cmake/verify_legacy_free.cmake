# Proves the first published 1.0.0 stays legacy-free (legacy removal program, LEGACY-011).
#
# The removal inventory closed on 2026-09-14: every surface the owner decided to remove is gone,
# every ratchet baseline is empty, and the deprecation ledger carries no entry. This gate pins
# that end state so it cannot silently regress: a new pending inventory row, a repopulated
# baseline, or a new deprecated declaration each needs an explicit owner decision, not a drive-by
# edit.
#
# Required variables:
#   WSE_SOURCE_ROOT              Repository root.
#   WSE_LEGACY_REMOVAL_INVENTORY Path to LegacyRemovalInventory.tsv.
#   WSE_DEPRECATION_LEDGER       Path to DeprecationLedger.tsv.

foreach(wse_required_variable
		WSE_SOURCE_ROOT WSE_LEGACY_REMOVAL_INVENTORY WSE_DEPRECATION_LEDGER)
	if(NOT DEFINED ${wse_required_variable})
		message(FATAL_ERROR "${wse_required_variable} is required.")
	endif()
endforeach()

# 1. Every inventory row is resolved: removed (or an explicit owner-retained decision).
file(STRINGS "${WSE_LEGACY_REMOVAL_INVENTORY}" wse_inventory_lines)
set(wse_inventory_rows 0)
foreach(wse_inventory_line IN LISTS wse_inventory_lines)
	if(wse_inventory_line STREQUAL "" OR wse_inventory_line MATCHES "^[ \t]*#")
		continue()
	endif()
	math(EXPR wse_inventory_rows "${wse_inventory_rows} + 1")
	string(REPLACE "|" ";" wse_inventory_fields "${wse_inventory_line}")
	list(GET wse_inventory_fields 0 wse_inventory_id)
	list(GET wse_inventory_fields 7 wse_inventory_status)
	if(NOT wse_inventory_status MATCHES "^(removed|retained)$")
		message(FATAL_ERROR
			"The legacy-free gate requires every inventory row to be resolved; "
			"'${wse_inventory_id}' is '${wse_inventory_status}'. A new legacy surface needs an "
			"explicit owner decision recorded in the inventory before it can ship.")
	endif()
endforeach()
if(wse_inventory_rows EQUAL 0)
	message(FATAL_ERROR "The legacy removal inventory is empty; the program history must remain.")
endif()

# 2. Every source-policy ratchet baseline is empty: the public API carries no reviewed debt.
foreach(wse_baseline_name
		public_legacy_error_baseline.txt
		public_raw_void_pointer_baseline.txt
		public_third_party_type_baseline.txt
		public_thread_state_baseline.txt)
	set(wse_baseline_path "${WSE_SOURCE_ROOT}/test/baseline/${wse_baseline_name}")
	if(NOT EXISTS "${wse_baseline_path}")
		message(FATAL_ERROR "Baseline file is missing: ${wse_baseline_name}")
	endif()
	file(STRINGS "${wse_baseline_path}" wse_baseline_lines)
	foreach(wse_baseline_line IN LISTS wse_baseline_lines)
		if(wse_baseline_line STREQUAL "" OR wse_baseline_line MATCHES "^[ \t]*#")
			continue()
		endif()
		message(FATAL_ERROR
			"The legacy-free gate requires an empty ${wse_baseline_name}; found: "
			"'${wse_baseline_line}'. Reviewed public-API debt may not return.")
	endforeach()
endforeach()

# 3. The deprecation ledger is empty: the first published API ships nothing deprecated.
file(STRINGS "${WSE_DEPRECATION_LEDGER}" wse_ledger_lines)
foreach(wse_ledger_line IN LISTS wse_ledger_lines)
	if(wse_ledger_line STREQUAL "" OR wse_ledger_line MATCHES "^[ \t]*#")
		continue()
	endif()
	message(FATAL_ERROR
		"The legacy-free gate requires an empty deprecation ledger; found: "
		"'${wse_ledger_line}'. The first published 1.0.0 ships no deprecated declaration.")
endforeach()

message(STATUS "Legacy-free release gate passed: ${wse_inventory_rows} inventory rows resolved, "
	"all ratchet baselines empty, deprecation ledger empty.")
