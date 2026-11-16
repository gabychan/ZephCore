/**
 * Zephtacore LoRa Mathematical Airtime & Link Budget Calculator
 * Implements standard Semtech LoRa modulation formulas.
 */
window.LoRaCalc = {
  calculateAirtime: function (opts) {
    const sf = parseInt(opts.sf || 7, 10);
    const bw = parseFloat(opts.bw || 62.5) * 1000; // in Hz
    const cr = parseInt(opts.cr || 2, 10); // 1 = 4/5, 2 = 4/6, etc.
    const pl = parseInt(opts.payloadBytes || 45, 10); // typical packet size
    const preambleLen = parseInt(opts.preamble || 16, 10); // Meshtastic uses 16 symbols
    const crc = opts.crc !== false ? 1 : 0;
    const explicitHeader = opts.header !== false ? 0 : 1; // 0 = explicit, 1 = implicit

    // Symbol duration in seconds
    const ts = Math.pow(2, sf) / bw;

    // Low Data Rate Optimization (mandatory if symbol duration > 16ms)
    const de = ts > 0.016 ? 1 : 0;

    // Preamble duration
    const tPreamble = (preambleLen + 4.25) * ts;

    // Payload symbol count
    const num = 8 * pl - 4 * sf + 28 + 16 * crc - 20 * explicitHeader;
    const den = 4 * (sf - 2 * de);
    const ceilVal = Math.ceil(num / den);
    const nPayload = 8 + Math.max(ceilVal * (cr + 4), 0);

    // Total packet airtime in ms
    const tPayload = nPayload * ts;
    const totalAirtimeMs = (tPreamble + tPayload) * 1000;

    // Raw bitrate in bps
    const bitRateBps = sf * (bw / Math.pow(2, sf)) * (4 / (4 + cr));

    // Approximate sensitivity in dBm for SX1262
    // -174 + 10*log10(BW) + NF (6dB) + SNR_req
    const snrLimits = { 7: -7.5, 8: -10, 9: -12.5, 10: -15, 11: -17.5, 12: -20 };
    const snrReq = snrLimits[sf] || -7.5;
    const sensitivityDbm = -174 + 10 * Math.log10(bw) + 6 + snrReq;

    return {
      airtimeMs: Math.round(totalAirtimeMs * 10) / 10,
      airtimeSec: Math.round((totalAirtimeMs / 1000) * 100) / 100,
      bitRateBps: Math.round(bitRateBps),
      symbolDurationMs: Math.round(ts * 1000 * 100) / 100,
      sensitivityDbm: Math.round(sensitivityDbm * 10) / 10,
      ldro: de === 1
    };
  }
};
