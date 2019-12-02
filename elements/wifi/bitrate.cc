#include <click/config.h>
#include "bitrate.hh"
#include <clicknet/wifi.h>

// MCS data rates (kbps)
static const uint32_t mcs_rate_lookup[16] =
{
6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000,
13000, 26000, 39000, 52000, 78000, 104000, 117000, 130000
};

unsigned
calc_transmit_time(int rate, int length)
{
	unsigned t_plcp_header = 0;
	if (is_b_rate(rate)) {
		t_plcp_header = (rate == 2) ? WIFI_PLCP_HEADER_LONG_B : WIFI_PLCP_HEADER_SHORT_B;
	} else {
		t_plcp_header = WIFI_PLCP_HEADER_A;
	}
	return (2 * length * 8) / rate + t_plcp_header;
}

unsigned
calc_backoff(int rate, int t)
{
	int t_slot = is_b_rate(rate) ? WIFI_SLOT_B : WIFI_SLOT_A;
	int cw = is_b_rate(rate) ? WIFI_CW_MIN_B : WIFI_CW_MIN;
	int cw_max = is_b_rate(rate) ? WIFI_CW_MAX_B : WIFI_CW_MAX;
	/* there is backoff, even for the first packet */
	for (int x = 0; x < t; x++) {
		cw = WIFI_MIN(cw_max, (cw + 1) * 2);
	}
	return t_slot * cw / 2;
}

unsigned
calc_usecs_wifi_packet_tries(int length, int rate, int try0, int tryN)
{
	if (!rate || !length || try0 > tryN) {
		return 99999;
	}
	unsigned t_ack = WIFI_ACK_A;
	unsigned t_sifs = WIFI_SIFS_A;
	if (is_b_rate(rate)) {
		t_ack = WIFI_ACK_B;
		t_sifs = WIFI_SIFS_B;
	}
	int tt = 0;
	for (int x = try0; x <= tryN; x++) {
		tt += calc_backoff(rate, x) + calc_transmit_time(rate, length) +
			t_sifs + t_ack;
	}
	return tt;
}

unsigned
calc_usecs_wifi_packet(int length, int rate, int retries)
{
	return calc_usecs_wifi_packet_tries(length, rate, 0, retries);
}

unsigned
calc_transmit_time_ht(int rate, int length)
{
	return (1000 * length * 8) / mcs_rate_lookup[rate] + WIFI_PLCP_HEADER_N;
}

unsigned
calc_backoff_ht(int, int t)
{
	int cw = WIFI_CW_MIN;
	/* there is backoff, even for the first packet */
	for (int x = 0; x < t; x++) {
		cw = WIFI_MIN(WIFI_CW_MAX, (cw + 1) * 2);
	}
	return WIFI_SLOT_N * cw / 2;
}

unsigned
calc_usecs_wifi_packet_tries_ht(int length, int rate, int try0, int tryN)
{
	if (rate < 0 || !length || try0 > tryN) {
		return 99999;
	}
	int tt = 0;
	for (int x = try0; x <= tryN; x++) {
		tt += calc_backoff_ht(rate, x) + calc_transmit_time_ht(rate, length) +
			WIFI_SIFS_N + WIFI_ACK_N;
	}
	return tt;
}

unsigned
calc_usecs_wifi_packet_ht(int length, int rate, int retries)
{
	return calc_usecs_wifi_packet_tries_ht(length, rate, 0, retries);
}

ELEMENT_PROVIDES(bitrate)
