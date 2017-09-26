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
	float 				VC_POS[20];
	float 				FORCE_LIMIT[10];
	float 				NET_CURRENT[10];
} SHMEM_DATA;

struct ThreadArgs {
	int clntSock;
};

void DieWithError(char* errorMessage); /* Error handling function */
void HandleTCPClient(int clntSocketc);   /* TCP client handling function */
void *server(void *arg);
int handleBuffer(void *arg);
void *ExecuteCmd(void *arg);
void initializeBuffer(void);
void NyceMainLoop(void);
void *clientThread(void *arg);
void socketSetUp(int socket);
void prepareStatusBuffer(void *statBuff, int* buffersize);
int NyceDisconnectAxis(void);


struct sockaddr_in echoServAddr; /* Local address */
struct sockaddr_in echoClntAddr; /* Client address */
unsigned short echoServPort;    /* Server port */
unsigned int clntLen;           /* Length of client address data structure */
pthread_t pth,serverpth,execmd;



float F_CMD_FLG_1[10];
float F_CTR_FLG_1[80];
char  F_AXS1_NAM_0[20];
char  F_AXS1_NAM_1[20];
char  F_AXS1_NAM_2[20];
char  F_AXS1_NAM_3[20];
char  F_AXS1_NAM_4[20];
char  F_AXS1_NAM_5[20];
char  F_AXS1_NAM_6[20];
char  F_AXS1_NAM_7[20];
char  F_AXS1_NAM_8[20];
char  F_AXS1_NAM_9[20];
int   F_AXS_TYPE_1[10];

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
float LAST_NET_CURRENT[10];
float LAST_CMD_FLG[10];

#define CMD_FLG     F_CMD_FLG_1
#define CTR_FLG		F_CTR_FLG_1
#define AXS_NAM0	F_AXS1_NAM_0
#define AXS_NAM1	F_AXS1_NAM_1
#define AXS_NAM2	F_AXS1_NAM_2
#define AXS_NAM3	F_AXS1_NAM_3
#define AXS_NAM4	F_AXS1_NAM_4
#define AXS_NAM5	F_AXS1_NAM_5
#define AXS_NAM6	F_AXS1_NAM_6
#define AXS_NAM7	F_AXS1_NAM_7
#define AXS_NAM8	F_AXS1_NAM_8
#define AXS_NAM9	F_AXS1_NAM_9
#define AXS_TYPE	F_AXS_TYPE_1



enum{
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

	E_PING = 4114,

};


#define MAXPENDING 5                   /* Maximum outstanding connection requests */
#define PORT 6666
#define MAX_BUFFER_SIZE 1024



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


enum{
	SYS_IDLE,
	SYS_INIT,
	SYS_READY,
	SYS_STOP,
};
#endif
