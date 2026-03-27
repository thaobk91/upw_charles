# Copyright (c) 2024 NOWi
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BOARD_NOWI_CELLULAR_NRF9151_NS)
  set(TFM_PUBLIC_KEY_FORMAT "full")
endif()

if(CONFIG_TFM_FLASH_MERGED_BINARY)
  set_property(TARGET runners_yaml_props_target PROPERTY hex_file tfm_merged.hex)
endif()

board_runner_args(jlink "--device=nRF9151_xxCA" "--speed=4000")

# NCS 3.1.0 uses nrfutil device by default, but keep jlink and nrfjprog as alternatives
include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)