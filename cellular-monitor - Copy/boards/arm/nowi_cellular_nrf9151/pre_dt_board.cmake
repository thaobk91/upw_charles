# Copyright (c) 2024 NOWi
# SPDX-License-Identifier: Apache-2.0

# This file is executed before devicetree processing
# to allow board-specific setup

# Enable TF-M
list(APPEND EXTRA_DTC_FLAGS "-Wno-unique_unit_address_if_enabled")
