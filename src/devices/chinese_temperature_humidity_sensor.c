/** @file

    Unknown brand chinese outdoor meteo sensor

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**

Unknown brand chinese outdoor meteo sensor
measures temperature and humidity

Transmit Interval: every ~50s.
Message Format: 42 bits (10.5 nibbles).

    Byte:      0        1        2        3        4
    Nibble:    1   2    3   4    5   6    7   8    9   10   11
    Type:      IIIIIIII ??CCTTTT TTTTTTTT HHHHHHHH ???BXXXX XX

- I: sensor ID (changes on battery change)
- C: channel number
- T: temperature
- H: humidity
- B: battery low flag (voltage below 2.6V)
- ?: unknown meaning
- X: checksum

Example data:

    [01] {42} 0e 20 cd 80 0c 40 : 00001110 00100000 11001101 10000000 00001100 01 ---> Temp/Hum/Ch : 20.5/64/3

Temperature:
- Sensor sends data in °C scaled by 10, signed (>2049 means negative temperature)
- in this case: `0000 1100 1101` = 205/10 = 20.5 °C
- in this case: `1111 1010 0101` = (4005-4096)/10 = -9.1 °C

Humidity:
- 8 bit unsigned (Nibbles 8,7) scaled by 2
- in this case `10000000` = 128/2 = 64 %

Channel number: (Bits 10,11) + 1
- in this case `02` --> `02` +1 = Channel3

Random Code / Device ID: (Nibble 1)
- changes on every battery change

Checksum:
- sum of all previous nibles in the payload, divided by modulo 64

*/

#include "decoder.h"

static int noname_chinese_callback(r_device *decoder, bitbuffer_t *bitbuffer)
{
    uint8_t b[6];
    data_t *data;

    // the signal should have 6 repeats with a sync pulse between
    // require at least 4 received repeats
    int r = bitbuffer_find_repeated_row(bitbuffer, 4, 42);
    if (r < 0 || bitbuffer->bits_per_row[r] != 42)
        return DECODE_ABORT_LENGTH;

    bitbuffer_extract_bytes(bitbuffer, r, 0, b, 42);

    // No need to decode/extract values for simple test
    // check id channel temperature humidity value not zero
    if (!b[0] && !b[1] && !b[2] && !b[3]) {
        if (decoder->verbose > 1) {
            fprintf(stderr, "%s: DECODE_FAIL_SANITY data all 0x00\n", __func__);
        }
        return DECODE_FAIL_SANITY;
    }

    if (decoder->verbose > 1) {
        fprintf(stderr, "%s: hex input: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x | %02x%02x%02x%02x%02x%02x\n", __func__, b[0], b[1], b[2], b[3], b[4], b[5], b[0], b[1], b[2], b[3], b[4], b[5]);
    }

    uint8_t checksum_recv = 0, checksum = 0;
    for (int i=0; i<4; i++) {
        checksum += (b[i] & 0xf0) >> 4;
        checksum += b[i] & 0x0f;
    }
    checksum += (b[4] & 0xf0) >> 4;
    checksum &= 0x3f;

    checksum_recv = ((b[4] & 0x0f) << 2) + ((b[5] & 0xc0) >> 6);

    if (decoder->verbose > 1) {
        fprintf(stderr, "%s: checksum: 0x%02x, checksum_recv: 0x%02x\n", __func__, checksum, checksum_recv);
    }

    if (checksum != checksum_recv ) {
        if (decoder->verbose > 1) {
            fprintf(stderr, "%s: checksum mismatch!\n", __func__);
        }
        return DECODE_FAIL_MIC;
    }

    int id          = b[0];
    int channel     = ((b[1] & 0x30) >> 4) + 1;
    int temp_raw    = ((b[1] & 0x0f) << 8) | (b[2] & 0xff);
    float temp_f    = temp_raw * 0.1f;

    if ( temp_raw & 0x800 ) {
        temp_f = (temp_raw - 4096) * 0.1f;
    }

    int humidity    = b[3] >> 1;
    int battery_low = (b[4] & 0x10) >> 4;

    // check valid range for temperature
    if ( temp_f > 200 || temp_f < -50  ) {
        if (decoder->verbose > 1) {
            fprintf(stderr, "%s: DECODE_FAIL_SANITY invalid temperature: %f\n", __func__, temp_f);
        }
        return DECODE_FAIL_SANITY;
    }

    // check valid range for humidity
    if ( humidity > 100 ) {
        if (decoder->verbose > 1) {
            fprintf(stderr, "%s: DECODE_FAIL_SANITY invalid humidity: %d\n", __func__, humidity);
        }
        return DECODE_FAIL_SANITY;
    }

    /* clang-format off */
    data = data_make(
            "model",            "",             DATA_STRING, "Noname chinese outdoor temperature & humidity sensor",
            "id",               "ID",           DATA_INT, id,
            "channel",          "Channel",      DATA_INT, channel,
            "battery",          "Battery",      DATA_STRING, battery_low ? "LOW" : "OK",
            "temperature_C",    "Temperature",  DATA_FORMAT, "%.01f C", DATA_DOUBLE, temp_f,
            "humidity",         "Humidity",     DATA_FORMAT, "%u %%", DATA_INT, humidity,
            "mic",              "Integrity",    DATA_STRING, "CHECKSUM",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data);
    return 1;
}

static char *output_fields[] = {
    "model",
    "id",
    "channel",
    "battery",
    "temperature_C",
    "humidity",
    "mic",
    NULL,
};

r_device chinese_temperature_humidity_sensor = {
    .name           = "Noname chinese outdoor temperature & humidity sensor",
    .modulation     = OOK_PULSE_PPM,
    .short_width    = 2000,
    .long_width     = 4000,
    .gap_limit      = 9000,
    .reset_limit    = 100000,
    .decode_fn      = &noname_chinese_callback,
    .disabled       = 0,
    .fields         = output_fields,
};
