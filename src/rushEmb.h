/*
 *  BOSCH REXROTH
 *
 *  Copyright (c) Bosch Rexroth AG 2017
 *  Internet: http://www.boschrexroth.com
 *
 *  Product Name:   NYCe4000
 *  Component Name: Examples
 */

/**
 *  @file
 *  @brief  Defines the arguments for starting my_udsx.c.
 */

#ifndef _MY_UDSX_INTERFACE_H_
#define _MY_UDSX_INTERFACE_H_

#include <n4k_basictypes.h>
#include <nycedefs.h>
#include <nhivariables.h>
#include "dyad.h"

/* Helper macros */
#define USR_ERROR(x)    (NYCE_STATUS)(NYCE_ERROR_MASK | (N4K_SS_USR << NYCE_SUBSYS_SHIFT) | (x))
#define USR_WARNING(x)  (NYCE_STATUS)(NYCE_OK_MASK    | (N4K_SS_USR << NYCE_SUBSYS_SHIFT) | (x))


/* My udsx user errors */

/* My UDSX UdsxInitialize detected an invalid argument */
#define MY_UDSX_ERR_INVALID_ARG             USR_ERROR(100)

/* My UDSX cannot run: analog input value too low */
#define MY_UDSX_ERR_AN_IN_LEVEL_TOO_LOW     USR_ERROR(101)

/* My UDSX runs, but the analog input value is low */
#define MY_UDSX_WRN_AN_IN_LEVEL_LOW         USR_WARNING(201)


/**
 * @brief   Argument for UdsxInitialize in my_udsx.c
 *          This is a shared interface between the UDSX and the application which starts the UDSX.
 */
typedef struct my_udsx_args
{
    NHI_VAR_ID          anInputvarId;           /**< Selected analog input, for example NHI_VAR_AN_IN0_VALUE_SLOT0.
                                                 *   If this variable does not define an analog input, starting the UDSX
                                                 *   returns MY_UDSX_ERR_INVALID_ARG.
                                                 */

    double              anInErrorThreshold;     /**< If the analog input is lower than this value,
                                                 *   starting the UDSX returns MY_UDSX_ERR_AN_IN_LEVEL_TOO_LOW.
                                                 */

    double              anInWarningThreshold;   /**< If the analog input is betweeen anInErrorThreshold and this value,
                                                 *   starting the UDSX returns the warning MY_UDSX_WRN_AN_IN_LEVEL_LOW.
                                                 *   Must not be lower than anInErrorThreshold, otherwise starting the UDSX
                                                 *   returns MY_UDSX_ERR_INVALID_ARG.
                                                 */

} MY_UDSX_ARGS;

typedef struct axis_setting{
	int					Shared_AxisType[10];
	char				Shared_AxisName0[20];
	char				Shared_AxisName1[20];
	char				Shared_AxisName2[20];
	char				Shared_AxisName3[20];
	char				Shared_AxisName4[20];
	char				Shared_AxisName5[20];
	char				Shared_AxisName6[20];
	char				Shared_AxisName7[20];
	char				Shared_AxisName8[20];
	char				Shared_AxisName9[20];
}AXIS_SETTING;


#define		SHMEM_AREA		2

typedef struct shmem_data
{
	unsigned int		Shared_StatFlag[10];
	float				Shared_CtrFlag[80];
	double				Shared_SetPointPos[20];
	int					Shared_AxisType[10];
	char				Shared_AxisName0[20];
	char				Shared_AxisName1[20];
	char				Shared_AxisName2[20];
	char				Shared_AxisName3[20];
	char				Shared_AxisName4[20];
	char				Shared_AxisName5[20];
	char				Shared_AxisName6[20];
	char				Shared_AxisName7[20];
	char				Shared_AxisName8[20];
	char				Shared_AxisName9[20];
	unsigned int		STAT_FLG[10];
	float 				VC_POS[20];
	float 				FORCE_LIMIT[10];
	float 				NET_CURRENT[10];
	int					udsx_enter;
	int					udsx_exit;
} SHMEM_DATA;


unsigned int		OLD_STAT_FLG[10];
float				OLD_NET_CURRENT[10];
float				OLD_CMD_FLG[10];

int resp_cmd;
float 				FORCE_LIMIT[10];
///nyce main loop

float CMD_FLG[10];
float CTR_FLG[80];
char  AXS_NAM0[20];
char  AXS_NAM1[20];
char  AXS_NAM2[20];
char  AXS_NAM3[20];
char  AXS_NAM4[20];
char  AXS_NAM5[20];
char  AXS_NAM6[20];
char  AXS_NAM7[20];
char  AXS_NAM8[20];
char  AXS_NAM9[20];
int   AXS_TYPE[10];

char  TempName0[20];
char  TempName1[20];
char  TempName2[20];
char  TempName3[20];
char  TempName4[20];
char  TempName5[20];
char  TempName6[20];
char  TempName7[20];
char  TempName8[20];
char  TempName9[20];
int   TempAxType;

float LAST_VC_POS[20];
unsigned int LAST_STAT_FLG[10];

NYCE_STATUS StatusSDisconnect[10];
NYCE_STATUS StatusSConnect[10];
NYCE_STATUS StatusPtp[10];
NYCE_STATUS StatusOpenLoop[10];
NYCE_STATUS StatusLock[10];
NYCE_STATUS StatusWParameter[10];

#define NyceNoError 0

#define TURRET		0
#define VC_PUSHER	1
#define STD_ABS		2
#define STD_REL		3
#define NA			9

#define ChgWorkPos	2
#define OpenLoop	3
#define AxisLock	4

float oldCmdPos[10];
float oldPtpPos[10];

int Axis_Type[10];
char Axis_Name[10][20];
int SacConnected[10];
int SacMovedCnt[10];
int Cmd_Toggle[10];

double distance[10];
double duration[10];
double ratio[10];


SAC_AXIS		sacAxis[10];
SAC_PTP_PARS	m_sacPtpPars[20];

float SPEED_FACTOR;
float STANDBY_POS[10];

void ExeParabolicProfile(int AxisID);
void EndForceUDSX(void);
void AxisInit(void);


int Init;
int Ready;
int stop_eth;


enum E_CMD{
	E_NO_CMD,
	E_CMD_FLG,
	E_CTR_FLG,
	E_AXS_NAM0,
	E_AXS_NAM1,
	E_AXS_NAM2,
	E_AXS_NAM3,
	E_AXS_NAM4,
	E_AXS_NAM5,
	E_AXS_NAM6,
	E_AXS_NAM7,
	E_AXS_NAM8,
	E_AXS_NAM9,
	E_AXS_TYPE,


	E_FORCE_LIMIT,
	E_NET_CURRENT,
	E_STAT_FLG,
	E_VC_POS,

	E_NYCE_INIT,
	E_NYCE_STOP,

	E_REQ_STAT,
	E_SYS_CASE,

	E_PING = 4114,

};


typedef struct resp_buff
{
	int					status;
	int					sys_case;
	float 				VC_POS[20];
	float 				NET_CURRENT[10];
	float				CMD_FLG[10];
	unsigned int		STAT_FLG[10];
}RESP_BUFF;


enum SEQ_SYS{
	SYS_IDLE,
	SYS_INIT,
	SYS_READY,
	SYS_STOP,
};



FILE *logfile;
char oldlogmsg[180];

void DieWithError(char* errorMessage); /* Error handling function */
void NyceMainLoop(void);
int NyceDisconnectAxis(void);
int waitforUDSX(int active);
int initLogFile(void);
int logging(int axis,float payload,const char* msg,const char* retval);
int memsearch(const char *hay, int haysize, const char *needle, int needlesize);
int closeLogFile(void);
uint32_t GetTimeStamp_ms();

int debug;


#define max_ports 10
#define max_clients 20


int master_socket[max_ports];
int client_socket[max_clients];
int server_port_list[max_clients];
int ports[max_ports];
fd_set readfds;

char nodeAddress[80];

void rushMakeBuffer(char* bufferout, char* bufferin, int* pointer, int size, char flag);
int rushSearchBuffer(unsigned long int* start, int* buffersize, int* size, char *flag, unsigned long int oriStart, int oriSize);
int rushMemsearch(const char *hay, int haysize, const char *needle, int needlesize);
static void onData(dyad_Event *e);
static void onAccept(dyad_Event *e);
static void onError(dyad_Event *e);
static void onReady(dyad_Event *e);
void updateThreadFunc(void);

#endif
