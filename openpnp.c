/*

  openpnp.c - plugin for for OpenPNP required M-codes

  Part of grblHAL

  Copyright (c) 2021-2025 Terje Io

  grblHAL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grblHAL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grblHAL. If not, see <http://www.gnu.org/licenses/>.

*/

#include "driver.h"

#if OPENPNP_ENABLE

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "grbl/hal.h"
#include "grbl/protocol.h"
#include "grbl/task.h"

#define NUM_ADCINPUTS 8

typedef struct {
    float factor;
    float offset;
} adc_scaling_t;

static uint8_t n_adc_inputs = 0, n_digital_outputs = 0;
static adc_scaling_t adc_scaling[NUM_ADCINPUTS];
static user_mcode_ptrs_t user_mcode;
static on_report_options_ptr on_report_options;

static user_mcode_type_t userMCodeCheck (user_mcode_t mcode)
{
    return mcode == OpenPNP_SetPinState || mcode == OpenPNP_GetCurrentPosition || mcode == OpenPNP_FirmwareInfo ||
            mcode == OpenPNP_GetADCRaw || mcode == OpenPNP_GetADCScaled || mcode == OpenPNP_SetADCScaling ||
             mcode == OpenPNP_SetAcceleration || mcode == OpenPNP_FinishMoves || mcode == OpenPNP_SettingsReset
#if ENABLE_JERK_ACCELERATION
             || mcode == OpenPNP_SetJerk
#endif
            ? UserMCode_Normal
            : (user_mcode.check ? user_mcode.check(mcode) : UserMCode_Unsupported);
}

#if ENABLE_JERK_ACCELERATION

static const parameter_words_t wordmap[] = {
   { .x = On },
   { .y = On },
   { .z = On },
#if N_AXIS > 3
   { .a = On },
   { .b = On },
   { .c = On },
#endif
#if N_AXIS > 6
   { .u = On },
   { .v = On }
#endif
};

// Validate M-code axis parameters
static axes_signals_t mcode_validate_axis_values (parser_block_t *gc_block)
{
    uint_fast8_t idx = N_AXIS;
    axes_signals_t axes = {};

    do {
        idx--;
        if(gc_block->words.mask & wordmap[idx].mask) {
            if(!isnan(gc_block->values.xyz[idx])) {
                bit_true(axes.bits, bit(idx));
                gc_block->words.mask &= ~wordmap[idx].mask;
            }
        }
    } while(idx);

    return axes;
}

#endif // ENABLE_JERK_ACCELERATION

static status_code_t userMCodeValidate (parser_block_t *gc_block)
{
    status_code_t state = Status_GcodeValueWordMissing;

    switch(gc_block->user_mcode) {

        case OpenPNP_SetPinState:
            if(gc_block->words.p && gc_block->words.s) {
                if(gc_block->values.p <= 255.0f && (uint8_t)gc_block->values.p < n_digital_outputs) {
                    gc_block->words.p = gc_block->words.s = Off;
                    state = Status_OK;
                } else
                    state = Status_InvalidStatement;
            }
            break;

        case OpenPNP_GetCurrentPosition:
            gc_block->words.d = gc_block->words.r = Off;
            state = Status_OK;
            break;

        case OpenPNP_GetADCRaw:
        case OpenPNP_GetADCScaled:
            if(gc_block->words.p && (uint8_t)gc_block->values.p < n_adc_inputs) {
                gc_block->words.p = Off;
                state = Status_OK;
            }
            break;

        case OpenPNP_SetADCScaling:
            if(gc_block->words.p && (uint8_t)gc_block->values.p < n_adc_inputs && gc_block->words.q && gc_block->words.s) {
                uint_fast8_t port;
                if((port = (uint8_t)gc_block->values.p) < n_adc_inputs) {
                    state = Status_OK;

                    gc_block->words.p = gc_block->words.q = gc_block->words.s = Off;
                } else
                    state = Status_InvalidStatement;
            }
            break;

        case OpenPNP_SetAcceleration:
            gc_block->words.p = gc_block->words.r = gc_block->words.s = gc_block->words.t = Off;
            // TODO: add validation
            state = Status_OK;
            break;

#if ENABLE_JERK_ACCELERATION
        case OpenPNP_SetJerk:
            if(mcode_validate_axis_values(gc_block).value)
                state = mcode_validate_axis_values(gc_block).value ? Status_OK : Status_GcodeNoAxisWords;
            break;
#endif

        case OpenPNP_FirmwareInfo:
        case OpenPNP_FinishMoves:
        case OpenPNP_SettingsReset:
            state = Status_OK;
            break;

        default:
            state = Status_Unhandled;
            break;
    }

    return state == Status_Unhandled && user_mcode.validate ? user_mcode.validate(gc_block) : state;
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
    for(idx = 0; idx < N_AXIS; idx++) {
        print_position[idx] -= gc_get_offset(idx, true);
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
            print_position[idx] -= gc_get_offset(idx, true);
            strcat(buf, axis_letter[idx]);
            strcat(buf, ":");
            itoa(current_position[idx], strchr(buf, '\0'), 10);
            strcat(buf, idx == N_AXIS - 1 ? ASCII_EOL : " ");
        }

        hal.stream.write(buf);
    }
}

static void userMCodeExecute (uint_fast16_t state, parser_block_t *gc_block)
{
    bool handled = true;

    if(state != STATE_CHECK_MODE)
      switch(gc_block->user_mcode) {

        case OpenPNP_SetPinState:
            hal.port.digital_out(gc_block->values.p, gc_block->values.s != 0.0f);
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

        case OpenPNP_GetADCRaw:
        case OpenPNP_GetADCScaled:
            {
                float v = (float)hal.port.wait_on_input(false, (uint8_t)gc_block->values.p, WaitMode_Immediate, 0.0f);
                if(gc_block->user_mcode == OpenPNP_GetADCScaled) {
                    v += adc_scaling[(uint8_t)gc_block->values.p].offset;
                    v *= adc_scaling[(uint8_t)gc_block->values.p].factor;
                }
                hal.stream.write("A");
                hal.stream.write(uitoa((uint32_t)gc_block->values.p));
                hal.stream.write(":");
                hal.stream.write(ftoa(v, 2));
                hal.stream.write(ASCII_EOL);
            }
            break;

        case OpenPNP_SetADCScaling:
            adc_scaling[(uint8_t)gc_block->values.p].factor = gc_block->values.s;
            adc_scaling[(uint8_t)gc_block->values.p].offset = gc_block->values.q;
            break;

        case OpenPNP_SetAcceleration:
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

#if ENABLE_JERK_ACCELERATION
        case OpenPNP_SetJerk:
            {
                uint_fast8_t idx = N_AXIS;
                axes_signals_t axes = mcode_validate_axis_values(gc_block);

                protocol_buffer_synchronize();
                do {
                    if(bit_istrue(axes.bits, bit(--idx)))
                        settings_override_jerk(idx, gc_block->values.xyz[idx]);
                } while(idx);
            }
            break;
#endif

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
        report_plugin("OpenPNP", "0.08");
}

static void openpnp_configure (void *data)
{
    n_digital_outputs = ioports_unclaimed(Port_Digital, Port_Output);
    if((n_adc_inputs = ioports_unclaimed(Port_Analog, Port_Input)) > NUM_ADCINPUTS)
        n_adc_inputs = NUM_ADCINPUTS;
}

void openpnp_init (void)
{
    uint_fast8_t idx = sizeof(adc_scaling) / sizeof(adc_scaling_t);

    do {
        adc_scaling[--idx].factor = 1.0f;
        adc_scaling[idx].offset = 0.0f;
    } while(idx);

    memcpy(&user_mcode, &grbl.user_mcode, sizeof(user_mcode_ptrs_t));

    grbl.user_mcode.check = userMCodeCheck;
    grbl.user_mcode.validate = userMCodeValidate;
    grbl.user_mcode.execute = userMCodeExecute;

    on_report_options = grbl.on_report_options;
    grbl.on_report_options = onReportOptions;

    task_run_on_startup(openpnp_configure, NULL);
}

#endif // OPENPNP_ENABLE
