#include "test.h"
#include <NodePrefs.h>
#include <cmath>
#include <zephyr/drivers/lora/zc_lora_timing.h>
#include <lr11xx_cad_peak.h>

// Same derivation as the production call site: floor(2^sf * 1e6 / bw_hz).
static uint32_t sym_us_for(uint8_t sf, uint32_t bw_hz)
{
    return (uint32_t)(((uint64_t)(1U << sf) * 1000000ULL) / bw_hz);
}

TEST(lora_grace, "UNIT-LORATIME-001", "Preamble grace is (preamble+8) symbols rounded up with an unconfigured default") {
    CHECK(zc_lora_preamble_grace_ms(7, 62500, 32) == 82);      // 40 * 2^7 / 62.5 kHz = 81.92 ms
    CHECK(zc_lora_preamble_grace_ms(12, 125000, 16) == 787);   // 24 * 2^12 / 125 kHz = 786.4 ms
    CHECK(zc_lora_preamble_grace_ms(7, 500000, 8) == 5);       // 4.096 ms rounds up
    CHECK(zc_lora_preamble_grace_ms(7, 0, 32) == 1000);
    CHECK(zc_lora_preamble_grace_ms(4, 125000, 32) == 1000);
    CHECK(zc_lora_preamble_grace_ms(13, 125000, 32) == 1000);
}
TEST(lora_max_payload, "UNIT-LORATIME-002", "Max-payload bound is 255 B at CR 4/8 with LDRO plus 25% and 100 ms") {
    CHECK(zc_lora_max_payload_ms(7, 125000) == 1165);          // 832 symbols: 852 ms + 213 + 100
    CHECK(zc_lora_max_payload_ms(12, 7812) == 272747);         // longest reachable preset, > 262 s
    CHECK(zc_lora_max_payload_ms(0, 125000) == 30000 && zc_lora_max_payload_ms(7, 0) == 30000);
    uint32_t prev = 0;
    for (uint8_t sf = 5; sf <= 12; ++sf) {                     // monotonic in SF at a fixed bandwidth
        uint32_t ms = zc_lora_max_payload_ms(sf, 62500);
        CHECK(ms > prev); prev = ms;
    }
    CHECK(zc_lora_max_payload_ms(9, 62500) > zc_lora_max_payload_ms(9, 125000));
}
TEST(lora_steps24, "UNIT-LORATIME-003", "Chip timer conversion saturates at 24 bits instead of wrapping") {
    CHECK(zc_lora_ms_to_steps24(1000, 32768) == 32768);
    CHECK(zc_lora_ms_to_steps24(262143, 64000) == 16777152);   // just inside the SX126x field
    CHECK(zc_lora_ms_to_steps24(262144, 64000) == 0x00FFFFFFu);
    CHECK(zc_lora_ms_to_steps24(272747, 64000) == 0x00FFFFFFu); // SF12/BW7.81 bound: masked it read ~11 s
    CHECK(zc_lora_ms_to_steps24(600000, 32768) == 0x00FFFFFFu);
    CHECK(zc_lora_ms_to_steps24(0xFFFFFFFFu, 64000) == 0x00FFFFFFu); // no 32-bit overflow on the way
    CHECK(zc_lora_ms_to_steps24(0, 32768) == 0);
}
TEST(lora_duty_window, "UNIT-LORATIME-006", "RX duty window at SF7 and SF8 on 62.5 kHz sleeps 44 and 46 percent") {
    // P32 D8 margin 5% wake 1500: the operating point S1/S2 pins.
    auto sf7 = zc_lora_rx_duty_window(7, 62500, 32, 8, 5, 1500);
    CHECK(sf7.reason == ZC_LORA_RX_DUTY_OK);
    CHECK(sf7.rx_us == 33262);
    CHECK(sf7.sleep_us == 27684);
    CHECK(sf7.trans_us == 1500);
    auto sf8 = zc_lora_rx_duty_window(8, 62500, 32, 8, 5, 1500);
    CHECK(sf8.reason == ZC_LORA_RX_DUTY_OK);
    CHECK(sf8.rx_us == 65774);
    CHECK(sf8.sleep_us == 56868);
    // The cycle and the sleep fraction the boot line reports.
    CHECK(sf7.rx_us + sf7.sleep_us + sf7.trans_us == 62446);
    CHECK((uint64_t)sf7.sleep_us * 100 / (sf7.rx_us + sf7.sleep_us + sf7.trans_us) == 44);
    CHECK((uint64_t)sf8.sleep_us * 100 / (sf8.rx_us + sf8.sleep_us + sf8.trans_us) == 45);
}
TEST(lora_duty_window_sf6, "UNIT-LORATIME-007", "SF6 needs 12 detect symbols, engages on 62.5 kHz and falls back on 125 kHz") {
    auto sf6 = zc_lora_rx_duty_window(6, 62500, 32, 12, 5, 1500);
    CHECK(sf6.reason == ZC_LORA_RX_DUTY_OK);
    CHECK(sf6.rx_us == 20897);
    CHECK(sf6.sleep_us == 5310);
    // 7 symbols of catch budget at 512 us, derated 5%, is 3405 us: under
    // the 3500 us the 1500 us wake plus its 2000 us floor needs.
    auto sf6_125 = zc_lora_rx_duty_window(6, 125000, 32, 12, 5, 1500);
    CHECK(sf6_125.reason == ZC_LORA_RX_DUTY_WAKE_TOO_LONG);
    CHECK(sf6_125.deaf_us == 3405);
}
TEST(lora_duty_window_fallback, "UNIT-LORATIME-008", "A short preamble or an unconfigured bandwidth rejects instead of dividing") {
    // SF9 keeps the 32 symbol preamble; its catch reserve (2D+1) outgrows a
    // 16 symbol one, so a Meshtastic-forced P16 falls back to continuous RX.
    CHECK(zc_lora_rx_duty_window(9, 62500, 16, 8, 5, 1500).reason == ZC_LORA_RX_DUTY_PREAMBLE_SHORT);
    CHECK(zc_lora_rx_duty_window(9, 62500, 17, 8, 5, 1500).reason == ZC_LORA_RX_DUTY_PREAMBLE_SHORT);
    CHECK(zc_lora_rx_duty_window(9, 62500, 18, 8, 5, 1500).reason == ZC_LORA_RX_DUTY_OK);
    CHECK(zc_lora_rx_duty_window(7, 0, 32, 8, 5, 1500).reason == ZC_LORA_RX_DUTY_PREAMBLE_SHORT);
    // The rejection carries no window for a caller to arm.
    auto rejected = zc_lora_rx_duty_window(9, 62500, 16, 8, 5, 1500);
    CHECK(rejected.rx_us == 0 && rejected.sleep_us == 0 && rejected.trans_us == 1500);
}
TEST(lora_duty_window_complete, "UNIT-LORATIME-009", "Every viable window meets 2R + S >= (P+14) symbols, the DS 13.1.7 completion rule") {
    // A future tuning that grows the sleep fraction by shrinking rx breaks
    // this and gets caught here, not on the air.
    for (uint8_t sf = 5; sf <= 12; ++sf)
        for (uint16_t preamble : {16, 32, 64, 255})
            for (uint16_t detect : {8, 12})
                for (uint32_t margin : {0, 5, 15, 40}) {
                    auto w = zc_lora_rx_duty_window(sf, 62500, preamble, detect, margin, 1500);
                    if (w.reason != ZC_LORA_RX_DUTY_OK) {
                        continue;
                    }
                    uint32_t sym = sym_us_for(sf, 62500);
                    CHECK((uint64_t)2 * w.rx_us + w.sleep_us >= (uint64_t)(preamble + 14) * sym);
                    CHECK(w.rx_us >= (uint32_t)(detect + 1) * sym);          // detection floor
                    CHECK((uint64_t)w.sleep_us + 1500 <= (uint64_t)(preamble - 2 * detect - 1) * sym); // catch
                }
    // Only the 1500 us wake and a 0 % derate reach this across the presets.
    for (uint8_t sf = 5; sf <= 12; ++sf) {
        uint32_t sym = sym_us_for(sf, 62500);
        auto w = zc_lora_rx_duty_window(sf, 62500, 32, 8, 0, 1500);
        CHECK(w.reason == ZC_LORA_RX_DUTY_OK);
        CHECK((uint64_t)2 * w.rx_us + w.sleep_us == (uint64_t)46 * sym);
    }
}
TEST(lr11xx_cad_peak, "UNIT-LORATIME-004", "LR11xx detPeak base table cells symbol adjustment and clamp headroom") {
    CHECK(lr11xx_cad_detect_peak(7, 62, 4) == 44);             // default preset, sub-125 row
    CHECK(lr11xx_cad_detect_peak(8, 125, 4) == 57);
    CHECK(lr11xx_cad_detect_peak(5, 500, 8) == 63);
    CHECK(lr11xx_cad_detect_peak(12, 250, 2) == 75);           // under 4 symbols: no adjustment
    CHECK(lr11xx_cad_detect_peak(3, 125, 2) == lr11xx_cad_detect_peak(9, 125, 2)); // out of range -> SF9
    for (uint8_t sf = 5; sf <= 12; ++sf)
        for (uint16_t bw : {31, 62, 125, 250, 500})
            for (uint8_t symb : {1, 2, 4, 8, 16}) {
                uint8_t p = lr11xx_cad_detect_peak(sf, bw, symb);
                CHECK(p >= LR11XX_CAD_PEAK_MIN && p <= LR11XX_CAD_PEAK_MAX);
            }
}

TEST(duty_percent, "UNIT-LORATIME-005", "Duty-cycle percent from the airtime factor is never 0") {
    CHECK(zcDutyCyclePercent(0.0f) == 100 && zcDutyCyclePercent(1.0f) == 50 && zcDutyCyclePercent(9.0f) == 10);
    CHECK(zcDutyCyclePercent(199.0f) == 1 && zcDutyCyclePercent(200.0f) == 1);
    CHECK(zcDutyCyclePercent(4294967.0f) == 1 && zcDutyCyclePercent(-5.0f) == 1 && zcDutyCyclePercent(NAN) == 1);
}
