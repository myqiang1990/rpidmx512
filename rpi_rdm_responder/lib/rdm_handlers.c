/**
 * @file rdm_handlers.c
 *
 */
/* Copyright (C) 2015 by Arjan van Vught <pm @ http://www.raspberrypi.org/forum/>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <string.h>

#include "sys_time.h"
#include "monitor_debug.h"
#include "hardware.h"
#include "util.h"
#include "dmx.h"
#include "rdm.h"
#include "rdm_send.h"
#include "rdm_e120.h"
#include "rdm_device_info.h"

static uint8_t rdm_is_mute = FALSE;
static uint8_t rdm_identify_mode_enabled = FALSE;

// Unique identifier (UID) which consists of a 2 byte ESTA manufacturer ID, and a 4 byte device ID.
static const uint8_t UID_ALL[RDM_UID_SIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

//static void rdm_get_queued_message(void);
static void rdm_get_supported_parameters(void);
static void rdm_get_device_info(void);
static void rdm_get_device_model_description(void);
static void rdm_get_manufacturer_label(void);
static void rdm_get_device_label(void);
static void rdm_set_device_label(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_factory_defaults(void);
static void rdm_set_factory_defaults(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_language(void);
static void rdm_set_language(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_software_version_label(void);
static void rdm_get_boot_software_version_id(void);
static void rdm_get_boot_software_version_label(void);
static void rdm_get_personality(void);
static void rdm_set_personality(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_personality_description(void);
static void rdm_get_dmx_start_address(void);
static void rdm_set_dmx_start_address(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_device_hours(void);
static void rdm_set_device_hours(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_identify_device(void);
static void rdm_set_identify_device(uint8_t was_broadcast, uint16_t sub_device);
static void rdm_get_real_time_clock(void);
static void rdm_set_reset_device(uint8_t was_broadcast, uint16_t sub_device);

struct _pid_definition
{
	uint16_t pid;
	void (*get_handler)(void);
	void (*set_handler)(uint8_t was_broadcast, uint16_t sub_device);
	uint8_t get_argument_size;
	uint8_t include_in_supported_params;
} const PID_DEFINITIONS[] = {
//		{E120_QUEUED_MESSAGE,              &rdm_get_queued_message,             NULL,                        1, TRUE },
		{E120_SUPPORTED_PARAMETERS,        &rdm_get_supported_parameters,       NULL,                        0, FALSE},
		{E120_DEVICE_INFO,                 &rdm_get_device_info,                NULL,                        0, FALSE},
		{E120_DEVICE_MODEL_DESCRIPTION,    &rdm_get_device_model_description,   NULL,                        0, TRUE },
		{E120_MANUFACTURER_LABEL,          &rdm_get_manufacturer_label,         NULL,                        0, TRUE },
		{E120_DEVICE_LABEL,                &rdm_get_device_label,               &rdm_set_device_label,       0, TRUE },
		{E120_FACTORY_DEFAULTS,            &rdm_get_factory_defaults,           &rdm_set_factory_defaults,   0, TRUE },
		{E120_LANGUAGE_CAPABILITIES,       &rdm_get_language,			        NULL,                        0, TRUE },
		{E120_LANGUAGE,				       &rdm_get_language,			        &rdm_set_language,           0, TRUE },
		{E120_SOFTWARE_VERSION_LABEL,      &rdm_get_software_version_label,     NULL,                        0, FALSE},
		{E120_BOOT_SOFTWARE_VERSION_ID,    &rdm_get_boot_software_version_id,   NULL,                        0, TRUE },
		{E120_BOOT_SOFTWARE_VERSION_LABEL, &rdm_get_boot_software_version_label,NULL,                        0, TRUE },
		{E120_DMX_PERSONALITY,		       &rdm_get_personality,                &rdm_set_personality,        0, TRUE },
		{E120_DMX_PERSONALITY_DESCRIPTION, &rdm_get_personality_description,    NULL,                        1, TRUE },
		{E120_DMX_START_ADDRESS,           &rdm_get_dmx_start_address,          &rdm_set_dmx_start_address,  0, FALSE},
		{E120_DEVICE_HOURS,                &rdm_get_device_hours,    	        &rdm_set_device_hours,       0, TRUE },
		{E120_REAL_TIME_CLOCK,		       &rdm_get_real_time_clock,            NULL,                        0, TRUE },
		{E120_IDENTIFY_DEVICE,		       &rdm_get_identify_device,		    &rdm_set_identify_device,    0, FALSE},
		{E120_RESET_DEVICE,			       NULL,                                &rdm_set_reset_device,       0, TRUE },
		};

/**
 *
 */
static void rdm_get_supported_parameters(void)
{
	uint8_t supported_params = 0;

	uint8_t i;
	for (i = 0;	i < sizeof(PID_DEFINITIONS) / sizeof(struct _pid_definition); i++)
	{
		if (PID_DEFINITIONS[i].include_in_supported_params)
			supported_params++;
	}

	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;

	rdm_response->param_data_length = (2 * supported_params);
	rdm_response->message_length = rdm_response->message_length + (2 * supported_params);

	int j = 0;
	for (i = 0;	i < sizeof(PID_DEFINITIONS) / sizeof(struct _pid_definition); i++)
	{
		if (PID_DEFINITIONS[i].include_in_supported_params)
		{
			rdm_response->param_data[j+j] = (uint8_t)(PID_DEFINITIONS[i].pid >> 8);
			rdm_response->param_data[j+j+1] = (uint8_t)PID_DEFINITIONS[i].pid;
			j++;
		}
	}

	rdm_send_respond_message_ack();

}

#if 0
static void rdm_get_queued_message(void)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *) rdm_data;
	rdm_response->port_id = E120_STATUS_MESSAGES;
	rdm_response->param_data_length = 0;

	rdm_send_respond_message_ack();
}
#endif

/**
 *
 * @return
 */
uint8_t rdm_is_mute_get(void)
{
	return rdm_is_mute;
}

/**
 *
 * @param name
 * @param lenght
 */
inline static void handle_string(const uint8_t *name, const uint8_t lenght)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *) rdm_data;

	rdm_response->param_data_length = lenght;
	rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + lenght;

	uint8_t i;
	for (i = 0; i < lenght; i++)
		rdm_response->param_data[i] = name[i];
}

/**
 *
 */
static void rdm_get_device_info(void)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;

	struct _rdm_device_info *rdm_device_info = rdm_device_info_get();

	struct _rdm_device_info *device_info = (struct _rdm_device_info *)rdm_response->param_data;
	rdm_response->param_data_length = sizeof(struct _rdm_device_info);
	rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + rdm_response->param_data_length;

	memcpy(device_info, rdm_device_info, sizeof(struct _rdm_device_info));

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_device_model_description(void)
{
	const uint8_t *board_model = hardware_get_board_model();
	const uint8_t board_model_length = hardware_get_board_model_length();
	handle_string(board_model, board_model_length);
	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_manufacturer_label(void)
{
	const uint8_t *manufacturer_name = rdm_device_info_get_manufacturer_name();
	const uint8_t manufacturer_name_length = rdm_device_info_get_manufacturer_name_length();
	handle_string(manufacturer_name, manufacturer_name_length);
	rdm_send_respond_message_ack();
}


/**
 *
 */
static void rdm_get_device_label(void)
{
	const uint8_t *device_name = rdm_device_info_get_label();
	const uint8_t device_name_length = rdm_device_info_get_label_length();
	handle_string(device_name, device_name_length);
	rdm_send_respond_message_ack();
}

/**
 *
 * @param was_broadcast
 * @param sub_device
 */
static void rdm_set_device_label(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length > 32)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	uint8_t device_label_length = rdm_command->param_data_length;
	uint8_t *device_label= &rdm_command->param_data[0];
	rdm_device_info_set_label(device_label, device_label_length);

	if(was_broadcast)
		return;

	rdm_command->param_data_length = 0;
	rdm_command->message_length = RDM_MESSAGE_MINIMUM_SIZE;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_factory_defaults(void)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length != 0)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	rdm_command->param_data_length = 1;
	rdm_command->message_length = RDM_MESSAGE_MINIMUM_SIZE + 1;
	rdm_command->param_data[0] = rdm_device_info_get_is_factory_defaults();

	rdm_send_respond_message_ack();
}

/**
 *
 * @param was_broadcast
 * @param sub_device
 */
static void rdm_set_factory_defaults(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length != 0)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	rdm_device_info_init();

	if(was_broadcast)
		return;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_language(void)
{
	const uint8_t *supported_language = rdm_device_info_get_supported_language();
	const uint8_t supported_language_length = rdm_device_info_get_supported_language_length();
	handle_string(supported_language, supported_language_length);
	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_software_version_label(void)
{
	const uint8_t *software_version = rdm_device_info_get_software_version();
	const uint8_t software_version_length = rdm_device_info_get_software_version_length();
	handle_string(software_version, software_version_length);
	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_boot_software_version_id(void)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length != 0)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	uint32_t boot_software_version_id = hardware_get_firmware_revision();

	rdm_command->param_data_length = 4;
	rdm_command->message_length = RDM_MESSAGE_MINIMUM_SIZE + 4;
	rdm_command->param_data[0] = (uint8_t)(boot_software_version_id >> 24);
	rdm_command->param_data[1] = (uint8_t)(boot_software_version_id >> 16);
	rdm_command->param_data[2] = (uint8_t)(boot_software_version_id >> 8);
	rdm_command->param_data[3] = (uint8_t)boot_software_version_id;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_boot_software_version_label(void)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length != 0)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	const uint8_t *firmware_copyright = hardware_get_firmware_copyright();
	const uint8_t firmware_copyright_length = hardware_get_firmware_copyright_length();
	handle_string(firmware_copyright, firmware_copyright_length);
	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_personality(void)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;
	uint8_t current_personality = rdm_device_info_get_current_personality();
	uint8_t personality_count = rdm_device_info_get_personality_count();
	rdm_response->param_data_length = 2;
	rdm_response->message_length = rdm_response->message_length + 2;
	rdm_response->param_data[0] = current_personality;
	rdm_response->param_data[1] = personality_count;

	rdm_send_respond_message_ack();
}

/**
 *
 * @param was_broadcast
 * @param sub_device
 */
static void rdm_set_personality(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length != 1)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	uint8_t personality = rdm_command->param_data[0];
	uint8_t max_personalities = rdm_device_info_get_personality_count();

	if ((personality == 0) || (personality > max_personalities))
	{
		rdm_send_respond_message_nack(E120_NR_DATA_OUT_OF_RANGE);
		return;
	}

	rdm_device_info_set_current_personality(personality);

	rdm_command->param_data_length = 0;
	rdm_command->message_length = RDM_MESSAGE_MINIMUM_SIZE;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_personality_description(void)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	uint8_t personality = rdm_command->param_data[0];
	uint8_t max_personalities = rdm_device_info_get_personality_count();

	if ((personality == 0) || (personality > max_personalities))
	{
		rdm_send_respond_message_nack(E120_NR_DATA_OUT_OF_RANGE);
		return;
	}

	uint16_t slots = rdm_device_info_get_personality_slots(personality);

	const char *description = rdm_device_info_get_personality_description(personality);
	uint8_t length = strlen(description);
	length = length > 32 ? 32 : length;

	rdm_command->param_data[1] = (uint8_t)(slots >> 8);
	rdm_command->param_data[2] = (uint8_t)(slots);

	uint8_t i = 0;
	for (i = 0; i < length; i++)
	{
		rdm_command->param_data[3 + i] = description[i];
	}

	rdm_command->param_data_length = 3 + length;
	rdm_command->message_length = RDM_MESSAGE_MINIMUM_SIZE + 3 + length;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_dmx_start_address(void)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;
	uint16_t dmx_start_address = rdm_device_info_get_dmx_start_address();
	rdm_response->param_data_length = 2;
	rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + 2;
	rdm_response->param_data[0] = (uint8_t)(dmx_start_address >> 8);
	rdm_response->param_data[1] = (uint8_t)dmx_start_address;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_device_hours(void)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;
	uint64_t device_hours = hardware_uptime_seconds() / 3600;
	rdm_response->param_data_length = 4;
	rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + 4;
	rdm_response->param_data[0] = (uint8_t)(device_hours >> 24);
	rdm_response->param_data[1] = (uint8_t)(device_hours >> 16);
	rdm_response->param_data[2] = (uint8_t)(device_hours >> 8);
	rdm_response->param_data[3] = (uint8_t)device_hours;

	rdm_send_respond_message_ack();
}

/**
 *
 * @param was_broadcast
 * @param sub_device
 */
static void rdm_set_device_hours(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;
	rdm_command->param_data_length = 0;
	rdm_command->message_length = RDM_MESSAGE_MINIMUM_SIZE;

	rdm_send_respond_message_nack(E120_NR_WRITE_PROTECT);
}

/**
 *
 */
static void rdm_get_identify_device(void)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;
	rdm_response->param_data_length = 1;
	rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + 1;
	rdm_response->param_data[0] = rdm_identify_mode_enabled;

	rdm_send_respond_message_ack();
}

/**
 *
 */
static void rdm_get_real_time_clock(void)
{
	time_t ltime = 0;
	struct tm *local_time = NULL;

	ltime = sys_time(NULL);
    local_time = localtime(&ltime);

	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;

	rdm_response->param_data_length = 7;
	rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + 7;

	uint16_t year = local_time->tm_year + 2000;

	rdm_response->param_data[0] = (uint8_t)(year >> 8);
	rdm_response->param_data[1] = (uint8_t)(year);
	rdm_response->param_data[2] = (uint8_t)(local_time->tm_mon);
	rdm_response->param_data[3] = (uint8_t)(local_time->tm_mday);
	rdm_response->param_data[4] = (uint8_t)(local_time->tm_hour);
	rdm_response->param_data[5] = (uint8_t)(local_time->tm_min);
	rdm_response->param_data[6] = (uint8_t)(local_time->tm_sec);

	rdm_send_respond_message_ack();
}


/**
 *
 * @param was_broadcast
 * @param sub_device
 */
static void rdm_set_identify_device(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *)rdm_data;

	if (rdm_command->param_data_length != 1)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	if ((rdm_command->param_data[0] != 0) && (rdm_command->param_data[0] != 1))
	{
		rdm_send_respond_message_nack(E120_NR_DATA_OUT_OF_RANGE);
		return;
	}

	rdm_identify_mode_enabled = rdm_command->param_data[0];

	//TODO Do something here Flash led?

	if(was_broadcast)
		return;

	rdm_command->param_data_length = 0;
	rdm_send_respond_message_ack();
}


static void rdm_set_reset_device(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_response = (struct _rdm_command *)rdm_data;
	rdm_response->param_data_length = 0;

	if(was_broadcast == FALSE)
		rdm_send_respond_message_ack();

	hardware_reboot();
}

static void rdm_set_dmx_start_address(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *) rdm_data;

	if (rdm_command->param_data_length != 2)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	uint16_t dmx_start_address = (rdm_command->param_data[0] << 8) + rdm_command->param_data[1];

	if ((dmx_start_address == 0) || (dmx_start_address > 512))
	{
		rdm_send_respond_message_nack(E120_NR_DATA_OUT_OF_RANGE);
		return;
	}

	rdm_device_info_set_dmx_start_address(dmx_start_address);

	if(was_broadcast)
		return;

	rdm_command->param_data_length = 0;
	rdm_send_respond_message_ack();
}

static void rdm_set_language(uint8_t was_broadcast, uint16_t sub_device)
{
	struct _rdm_command *rdm_command = (struct _rdm_command *) rdm_data;

	if (rdm_command->param_data_length != 2)
	{
		rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
		return;
	}

	const uint8_t *supported_language = rdm_device_info_get_supported_language();

	if ((rdm_command->param_data[0] != supported_language[0]) || (rdm_command->param_data[1] != supported_language[1]))
	{
		rdm_send_respond_message_nack(E120_NR_DATA_OUT_OF_RANGE);
		return;
	}

	if(was_broadcast)
		return;

	rdm_command->param_data_length = 0;
	rdm_send_respond_message_ack();
}

/**
 * @ingroup rdm_handlers
 *
 * @param is_broadcast
 * @param command_class
 * @param param_id
 * @param param_data_length
 * @param sub_device
 */
static void process_rdm_message(const uint8_t is_broadcast, const uint8_t command_class, const uint16_t param_id, const uint8_t param_data_length, const uint16_t sub_device)
{
	struct _pid_definition const *pid_handler = NULL;

	if (command_class != E120_GET_COMMAND && command_class != E120_SET_COMMAND)
	{
		rdm_send_respond_message_nack(E120_NR_UNSUPPORTED_COMMAND_CLASS);
		return;
	}

	if (sub_device != 0 && sub_device != 0xffff)
	{
		rdm_send_respond_message_nack(E120_NR_SUB_DEVICE_OUT_OF_RANGE);
		return;
	}

	uint8_t i;
	for (i = 0; i < sizeof(PID_DEFINITIONS) / sizeof(struct _pid_definition);
			++i)
	{
		if (PID_DEFINITIONS[i].pid == param_id)
			pid_handler = &PID_DEFINITIONS[i];
	}

	if (!pid_handler)
	{
		rdm_send_respond_message_nack(E120_NR_UNKNOWN_PID);
		return;
	}

	if (command_class == E120_GET_COMMAND)
	{
		if (!pid_handler->get_handler)
		{
			rdm_send_respond_message_nack(E120_NR_UNSUPPORTED_COMMAND_CLASS);
			return;
		}

		if(is_broadcast)
		{
			return;
		}

		if (sub_device)
		{
			rdm_send_respond_message_nack(E120_NR_SUB_DEVICE_OUT_OF_RANGE);
			return;
		}

		if (param_data_length != pid_handler->get_argument_size)
		{
			rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
			return;
		}

		pid_handler->get_handler();
	} else {
		if (!pid_handler->set_handler)
		{
			rdm_send_respond_message_nack(E120_NR_UNSUPPORTED_COMMAND_CLASS);
			return;
		}

		pid_handler->set_handler(is_broadcast, sub_device);
	}
}

/**
 * @ingroup rdm_handlers
 *
 * Main function for handling RDM data.
 * The function is registered in the poll table \file main.c
 */
void rdm_handle_data(void)
{
	if(rdm_available_get() == FALSE)
		return;

	rdm_available_set(FALSE);

	uint8_t rdm_packet_is_for_me = FALSE;
	uint8_t rdm_packet_is_broadcast = FALSE;
	uint8_t rdm_packet_is_vendorcast = FALSE;

	struct _rdm_command *rdm_cmd = (struct _rdm_command *)rdm_data;

	uint8_t command_class = rdm_cmd->command_class;
	uint16_t param_id = (rdm_cmd->param_id[0] << 8) + rdm_cmd->param_id[1];

	monitor_debug_line(23, "handle_rdm_data, param_id [%.2x%.2x]:%d", rdm_cmd->param_id[0], rdm_cmd->param_id[1], param_id);

	const uint8_t *uid_device = rdm_device_info_get_uuid();

	if (memcmp(rdm_cmd->destination_uid, UID_ALL, RDM_UID_SIZE) == 0)
	{
		rdm_packet_is_broadcast = TRUE;
	}

	if ((memcmp(rdm_cmd->destination_uid, uid_device, 2) == 0) && (memcmp(&rdm_cmd->destination_uid[2], UID_ALL, RDM_UID_SIZE-2) == 0))
	{
		rdm_packet_is_vendorcast = TRUE;
		rdm_packet_is_for_me = TRUE;
	}
	else if (memcmp(rdm_cmd->destination_uid, uid_device, RDM_UID_SIZE) == 0)
	{
		rdm_packet_is_for_me = TRUE;
	}

	if ((rdm_packet_is_for_me == FALSE) && (rdm_packet_is_broadcast == FALSE))
	{
		// Ignore RDM packet
	}
	else if (command_class == E120_DISCOVERY_COMMAND)
	{
		if (param_id == E120_DISC_UNIQUE_BRANCH)
		{
			if (rdm_is_mute == FALSE)
			{
				if ((memcmp(rdm_cmd->param_data, uid_device, RDM_UID_SIZE) <= 0)
						&& (memcmp(uid_device, rdm_cmd->param_data + 6,	RDM_UID_SIZE) <= 0))
				{
					monitor_debug_line(24, "E120_DISC_UNIQUE_BRANCH");

					struct _rdm_discovery_msg *p = (struct _rdm_discovery_msg *) (rdm_data);
					uint16_t rdm_checksum = 6 * 0xFF;

					uint8_t i = 0;
					for (i = 0; i < 7; i++)
					{
						p->header_FE[i] = 0xFE;
					}
					p->header_AA = 0xAA;

					for (i = 0; i < 6; i++)
					{
						p->masked_device_id[i + i] = uid_device[i] | 0xAA;
						p->masked_device_id[i + i + 1] = uid_device[i] | 0x55;
						rdm_checksum += uid_device[i];
					}

					p->checksum[0] = (rdm_checksum >> 8) | 0xAA;
					p->checksum[1] = (rdm_checksum >> 8) | 0x55;
					p->checksum[2] = (rdm_checksum & 0xFF) | 0xAA;
					p->checksum[3] = (rdm_checksum & 0xFF) | 0x55;

					rdm_send_discovery_msg(rdm_data, sizeof(struct _rdm_discovery_msg));
				}
			}
		} else if (param_id == E120_DISC_UN_MUTE)
		{
			if (rdm_cmd->param_data_length != 0) {
				/* The response RESPONSE_TYPE_NACK_REASON shall only be used in conjunction
				 * with the Command Classes GET_COMMAND_RESPONSE & SET_COMMAND_RESPONSE.
				 */
				//rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
				return;
			}
			rdm_is_mute = FALSE;
			rdm_cmd->message_length = RDM_MESSAGE_MINIMUM_SIZE + 2;
			rdm_cmd->param_data_length = 2;
			rdm_cmd->param_data[0] = 0x00;	// Control Field
			rdm_cmd->param_data[1] = 0x00;	// Control Field
			rdm_send_respond_message_ack();
		} else if (param_id == E120_DISC_MUTE)
		{
			if (rdm_cmd->param_data_length != 0) {
				/* The response RESPONSE_TYPE_NACK_REASON shall only be used in conjunction
				 * with the Command Classes GET_COMMAND_RESPONSE & SET_COMMAND_RESPONSE.
				 */
				//rdm_send_respond_message_nack(E120_NR_FORMAT_ERROR);
				return;
			}
			rdm_is_mute = TRUE;
			rdm_cmd->message_length = RDM_MESSAGE_MINIMUM_SIZE + 2;
			rdm_cmd->param_data_length = 2;
			rdm_cmd->param_data[0] = 0x00;	// Control Field
			rdm_cmd->param_data[1] = 0x00;	// Control Field
			rdm_send_respond_message_ack();
		}
	}
	else
	{
		uint16_t sub_device = (rdm_cmd->sub_device[0] << 8) + rdm_cmd->sub_device[1];
		process_rdm_message(rdm_packet_is_broadcast || rdm_packet_is_vendorcast, command_class, param_id, rdm_cmd->param_data_length, sub_device);
	}
}