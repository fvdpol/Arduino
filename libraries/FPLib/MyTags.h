#ifndef _mytags_h
#define _mytags_h


// float tags
#define TAG_F_WATERBED_ROOM_TEMPERATURE		1
#define TAG_F_WATERBED_BED_TEMPERATURE		2
#define TAG_F_WATERBED_BED_SETPOINT			3
#define TAG_F_WATERBED_BED_OUTPUT			4

#define TAG_F_WATERBED_UPDATE_SETPOINT		6



#define TAG_F_UTILITY_ELECTRICITY_TOTAL		20	/* NOT IN USE -- total E consumption in kWh */
#define TAG_F_UTILITY_ELECTRICITY_RESET		21	/* NOT IN USE -- reset E kWh counter to given value */
#define TAG_F_UTILITY_ELECTRICITY_ACTUAL	22	/* NOT IN USE -- actual E consumption in W */
#define TAG_F_UTILITY_GAS_TOTAL				23	/* NOT IN USE -- total G consumption in M3 */
#define TAG_F_UTILITY_GAS_RESET				24	/* NOT IN USE -- reset G counter to given value */
#define TAG_F_UTILITY_GAS_ACTUAL			25	/* NOT IN USE -- actual G consumption in l/h */




// long tags


#define TAG_L_LIFESIGN_NODE0_METERKAST		100     /* Meterkast gateway MQTT */
#define TAG_L_LIFESIGN_NODE1_GATEWAY		101     /* JeeNode RF12 <-> controller Gateway */
#define TAG_L_LIFESIGN_NODE2				102
#define TAG_L_LIFESIGN_NODE3				103
#define TAG_L_LIFESIGN_NODE4				104	    /* Waterbed controller */
#define TAG_L_LIFESIGN_NODE5				105
#define TAG_L_LIFESIGN_NODE6        		106
#define TAG_L_LIFESIGN_NODE7				107
#define TAG_L_LIFESIGN_NODE8				108
#define TAG_L_LIFESIGN_NODE9				109
#define TAG_L_LIFESIGN_NODE10				110


// long tags


#endif