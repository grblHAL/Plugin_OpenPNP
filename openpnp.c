/*

  openpnp.c - plugin for for OpenPNP required M-codes

  Part of grblHAL

  Copyright (c) 2021-2022 Terje Io

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "driver.h"

#if OPENPNP_ENABLE

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "grbl/hal.h"
#include "grbl/protocol.h"

static user_mcode_ptrs_t user_mcode;
static on_report_options_ptr on_report_options;
static uint8_t tport;

static user_mcode_t userMCodeCheck (user_mcode_t mcode)
{
    return mcode == OpenPNP_SetPinState || mcode == OpenPNP_GetADCReading || mcode == OpenPNP_GetCurrentPosition || mcode == OpenPNP_FirmwareInfo ||
             mcode == OpenPNP_SetAcceleration || mcode == OpenPNP_FinishMoves || mcode == OpenPNP_SettingsReset
            ? mcode
            : (user_mcode.check ? user_mcode.check(mcode) : UserMCode_Ignore);
}

static status_code_t userMCodeValidate (parser_block_t *gc_block, parameter_words_t *deprecated)
{
    status_code_t state = Status_GcodeValueWordMissing;

    UNUSED(deprecated);

    switch(gc_block->user_mcode) {

        case OpenPNP_SetPinState:
            if(gc_block->words.p && gc_block->words.s) {

                if(gc_block->words.p && isnan(gc_block->values.p))
                    state = Status_BadNumberFormat;

                if(gc_block->words.s && isnan(gc_block->values.s))
                    state = Status_BadNumberFormat;

                if(state != Status_BadNumberFormat) {
                    if(gc_block->values.p <= 255.0f && (uint8_t)gc_block->values.p < hal.port.num_digital_out) {
                        gc_block->words.p = gc_block->words.s = Off;
                        state = Status_OK;
                    } else
                        state = Status_InvalidStatement;
                }
            }
            break;

        case OpenPNP_GetADCReading:
            if(gc_block->words.t) {
                if(gc_block->values.t < hal.port.num_analog_in) {
                    gc_block->words.t = Off;
                    state = Status_OK;
                } else
                    state = Status_InvalidStatement;
            }
            break;

        case OpenPNP_GetCurrentPosition:
            gc_block->words.d = gc_block->words.r = Off;
            state = Status_OK;
            break;

        case OpenPNP_SetAcceleration:
            if(gc_block->words.p && isnan(gc_block->values.p))
                state = Status_BadNumberFormat;

            if(gc_block->words.r && isnan(gc_block->values.r))
                state = Status_BadNumberFormat;

            if(gc_block->words.s && isnan(gc_block->values.s))
                state = Status_BadNumberFormat;

            if(state != Status_BadNumberFormat) {
                gc_block->words.p = gc_block->words.r = gc_block->words.s = gc_block->words.t = Off;
                // TODO: add validation
                state = Status_OK;
            }
            break;

        case OpenPNP_FirmwareInfo:
        case OpenPNP_FinishMoves:
        case OpenPNP_SettingsReset:
            state = Status_OK;
            break;

        default:
            state = Status_Unhandled;
            break;
    }

    return state == Status_Unhandled && user_mcode.validate ? user_mcode.validate(gc_block, deprecated) : state;
}

static void report_position (bool real, bool detailed)
{
    uint_fast8_t idx;
    int32_t current_position[N_AXIS];
    float print_position[N_AXIS];
    char buf[(STRLEN_COORDVALUE + 4) * N_AXIS];

    if(real || detailed) {
        memcpy(current_position, sys.position, sizeof(sys.position));

    if(real)
        system_convert_array_steps_to_mpos(print_position, current_position);
    } else
        memcpy(print_position, gc_state.position, sizeof(print_position));

    *buf = '\0';
    for (idx = 0; idx < N_AXIS; idx++) {
        print_position[idx] -= gc_get_offset(idx);
        strcat(buf, axis_letter[idx]);
        strcat(buf, ":");
        strcat(buf, ftoa(print_position[idx], N_DECIMAL_COORDVALUE_MM)); // always mm and 3 decimals?
        strcat(buf, idx == N_AXIS - 1 ? (detailed ? " " : ASCII_EOL) : " ");
    }

    hal.stream.write(buf);

    if(detailed) {

        *buf = '\0';
        hal.stream.write("Count ");

        for (idx = 0; idx < N_AXIS; idx++) {
            print_position[idx] -= gc_get_offset(idx);
            strcat(buf, axis_letter[idx]);
            strcat(buf, ":");
            itoa(current_position[idx], strchr(buf, '\0'), 10);
            strcat(buf, idx == N_AXIS - 1 ? ASCII_EOL : " ");
        }

        hal.stream.write(buf);
    }
}

static void report_temperature (sys_state_t state)
{
    int32_t v = hal.port.wait_on_input(false, tport, WaitMode_Immediate, 0.0f);
    // format output -> T:21.17 /0.0000 B:21.04 /0.0000 @:0 B@:0
}

static void userMCodeExecute (uint_fast16_t state, parser_block_t *gc_block)
{
    bool handled = true;

    if (state != STATE_CHECK_MODE)
      switch(gc_block->user_mcode) {

        case OpenPNP_SetPinState:
            hal.port.digital_out(gc_block->values.p, gc_block->values.s != 0.0f);
            break;

        case OpenPNP_GetADCReading: // Request temperature report
            tport = gc_block->values.t;
            protocol_enqueue_rt_command(report_temperature);
            break;

        case OpenPNP_GetCurrentPosition:
            report_position(gc_block->words.r, gc_block->words.d);
            break;

        case OpenPNP_FirmwareInfo:
            hal.stream.write("FIRMWARE_NAME:grblHAL ");
            hal.stream.write("FIRMWARE_URL:https%3A//github.com/grblHAL ");
            hal.stream.write("FIRMWARE_VERSION:" GRBL_VERSION " ");
            hal.stream.write("FIRMWARE_BUILD:");
            hal.stream.write(uitoa(GRBL_BUILD));
            hal.stream.write(ASCII_EOL);
            break;

        case OpenPNP_SetAcceleration: // Set acceleration
            {
                uint_fast8_t idx = N_AXIS;

                protocol_buffer_synchronize();
                do {
                    idx--;
                    if(gc_block->words.s || idx == X_AXIS || idx == Y_AXIS)
                        settings_override_acceleration(idx, gc_block->words.s ? gc_block->values.s : gc_block->values.t);
                    else
                        settings_override_acceleration(idx, gc_block->values.p);
                } while(idx);
            }
            break;

        case OpenPNP_FinishMoves: // Wait for buffered motions to complete
            protocol_buffer_synchronize();
            break;

        case OpenPNP_SettingsReset: // Restore acceleration to configured values
            {
                uint_fast8_t idx = N_AXIS;

                protocol_buffer_synchronize();
                do {
                    settings_override_acceleration(--idx, 0.0f);
                } while(idx);
            }
            break;

        default:
            handled = false;
            break;
    }

    if(!handled && user_mcode.execute)
        user_mcode.execute(state, gc_block);
}

static void onReportOptions (bool newopt)
{
    on_report_options(newopt);

    if(!newopt)
        hal.stream.write("[PLUGIN:OpenPNP v0.02]" ASCII_EOL);
}

void openpnp_init (void)
{
    memcpy(&user_mcode, &hal.user_mcode, sizeof(user_mcode_ptrs_t));

    hal.user_mcode.check = userMCodeCheck;
    hal.user_mcode.validate = userMCodeValidate;
    hal.user_mcode.execute = userMCodeExecute;

    on_report_options = grbl.on_report_options;
    grbl.on_report_options = onReportOptions;
}

#endif
