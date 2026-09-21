# @file verify_legacy_removal_inventory.cmake
# @brief Verifies the pre-publication legacy removal inventory and its ledger linkage.
#
# The inventory is the reviewed list of compatibility surfaces the owner decided to remove before
# the first published 1.0.0. This script proves each row is well formed, that its migration
# references resolve to real anchors, and that every DeprecationLedger id is represented, so a
# deprecated surface cannot fall out of the removal program unnoticed.

if(NOT DEFINED WSE_SOURCE_ROOT OR NOT IS_DIRECTORY "${WSE_SOURCE_ROOT}")
	message(FATAL_ERROR "WSE_SOURCE_ROOT must name the WSE source directory.")
endif()
if(NOT DEFINED WSE_LEGACY_REMOVAL_INVENTORY OR NOT EXISTS "${WSE_LEGACY_REMOVAL_INVENTORY}")
	message(FATAL_ERROR "WSE_LEGACY_REMOVAL_INVENTORY must name the reviewed inventory file.")
endif()
if(NOT DEFINED WSE_DEPRECATION_LEDGER OR NOT EXISTS "${WSE_DEPRECATION_LEDGER}")
	message(FATAL_ERROR "WSE_DEPRECATION_LEDGER must name the reviewed TSV ledger.")
endif()

file(STRINGS "${WSE_LEGACY_REMOVAL_INVENTORY}" wse_inventory_lines ENCODING UTF-8)
set(wse_inventory_ids)

foreach(wse_inventory_line IN LISTS wse_inventory_lines)
	if(wse_inventory_line STREQUAL "" OR wse_inventory_line MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" wse_inventory_fields "${wse_inventory_line}")
	list(LENGTH wse_inventory_fields wse_inventory_field_count)
	if(NOT wse_inventory_field_count EQUAL 12)
		message(FATAL_ERROR "Invalid legacy removal inventory row: ${wse_inventory_line}")
	endif()
	list(GET wse_inventory_fields 0 wse_id)
	list(GET wse_inventory_fields 1 wse_component)
	list(GET wse_inventory_fields 2 wse_surface)
	list(GET wse_inventory_fields 3 wse_kind)
	list(GET wse_inventory_fields 4 wse_replacement)
	list(GET wse_inventory_fields 5 wse_managed_consumers)
	list(GET wse_inventory_fields 6 wse_abi_impact)
	list(GET wse_inventory_fields 7 wse_removal_status)
	list(GET wse_inventory_fields 8 wse_removal_phase)
	list(GET wse_inventory_fields 9 wse_ja_migration)
	list(GET wse_inventory_fields 10 wse_en_migration)
	list(GET wse_inventory_fields 11 wse_owner_decision)

	list(FIND wse_inventory_ids "${wse_id}" wse_existing_id_index)
	if(NOT wse_existing_id_index EQUAL -1)
		message(FATAL_ERROR "Duplicate legacy removal inventory id: ${wse_id}")
	endif()
	list(APPEND wse_inventory_ids "${wse_id}")
	if(NOT wse_kind MATCHES
			"^(deprecated|compatibility-only|unsafe-public-state|third-party-coupling|legacy-error|legacy-naming)$")
		message(FATAL_ERROR "Unknown legacy removal kind for ${wse_id}: ${wse_kind}")
	endif()
	if(NOT wse_removal_status MATCHES "^(pending|in-progress|removed|retained)$")
		message(FATAL_ERROR "Unknown legacy removal status for ${wse_id}: ${wse_removal_status}")
	endif()
	if(NOT wse_removal_phase MATCHES "^[1-8]$")
		message(FATAL_ERROR "Legacy removal phase must be 1-8 for ${wse_id}: ${wse_removal_phase}")
	endif()
	if(NOT wse_abi_impact MATCHES "^(source|source\\+abi|source\\+package|package-config)$")
		message(FATAL_ERROR "Unknown ABI impact for ${wse_id}: ${wse_abi_impact}")
	endif()
	foreach(wse_required_field IN ITEMS
			"${wse_component}" "${wse_surface}" "${wse_replacement}"
			"${wse_managed_consumers}" "${wse_owner_decision}")
		if(wse_required_field STREQUAL "")
			message(FATAL_ERROR "Incomplete legacy removal inventory row for ${wse_id}")
		endif()
	endforeach()

	foreach(wse_migration_reference IN ITEMS "${wse_ja_migration}" "${wse_en_migration}")
		string(FIND "${wse_migration_reference}" "#" wse_anchor_separator)
		if(wse_anchor_separator LESS 1)
			message(FATAL_ERROR "Migration reference lacks a stable anchor: ${wse_migration_reference}")
		endif()
		string(SUBSTRING "${wse_migration_reference}" 0 ${wse_anchor_separator} wse_migration_path)
		math(EXPR wse_anchor_start "${wse_anchor_separator} + 1")
		string(SUBSTRING "${wse_migration_reference}" ${wse_anchor_start} -1 wse_migration_anchor)
		if(NOT EXISTS "${WSE_SOURCE_ROOT}/${wse_migration_path}")
			message(FATAL_ERROR "Migration document is missing: ${wse_migration_path}")
		endif()
		file(READ "${WSE_SOURCE_ROOT}/${wse_migration_path}" wse_migration_text)
		string(FIND "${wse_migration_text}" "<a id=\"${wse_migration_anchor}\"></a>" wse_anchor_index)
		if(wse_anchor_index EQUAL -1)
			message(FATAL_ERROR
				"Migration anchor '${wse_migration_anchor}' is missing from ${wse_migration_path}")
		endif()
	endforeach()
endforeach()

list(LENGTH wse_inventory_ids wse_inventory_total)
if(wse_inventory_total EQUAL 0)
	message(FATAL_ERROR "The legacy removal inventory contains no rows.")
endif()

# Every ledger id must be tracked by the removal program.
file(STRINGS "${WSE_DEPRECATION_LEDGER}" wse_ledger_lines ENCODING UTF-8)
foreach(wse_ledger_line IN LISTS wse_ledger_lines)
	if(wse_ledger_line STREQUAL "" OR wse_ledger_line MATCHES "^[ \t]*#")
		continue()
	endif()
	string(REPLACE "|" ";" wse_ledger_fields "${wse_ledger_line}")
	list(GET wse_ledger_fields 0 wse_ledger_id)
	list(FIND wse_inventory_ids "${wse_ledger_id}" wse_ledger_id_index)
	if(wse_ledger_id_index EQUAL -1)
		message(FATAL_ERROR
			"Deprecation ledger id is missing from the legacy removal inventory: ${wse_ledger_id}")
	endif()
endforeach()

message(STATUS
	"Legacy removal inventory passed: ${wse_inventory_total} tracked surfaces cover the ledger")
