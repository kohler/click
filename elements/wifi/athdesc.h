#ifndef _ATHDESC_H_
#define _ATHDESC_H_

#define ATHDESC_HEADER_SIZE 32
struct ar5212_rx_status {
	u_int32_t data_len:12;
	u_int32_t more:1;
	u_int32_t decomperr:2;
	u_int32_t rx_rate:5;
	u_int32_t rx_rssi:8;
	u_int32_t rx_ant:4;


	u_int32_t done:1;
	u_int32_t rx_ok:1;
	u_int32_t crcerr:1;
	u_int32_t decryptcrc:1;

} CLICK_SIZE_PACKED_ATTRIBUTE;

struct ar5212_desc {
	/*
	 * tx_control_0
	 */
	u_int32_t	frame_len:12;
	u_int32_t	reserved_12_15:4;
	u_int32_t	xmit_power:6;
	u_int32_t	rts_cts_enable:1;
	u_int32_t	veol:1;
	u_int32_t	clear_dest_mask:1;
	u_int32_t	ant_mode_xmit:4;
	u_int32_t	inter_req:1;
	u_int32_t	encrypt_key_valid:1;
	u_int32_t	cts_enable:1;

	/*
	 * tx_control_1
	 */
	u_int32_t	buf_len:12;
	u_int32_t	more:1;
	u_int32_t	encrypt_key_index:7;
	u_int32_t	frame_type:4;
	u_int32_t	no_ack:1;
	u_int32_t	comp_proc:2;
	u_int32_t	comp_iv_len:2;
	u_int32_t	comp_icv_len:2;
	u_int32_t	reserved_31:1;

	/*
	 * tx_control_2
	 */
	u_int32_t	rts_duration:15;
	u_int32_t	duration_update_enable:1;
	u_int32_t	xmit_tries0:4;
	u_int32_t	xmit_tries1:4;
	u_int32_t	xmit_tries2:4;
	u_int32_t	xmit_tries3:4;

	/*
	 * tx_control_3
	 */
	u_int32_t	xmit_rate0:5;
	u_int32_t	xmit_rate1:5;
	u_int32_t	xmit_rate2:5;
	u_int32_t	xmit_rate3:5;
	u_int32_t	rts_cts_rate:5;
	u_int32_t	reserved_25_31:7;

	/*
	 * tx_status_0
	 */
	u_int32_t	frame_xmit_ok:1;
	u_int32_t	excessive_retries:1;
	u_int32_t	fifo_underrun:1;
	u_int32_t	filtered:1;
	u_int32_t	rts_fail_count:4;
	u_int32_t	data_fail_count:4;
	u_int32_t	virt_coll_count:4;
	u_int32_t	send_timestamp:16;

	/*
	 * tx_status_1
	 */
	u_int32_t	done:1;
	u_int32_t	seq_num:12;
	u_int32_t	ack_sig_strength:8;
	u_int32_t	final_ts_index:2;
	u_int32_t	comp_success:1;
	u_int32_t	xmit_antenna:1;
	u_int32_t	reserved_25_31_x:7;
} CLICK_SIZE_PACKED_ATTRIBUTE;







inline int 
ratecode_to_dot11(int ratecode) {
	switch (ratecode) {
		/* a */
	case 11: return 12;  
	case 15: return 18;  
	case 10: return 24;  
	case 14: return 36;  
	case 9: return 48;  
	case 13: return 72;  
	case 8: return 96;  
	case 12: return 108; 
		
	case 0x1b: return 2;   
	case 0x1a: return 4;   
	case 0x1e: return 4;   
	case 0x19: return 11;  
	case 0x1d: return 11;  
	case 0x18: return 22;  
	case 0x1c: return 22;  
	}
	return 0;
}

inline int 
dot11_to_ratecode(int dot11) {
	switch (dot11) {
	  case 12:  return 11; 
	  case 18:  return 15; 
	  case 24:  return 10; 
	  case 36:  return 14; 
	  case 48:  return 9; 
	  case 72:  return 13; 
	  case 96:  return 8; 
	  case 108: return 12;

	  case 2:   return 0x1b; 
	  case 4:   return 0x1e; 
	  case 11:  return 0x1d; 
	  case 22:  return 0x1c; 
	}
	return 0;
}

#endif /* _ATHDESC_H_ */

