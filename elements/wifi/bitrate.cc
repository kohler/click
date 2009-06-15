#include <click/config.h>
#include "bitrate.hh"
#include <clicknet/wifi.h>


unsigned
calc_transmit_time(int rate, int length)
{
	unsigned t_plcp_header = 96;
	if (rate == 2) {
		t_plcp_header = 192;
	} else if (!is_b_rate(rate)) {
		t_plcp_header = 20;
  }
	return (2 * (t_plcp_header + ((length * 8))))/ rate;
}

unsigned
calc_backoff(int rate, int t)
{
	int t_slot = is_b_rate(rate) ? WIFI_SLOT_B : WIFI_SLOT_A;
	int cw = WIFI_CW_MIN;

	/* there is backoff, even for the first packet */
	for (int x = 0; x < t; x++) {
		cw = WIFI_MIN(WIFI_CW_MAX, (cw + 1) * 2);
	}
	return t_slot * cw / 2;
}

unsigned
calc_usecs_wifi_packet_tries(int length, int rate, int try0, int tryN)
{
	if (!rate || !length || try0 > tryN) {
		return 99999;
	}

	/* pg 205 ieee.802.11.pdf */
	unsigned t_slot = 20;
	unsigned t_ack = 304; // 192 + 14*8/1
	unsigned t_difs = 50;
	unsigned t_sifs = 10;


	if (!is_b_rate(rate)) {
		/* with 802.11g, things are at 6 mbit/s */
		t_slot = 9;
		t_sifs = 9;
		t_difs = 28;
		t_ack = 30;
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

ELEMENT_PROVIDES(bitrate)
