#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>



#include "mavlink_bridge_header.h"
#include "mavlink.h"
#include "settings.h"
#include "scheduler.h"
#include "udp_driver.h"


mavlink_system_t mavlink_system;

static uint32_t m_parameter_i = 0;

void communication_init(void)
{
	udp_init();

	mavlink_system.sysid = global_data.param[PARAM_SYSTEM_ID];
	mavlink_system.compid = global_data.param[PARAM_COMPONENT_ID];
}

void communication_system_state_send(void)
{
	mavlink_msg_heartbeat_send(MAVLINK_COMM_0, global_data.param[PARAM_SYSTEM_TYPE], global_data.param[PARAM_AUTOPILOT_TYPE], 0, 0, 0);
}

void communication_parameter_send(void)
{
	if (m_parameter_i < ONBOARD_PARAM_COUNT)
	{
		mavlink_msg_param_value_send(MAVLINK_COMM_0,
						global_data.param_name[m_parameter_i],
						global_data.param[m_parameter_i], MAVLINK_TYPE_FLOAT, ONBOARD_PARAM_COUNT, m_parameter_i);
		m_parameter_i++;
	}
}

void handle_mavlink_message(mavlink_channel_t chan, mavlink_message_t *msg)
{
	switch (msg->msgid)
	{
		case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
		{
			mavlink_param_request_read_t set;
			mavlink_msg_param_request_read_decode(msg, &set);

			if ((uint8_t) set.target_system == (uint8_t) global_data.param[PARAM_SYSTEM_ID]
					&& (uint8_t) set.target_component == (uint8_t) global_data.param[PARAM_COMPONENT_ID])
			{

			}
		}
		break;

		case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
		{
			m_parameter_i = 0;
		}
		break;

		case MAVLINK_MSG_ID_PARAM_SET:
		{
			mavlink_param_set_t set;
			mavlink_msg_param_set_decode(msg, &set);

			if ((uint8_t) set.target_system == (uint8_t) global_data.param[PARAM_SYSTEM_ID]
					&& (uint8_t) set.target_component == (uint8_t) global_data.param[PARAM_COMPONENT_ID])
			{
				char *key = (char *) set.param_id;
				for (int i = 0; i < ONBOARD_PARAM_COUNT; i++)
				{
					bool match = true;
					for (int j = 0; j < ONBOARD_PARAM_NAME_LENGTH; j++)
					{
						if ((char) (global_data.param_name[i][j])
							!= (char) (key[j]))
						{
							match = false;
						}
						if ((char) global_data.param_name[i][j] == '\0')
						{
							break;
						}
					}
					if (match)
					{
						if (global_data.param[i] != set.param_value
								&& !isnan(set.param_value)
								&& !isinf(set.param_value)
								&& global_data.param_access[i])
						{
							global_data.param[i] = set.param_value;

						}
						else
						{
							mavlink_msg_param_value_send(MAVLINK_COMM_0,
									global_data.param_name[i],
									global_data.param[i], MAVLINK_TYPE_FLOAT, ONBOARD_PARAM_COUNT, m_parameter_i);

						}
					}
				}
			}

		}
		break;

		case MAVLINK_MSG_ID_PING:
		{
			mavlink_ping_t ping;
			mavlink_msg_ping_decode(msg, &ping);
			if (ping.target_system == 0 && ping.target_component == 0)
			{
				unsigned long r_timestamp;
				get_us((uint32_t *)&r_timestamp);
				mavlink_msg_ping_send(chan, ping.seq, msg->sysid, msg->compid, r_timestamp);
			}
		}
		break;

		case MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE:
		{
			mavlink_rc_channels_override_t rc_channels_override;
			mavlink_msg_rc_channels_override_decode(msg, &rc_channels_override);
			if ((uint8_t) rc_channels_override.target_system == (uint8_t) global_data.param[PARAM_SYSTEM_ID]
					&& (uint8_t) rc_channels_override.target_component == (uint8_t) global_data.param[PARAM_COMPONENT_ID])
			{
				fprintf(stdout, "\n");
				fprintf(stdout, "RC channel 1: %d\n", rc_channels_override.chan1_raw);
				fprintf(stdout, "RC channel 2: %d\n", rc_channels_override.chan2_raw);
				fprintf(stdout, "RC channel 3: %d\n", rc_channels_override.chan3_raw);
				fprintf(stdout, "RC channel 4: %d\n", rc_channels_override.chan4_raw);

			}
		}
		break;

		default:
			break;

	}
}

void communication_receive(void)
{
	mavlink_message_t msg;
	mavlink_status_t status = {0};
	int i = 0;
	int rec_size = 0;
	uint8_t buf_receive[500];
	memset(buf_receive, 0, 500);
	rec_size = udp_receive(buf_receive, 500);

	if (rec_size < 0)
	{
		return ;
	}
	else
	{
		for (i = 0; i < rec_size; i++)
		{
			if (mavlink_parse_char(MAVLINK_COMM_0, buf_receive[i], &msg, &status))
			{
				fprintf(stdout, "\n");
				fprintf(stdout, "received packed,sys: %d, comp: %d, LEN : %d, MSG ID: %d\n", msg.sysid, msg.compid, msg.len, msg.msgid);
				handle_mavlink_message(MAVLINK_COMM_0, &msg);
			}
		}
	}

}

void mavlink_send_uart_bytes(mavlink_channel_t chan, uint8_t *ch, uint16_t length)
{
	int send_size = 0;
	if (chan == MAVLINK_COMM_0)
	{
		send_size = udp_send(ch, length);
		if (send_size < 0)
		{
			fprintf(stderr, "sending packet err: %s\n", strerror(errno));
		}
	}
}
