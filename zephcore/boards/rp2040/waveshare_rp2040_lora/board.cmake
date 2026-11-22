# SPDX-License-Identifier: Apache-2.0

# UF2 drag-and-drop: hold BOOT while plugging in, the board appears as RPI-RP2.
board_runner_args(uf2 "--board-id=RPI-RP2")

board_set_flasher_ifnset(uf2)

include(${ZEPHYR_BASE}/boards/common/uf2.board.cmake)
