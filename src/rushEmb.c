/*
 *  Exis Tech
 *
 *	Company name : Exis Tech Sdn. bhd.
 *
 *	Author : Muhammad Rushdi bin Mohd Rasid
 *  Product Name:   NYCe4000
 *  Component Name: rushNodeSeq and rushNodeSeq.so
 */

/**
 *  @file
 *  @brief  establish comm server, manage and execute commands at the NYCE4114 and return status
 *
 *  To compile the embedded application
 *  compile with:
 *      arm-rexroth-linux-gnueabihf-gcc -DBIN -c rushSeq.c -o rushSeq.bin.o
 *  link with:
 *      arm-rexroth-linux-gnueabihf-gcc -o rushSeq rushSeq.bin.o -lnyce -lsys -lsac -lnhi -lrt -lpthread
 *
 *  All output files should be put at /home/user/ at the nyce sequencer
 *  please put .SO at the output file if compiling the udsx
 *  .SO is a position independent code -fPIC, shared library -shared
 */

#define BIN
#define DEBUG 0

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <sactypes.h>
#include "sysapi.h"
#include "udsxapi.h"
#include <time.h>
#include <errno.h>

/////////tcp communication

#include <sys/socket.h>                /* for socket(), bind(), connect(), recv() and send() */
#include <arpa/inet.h>                 /* for sockaddr_inand inet_ntoa() */
#include <pthread.h>
#include <netinet/tcp.h>

/* Include MY_UDSX_ARGS type and USR error codes */
#include "rushEmb.h"
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>



/*
 * Name of the shared memory file which is located in /dev/shm.
 */
#define SHM_NAME "mycounter"

#define UNUSED(x) (void)(x)

/*
 * User defined errors.
 */
#define USR_ERR_FAILED_TO_CREATE_SHM        ((NYCE_STATUS)((NYCE_ERROR_MASK)|((N4K_SS_USR<<NYCE_SUBSYS_SHIFT)|0)))
#define USR_ERR_FAILED_TO_RESIZE_SHM        ((NYCE_STATUS)((NYCE_ERROR_MASK)|((N4K_SS_USR<<NYCE_SUBSYS_SHIFT)|1)))
#define USR_ERR_FAILED_TO_MAP_SHM           ((NYCE_STATUS)((NYCE_ERROR_MASK)|((N4K_SS_USR<<NYCE_SUBSYS_SHIFT)|2)))
#define USR_ERR_NOT_INITIALIZED             ((NYCE_STATUS)((NYCE_ERROR_MASK)|((N4K_SS_USR<<NYCE_SUBSYS_SHIFT)|3)))

/*
 * Administration of the shared memory.
 */
int         g_sharedMemoryDescriptor = -1;  // Descriptor to the file.
SHMEM_DATA* pShmem_data = NULL;              // Pointer to the shared memory data.
BOOL        g_created = FALSE;              // Flag to remember whether or not the shared memory was created by this process.

/**
 *  @brief  Initialize the shared memory.
 *
 *  @param[in]  create       Whether or not the application may create the shared memory file if it does not exist.
 *  @return     Status of the success of creating the shared memory.
 */
static NYCE_STATUS Initialize(BOOL create);

/**
 *  @brief  Terminate the shared memory.
 */
static void Terminate(void);


//nyce node
unsigned int    nodeId,sysnodeId;

//mutex
pthread_mutex_t lock;

volatile sig_atomic_t g_stop;

char sys_case;
char logstr[80];
pthread_t updateThread;

/**
 *  @brief  Interrupt signal handler for catching Ctrl-C
 *
 *  The application is stopped when the signal is received.
 *
 *  @param[in]  signum  number of the signal that occurred.
 */
void IntHandler(int signum)
{
    UNUSED(signum);
    g_stop = 1;
}

void updateThreadFunc(void)
{
	static int i;
	while(g_stop == 0)
	{
		for(i = 0 ; i < 10;i++)
		{
			if (dyad_getStreamCount() > 0) {
			dyad_update();
			}
		}
		//usleep(10);
	}

}


int main(void)
{


    NYCE_STATUS retVal;

	initLogFile();

	 //initialize dyad
	dyad_Stream *s;
	dyad_init();

	s = dyad_newStream();
	dyad_addListener(s, DYAD_EVENT_ERROR, onError, NULL);
	dyad_addListener(s, DYAD_EVENT_ACCEPT, onAccept, NULL);
	dyad_listen(s, 6666);
	//dyad_setUpdateTimeout(0);
	pthread_create(&updateThread,0,updateThreadFunc,0);
	logging(100,0,"Start ETH server","success");  ////////////////log


	 //init buffer mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    logging(100,(float)0,"Init mutex","success"); ///// log

    //connect to NYCE
    retVal = NyceInit(NYCE_ETH);
    if (NyceError(retVal))
    {
    	printf("NyceInit Error %s\n", NyceGetStatusString(retVal));
    	return 0;
    }
    logging(100,0,"Init NyceInit",NyceGetStatusString(retVal)); ///// log



	while((!NyceSuccess(SysSynchronize(SYS_REQ_NETWORK_OPERATIONAL,0,10))))
	{
		puts("waiting for SysSynchronize");
	}
    puts("SysSynchronize done");
    logging(100,0,"Wait for sys synchrounous",NyceGetStatusString(retVal)); ///// log


    retVal = NhiConnect("localhost", &nodeId);
    if (NyceError(retVal))
    {
       printf("NhiConnect Error %s\n", NyceGetStatusString(retVal));
       return 0;
    }
    logging(100,0,"Nhi connect",NyceGetStatusString(retVal));  ////////////////log


    //End previous udsx
    EndForceUDSX();

	//signal handler
    (void)signal(SIGINT, IntHandler);

    printf("Press Ctrl-C to stop\n");
    while (!g_stop)
    {

		switch(sys_case)
		{
			case SYS_IDLE:
				break;
			case SYS_INIT:
				puts("init");
				logging(123,sys_case,"Initialising","status"); ///// log
				//init all axis
				AxisInit();
				CTR_FLG[19] = 255;	//Terminate sequence flag
				sys_case = SYS_READY;
				logging(123,sys_case,"system ready","status"); ///// log
				break;
			case SYS_READY:
				if(CTR_FLG[19] != 255)
				{
					sys_case = SYS_STOP;
				}
				break;
			case SYS_STOP:
				logging(123,sys_case,"system stop","status"); ///// log
				puts("stop");
				CTR_FLG[19] = 0;
				NyceDisconnectAxis();
				EndForceUDSX();
				sys_case = SYS_IDLE;
				break;

		}

		usleep(1000);
    }


      NyceDisconnectAxis();
      logging(100,1,"disconnect axis","success");  ////////////////log


      EndForceUDSX();

      retVal = NhiDisconnect(nodeId);
      if ( NyceError(retVal) )
      {

           printf("NhiDisconnect Error %s\n", NyceGetStatusString(retVal));
      }
      logging(100,1,"Nhi disconnected",NyceGetStatusString(retVal));  ////////////////log


      retVal = NyceTerm();
      if (NyceError(retVal))
      {
    	  printf("NyceTerm Error %s\n", NyceGetStatusString(retVal));
      }
      logging(100,1,"nyce terminated",NyceGetStatusString(retVal));  ////////////////log

      //disable mutex
      pthread_mutex_destroy(&lock);


      ////shutdown DYAD
      dyad_shutdown();
      logging(100,1,"stopping ETH","success");  ////////////////log


      ///clost log
      closeLogFile();

      return 0;
}

void AxisInit(void)
{
	int ax,retVal;
	AXIS_SETTING axisSetting;

	puts("NyceInit");

	//-------------------------
	//		Axis Naming
	//-------------------------
    for ( ax = 0; ax < 10; ax++ )
    {
		strcpy(Axis_Name[ax],"NA");
	}

	strcpy(TempName0,AXS_NAM0);
	strcpy(axisSetting.Shared_AxisName0,TempName0);
	strcpy(Axis_Name[0],axisSetting.Shared_AxisName0);

	strcpy(TempName1,AXS_NAM1);
	strcpy(axisSetting.Shared_AxisName1,TempName1);
	strcpy(Axis_Name[1],axisSetting.Shared_AxisName1);

	strcpy(TempName2,AXS_NAM2);
	strcpy(axisSetting.Shared_AxisName2,TempName2);
	strcpy(Axis_Name[2],axisSetting.Shared_AxisName2);

	strcpy(TempName3,AXS_NAM3);
	strcpy(axisSetting.Shared_AxisName3,TempName3);
	strcpy(Axis_Name[3],axisSetting.Shared_AxisName3);

	strcpy(TempName4,AXS_NAM4);
	strcpy(axisSetting.Shared_AxisName4,TempName4);
	strcpy(Axis_Name[4],axisSetting.Shared_AxisName4);

	strcpy(TempName5,AXS_NAM5);
	strcpy(axisSetting.Shared_AxisName5,TempName5);
	strcpy(Axis_Name[5],axisSetting.Shared_AxisName5);

	strcpy(TempName6,AXS_NAM6);
	strcpy(axisSetting.Shared_AxisName6,TempName6);
	strcpy(Axis_Name[6],axisSetting.Shared_AxisName6);

	strcpy(TempName7,AXS_NAM7);
	strcpy(axisSetting.Shared_AxisName7,TempName7);
	strcpy(Axis_Name[7],axisSetting.Shared_AxisName7);

	strcpy(TempName8,AXS_NAM8);
	strcpy(axisSetting.Shared_AxisName8,TempName8);
	strcpy(Axis_Name[8],axisSetting.Shared_AxisName8);

	strcpy(TempName9,AXS_NAM9);
	strcpy(axisSetting.Shared_AxisName9,TempName9);
	strcpy(Axis_Name[9],axisSetting.Shared_AxisName9);

// 	-------------------------
// 			Axis Type
// 	-------------------------
    for ( ax = 0; ax < 10; ax++ )
    {
		Axis_Type[ax] = NA;
	}

 	for (ax = 0 ; ax < 10 ; ax++)
 	{
		TempAxType = AXS_TYPE[ax];
		Axis_Type[ax] = TempAxType;
		axisSetting.Shared_AxisType[ax] = Axis_Type[ax];
 	}

	//FORCE_THRESHOLD = 3000;
	//DP_THRESHOLD = 150;
	//LINEAR_THRESHOLD = 800;



 	 if (NyceSuccess(NhiUdsxStart(nodeId, "/home/user/librushUDSX.so" , &axisSetting, (uint32_t)sizeof(axisSetting))))
 	 {
 		 	 	 	printf("UDSX started successfully.\n");
 		 	 	 	logging(144,1,"UDSX started","/home/user/librushUDSX.so");  ////////////////log
 	 }



 	//Initialize the shared memory.
 	retVal = Initialize(FALSE);
 	if ( NyceError(retVal) )
 	   {
 		printf("Initialize Error %s\n", NyceGetStatusString(retVal));
 		logging(144,1,"initialise shared memory failed",NyceGetStatusString(retVal));  ////////////////log
 	   }
 	else
 	{
 		logging(144,1,"shared memory started",NyceGetStatusString(retVal));  ////////////////log
 	 	 if(pShmem_data){
	 	 	printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName0);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[0]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName1);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[1]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName2);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[2]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName3);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[3]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName4);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[4]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName5);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[5]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName6);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[6]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName7);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[7]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName8);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[8]);
			printf("axis name %s ",(char*)&pShmem_data->Shared_AxisName9);
			printf("axis type %d \n",pShmem_data->Shared_AxisType[9]);
 	 	 }
 	}



    for ( ax = 0; ax < 10; ax++ )
    {
		if (Axis_Type[ax] != NA)
		{
			StatusSConnect[ax] = SacConnect(Axis_Name[ax], &sacAxis[ax]);

			if (StatusSConnect[ax] == 0)
			{
				SacConnected[ax] = 255;
				logging(ax,(float)SacConnected[ax],"From 144 : SacConnect","sac Connected");  ////////////////log
			}
		}
	}

    if (pShmem_data)
    {

		for ( ax = 0; ax < 10; ax++ )
		{
			SacMovedCnt[ax] = 0;
			CMD_FLG[ax] = Cmd_Toggle[ax] = 0;
			pShmem_data->Shared_StatFlag[ax] = 0x01;
			pShmem_data->Shared_CtrFlag[ax] = CTR_FLG[ax] = 0;
			pShmem_data->Shared_CtrFlag[ax + 10] = CTR_FLG[ax + 10] = 0;

			if (Axis_Type[ax] == TURRET)
			{
				pShmem_data->Shared_CtrFlag[ax + 20] = CTR_FLG[ax + 20] = 10;
				pShmem_data->Shared_CtrFlag[ax + 30] = CTR_FLG[ax + 30] = 1;
			}
			else
			{
				pShmem_data->Shared_CtrFlag[ax + 20] = CTR_FLG[ax + 20] = 1000;
				pShmem_data->Shared_CtrFlag[ax + 30] = CTR_FLG[ax + 30] = 1;
			}

		}
    }

	CTR_FLG[10] = 4.5;	//speed factor
	CTR_FLG[11] = 150;	//DP sensitivity
	CTR_FLG[12] = 800;	//Linear up threshold
	CTR_FLG[13] = 1000;	//Rest position
	CTR_FLG[14] = 3000;	//Force control threshold
	CTR_FLG[15] = 10;	//ScanForceRate
	CTR_FLG[16] = 0;	//Tweak table bypass
	CTR_FLG[17] = 0;	//VC Open loop ramp
	CTR_FLG[18] = 0;	//VC Open loop value
	//CTR_FLG[19] = 255;	//Terminate sequence flag

	CTR_FLG[40] = 250;	//VC soft landing default distance[AxisID]
	CTR_FLG[41] = 0.005;//VC soft landing default duration


}

void NyceMainLoop(void)
{

	int ax;
    int CmdType;

	if(CTR_FLG[19] == 255)
	{

		SPEED_FACTOR = CTR_FLG[10];

		for ( ax = 0; ax < 10; ax++)
		{
			STANDBY_POS[ax] = CTR_FLG[ax+60];
		}

		for ( ax = 0; ax < 10; ax++)
		{
			if (SacConnected[ax] == 255)
			{


				if (CMD_FLG[ax] != 0)
				{
					logging(ax,CMD_FLG[ax],"CMD_FLG"," NyceMainLoop");  ////////////////log
					oldCmdPos[ax] = CMD_FLG[ax];
					switch(Axis_Type[ax])
					{
						case TURRET:
							if (CTR_FLG[50 + ax] == 0)
							{
								m_sacPtpPars[ax].positionReference = SAC_RELATIVE;
							}
							else
							{
								m_sacPtpPars[ax].positionReference = SAC_ABSOLUTE;
							}

							CmdType = CMD_FLG[ax]/10000;
							switch(CmdType)
							{
								default:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"default","turret");  ////////////////log
									ExeParabolicProfile(ax);
									break;

								case OpenLoop:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"open loop","turret");  ////////////////log
									StatusOpenLoop[ax] = SacOpenLoop(sacAxis[ax]);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
						 			break;

								case AxisLock:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"open loop","turret");  ////////////////log
									StatusLock[ax] = SacLock(sacAxis[ax]);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;
							}
							break;

						case VC_PUSHER:
							m_sacPtpPars[ax].positionReference = SAC_ABSOLUTE;
							CmdType = CMD_FLG[ax]/10000;
			 				switch(CmdType)
							{
								default:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"default","pusher");  ////////////////log
									ExeParabolicProfile(ax);
									break;

								case ChgWorkPos:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"change work position","pusher");  ////////////////log
									CMD_FLG[ax] = CMD_FLG[ax] - ChgWorkPos*10000;
									CTR_FLG[ax] = CMD_FLG[ax];

			 						ExeParabolicProfile(ax);
									break;

								case OpenLoop:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"open loop","pusher");  ////////////////log
									StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_RAMP,CTR_FLG[17]);
                                    StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_VALUE,CTR_FLG[18]);
                                    StatusOpenLoop[ax] = SacOpenLoop(sacAxis[ax]);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;

								case AxisLock:
									puts("axis lock");
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"lock","pusher");  ////////////////log
									StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_RAMP,CTR_FLG[17]);
                                    StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_VALUE,0);
                                    StatusLock[ax] = SacLock(sacAxis[ax]);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;
							}

							break;

						case STD_ABS:
							logging(ax,pShmem_data->Shared_StatFlag[ax],"STD_ABS","STD");  ////////////////log
							m_sacPtpPars[ax] .positionReference = SAC_ABSOLUTE;
							ExeParabolicProfile(ax);
							break;

						case STD_REL:
							logging(ax,pShmem_data->Shared_StatFlag[ax],"STD_REL","STD");  ////////////////log
							m_sacPtpPars[ax] .positionReference = SAC_RELATIVE;
							ExeParabolicProfile(ax);
							break;
					}

					if (Cmd_Toggle[ax] == 0)
					{
						Cmd_Toggle[ax] = 1;
					}
					else
					{
						Cmd_Toggle[ax] = 0;
					}

					pShmem_data->Shared_StatFlag[ax] |= Cmd_Toggle[ax]*0x80;

					oldPtpPos[ax] = CMD_FLG[ax];
					CMD_FLG[ax] = 0;

					SacMovedCnt[ax]++;
				}
			}

			if(pShmem_data){
				pShmem_data->Shared_CtrFlag[ax] = CTR_FLG[ax];
				pShmem_data->Shared_CtrFlag[ax + 10] = CTR_FLG[ax + 10];
				pShmem_data->Shared_CtrFlag[ax + 50] = CTR_FLG[ax + 50];
				pShmem_data->Shared_CtrFlag[ax + 60] = CTR_FLG[ax + 60];
			}
		}

		//waitforUDSX(1);//(resp_cmd);
	}
}

int NyceDisconnectAxis(void)
{
	int ax;
	   for ( ax = 0; ax < 10; ax++ )
	    {
			if (SacConnected[ax] == 255)
			{
				StatusSDisconnect[ax] = SacDisconnect(sacAxis[ax]);
				logging(ax,(float)StatusSDisconnect[ax],"SacDisconnect","nyce disconnect axis");  ////////////////log
			}
	    }
	return 0;
}


void ExeMinJerkProfile(int AxisID)
{
	if (m_sacPtpPars[AxisID].positionReference == SAC_ABSOLUTE)
	{
		distance[AxisID] = CMD_FLG[AxisID] - pShmem_data->Shared_SetPointPos[AxisID];
	}
	else
	{
		distance[AxisID] = CMD_FLG[AxisID];
	}

	ratio[AxisID] = abs(distance[AxisID])/CTR_FLG[20 + AxisID];			//CMD_FLG[20 + ax] defines the default distance of an axis
	duration[AxisID] = CTR_FLG[30 + AxisID] * ratio[AxisID];			//CMD_FLG[30 + ax] defines the default duration of an axis

	if (duration[AxisID] < CTR_FLG[30 + AxisID])
	{
		duration[AxisID] = CTR_FLG[30 + AxisID];
	}

	m_sacPtpPars[AxisID].velocity		=  2 * abs(distance[AxisID]) / duration[AxisID];
	m_sacPtpPars[AxisID].acceleration	=  4 * m_sacPtpPars[AxisID].velocity / duration[AxisID];
	m_sacPtpPars[AxisID].jerk			=  4 * m_sacPtpPars[AxisID].acceleration / duration[AxisID];
	m_sacPtpPars[AxisID].position		=  (double)CMD_FLG[AxisID];

	if (m_sacPtpPars[AxisID].velocity == 0)
	{
		m_sacPtpPars[AxisID].velocity = 10;
	}

	if (m_sacPtpPars[AxisID].acceleration == 0)
	{
		m_sacPtpPars[AxisID].acceleration = 10;
	}

	if (m_sacPtpPars[AxisID].jerk == 0)
	{
		m_sacPtpPars[AxisID].jerk = 10;
	}

	StatusPtp[AxisID] = SacPointToPoint(sacAxis[AxisID],&m_sacPtpPars[AxisID]);

	pShmem_data->Shared_StatFlag[AxisID] = 0;

	if (fabs(distance[AxisID]) <= 1)
	{
		pShmem_data->Shared_StatFlag[AxisID] |= 0x01;
	}
}

void ExeEnergyOptimum3rdOrderProfile(int AxisID)
{
	if (m_sacPtpPars[AxisID].positionReference == SAC_ABSOLUTE)
	{
		distance[AxisID] = CMD_FLG[AxisID] - pShmem_data->Shared_SetPointPos[AxisID];
	}
	else
	{
		distance[AxisID] = CMD_FLG[AxisID];
	}

	ratio[AxisID] = abs(distance[AxisID])/CTR_FLG[20 + AxisID];			//CMD_FLG[20 + ax] defines the default distance of an axis
	duration[AxisID] = CTR_FLG[30 + AxisID] * ratio[AxisID];			//CMD_FLG[30 + ax] defines the default duration of an axis

	if (duration[AxisID] < CTR_FLG[30 + AxisID])
	{
		duration[AxisID] = CTR_FLG[30 + AxisID];
	}

	m_sacPtpPars[AxisID].velocity		= 1.5 * abs(distance[AxisID]) / duration[AxisID];
	m_sacPtpPars[AxisID].acceleration	= 4.5 * m_sacPtpPars[AxisID].velocity / duration[AxisID];
	m_sacPtpPars[AxisID].jerk			= 9   * m_sacPtpPars[AxisID].acceleration / duration[AxisID];
	m_sacPtpPars[AxisID].position		=  (double)CMD_FLG[AxisID];

	if (m_sacPtpPars[AxisID].velocity == 0)
	{
		m_sacPtpPars[AxisID].velocity = 10;
	}

	if (m_sacPtpPars[AxisID].acceleration == 0)
	{
		m_sacPtpPars[AxisID].acceleration = 10;
	}

	if (m_sacPtpPars[AxisID].jerk == 0)
	{
		m_sacPtpPars[AxisID].jerk = 10;
	}

	StatusPtp[AxisID] = SacPointToPoint(sacAxis[AxisID],&m_sacPtpPars[AxisID]);

	pShmem_data->Shared_StatFlag[AxisID] = 0;

	if (fabs(distance[AxisID]) <= 1)
	{
		pShmem_data->Shared_StatFlag[AxisID] |= 0x01;
	}
}

void ExeParabolicProfile(int AxisID)
{
	if (m_sacPtpPars[AxisID].positionReference == SAC_ABSOLUTE)
	{
		distance[AxisID] = CMD_FLG[AxisID] - pShmem_data->Shared_SetPointPos[AxisID];
	}
	else
	{
		distance[AxisID] = CMD_FLG[AxisID];
	}

	ratio[AxisID] = abs(distance[AxisID])/CTR_FLG[20 + AxisID];			//CMD_FLG[20 + ax] defines the default distance of an axis
	duration[AxisID] = CTR_FLG[30 + AxisID] * ratio[AxisID];			//CMD_FLG[30 + ax] defines the default duration of an axis

	if (duration[AxisID] < CTR_FLG[30 + AxisID])
	{
		duration[AxisID] = CTR_FLG[30 + AxisID];
	}

	m_sacPtpPars[AxisID].velocity		= abs(distance[AxisID]) / (duration[AxisID]/2);
	m_sacPtpPars[AxisID].acceleration	= m_sacPtpPars[AxisID].velocity / (duration[AxisID]/2);
	m_sacPtpPars[AxisID].jerk			= -1;												//0 => no value , = infinity
	m_sacPtpPars[AxisID].position		= (double)CMD_FLG[AxisID];

	if (m_sacPtpPars[AxisID].velocity == 0)
	{
		m_sacPtpPars[AxisID].velocity = 10;
	}

	if (m_sacPtpPars[AxisID].acceleration == 0)
	{
		m_sacPtpPars[AxisID].acceleration = 10;
	}

	StatusPtp[AxisID] = SacPointToPoint(sacAxis[AxisID],&m_sacPtpPars[AxisID]);

	pShmem_data->Shared_StatFlag[AxisID] = 0;

	if (fabs(distance[AxisID]) <= 1)
	{
		pShmem_data->Shared_StatFlag[AxisID] |= 0x01;
	}
}

void EndForceUDSX(void)
{
	NYCE_STATUS return_stat;
	Terminate();

    return_stat = NhiUdsxStop(nodeId);
    logging(1,(float)return_stat,"NhiUdsxStop","EndForceUDSX");  ////////////////log

}

static NYCE_STATUS Initialize(BOOL create)
{
    NYCE_STATUS retVal = NYCE_OK;

    /*
     * Create shared memory object.
     */
    g_sharedMemoryDescriptor = shm_open(SHM_NAME, O_RDWR | (create ? O_CREAT : 0), S_IRUSR | S_IWUSR);
    if (g_sharedMemoryDescriptor != -1)
    {
        g_created = create;

        /*
         *  Set its size to the size of the data.
         */
        if (!create ||
            (ftruncate(g_sharedMemoryDescriptor, sizeof(*pShmem_data)) == 0))
        {
            /*
             * Map shared memory object.
             */
        	pShmem_data = mmap(NULL, sizeof(*pShmem_data),
                              PROT_READ | PROT_WRITE, MAP_SHARED,
                              g_sharedMemoryDescriptor, 0);
            if (pShmem_data == MAP_FAILED)
            {
                retVal = USR_ERR_FAILED_TO_MAP_SHM;
                pShmem_data = NULL;
            }
            else
            {
                if (create)
                {
                    memset(pShmem_data, 0, sizeof(*pShmem_data));
                }
            }
        }
        else
        {
            retVal = USR_ERR_FAILED_TO_RESIZE_SHM;
        }

        if (NyceError(retVal))
        {
            /*
             * If an error occurred we must close the descriptor again.
             */
            Terminate();
        }
    }
    else
    {
        retVal = USR_ERR_FAILED_TO_CREATE_SHM;
    }

    return retVal;
}

static void Terminate(void)
{
    if (pShmem_data)
    {
        /*
         * If the counter is mapped we must unmap it.
         */
        (void)munmap(pShmem_data, sizeof(*pShmem_data));
        pShmem_data = NULL;
    }

    if (g_sharedMemoryDescriptor != -1)
    {
        /*
         * When the descriptor is still open we need to close it.
         */
        (void)close(g_sharedMemoryDescriptor);
        g_sharedMemoryDescriptor = -1;

        if (g_created)
        {
            /*
             * Finally, the file that was created must be removed.
             */
            (void)shm_unlink(SHM_NAME);
        }
    }
}


void DieWithError(char* errorMessage)
{
	printf("%s \n",errorMessage);
	logging(100,100,errorMessage,"DieWithError");
}


int initLogFile(void)
{
	debug = DEBUG;
	char filename[50];
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	sprintf(filename,"/home/user/logging_%d_%d_%d_%d_%d.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
	if(debug)
	{
		logfile = fopen(filename, "a+");
		if(logfile == NULL)
		{
			printf("Log open problem \n");
			return -1;
		}
	}
	return 1;
}


int logging(int axis,float payload,const char* msg, const char* retval)
{

char logmsg[180];
time_t t = time(NULL);
struct tm tm = *localtime(&t);
uint32_t microsecond = GetTimeStamp_ms();

if(debug)
	{
		sprintf(logmsg,"Axis :%d ,Payload : %.3f, Log Msg: %s, Return: %s",axis,payload,msg,retval);

		if(strcmp(logmsg,oldlogmsg) != 0)
		{
			fprintf(logfile,"%d-%d-%d %d:%d:%d:%d : %s \n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,microsecond,logmsg);
			memcpy(oldlogmsg,logmsg,sizeof(logmsg));
		}
	}
	return 1;
}

uint32_t GetTimeStamp_ms() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    //return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
    return tv.tv_usec/1000;
}

int closeLogFile(void)
{
	if(debug)
	{
		fclose(logfile);
	}
	return 1;
}

int waitforUDSX(int active)
{
	if(pShmem_data)
		{
			if(active)
			{
				pShmem_data->udsx_enter = 0;
				pShmem_data->udsx_exit = 0;
				while(pShmem_data->udsx_enter == 0 || pShmem_data->udsx_exit == 0)
				{
					;
				};
			}
		}

	return 1;
}


void rushMakeBuffer(char* bufferout, char* bufferin, int* pointer, int size, char flag)
{
	memcpy(bufferout + *pointer, "786", 3);
	*pointer += 3;
	memcpy(bufferout + *pointer, &flag, sizeof(char));
	*pointer += sizeof(char);
	memcpy(bufferout + *pointer, &size, sizeof(int));
	*pointer += sizeof(int);
	memcpy(bufferout + *pointer, bufferin, size);
	*pointer += size;
	//memcpy(bufferout + *pointer, "rus", 3);
	//*pointer += 3;
}

int rushSearchBuffer(unsigned long int* start, int* buffersize, int* size, char *flag, unsigned long int oriStart, int oriSize)
{
	int pointer = 0;
	if (oriSize < 0)
	{
		return -1;
	}
	pointer = rushMemsearch(*start, *buffersize, "786", 3);
	if (pointer < 0)
	{
		return -1;
	}
	pointer += 3;
	memcpy(flag, *start + (void*)pointer, sizeof(char));
	pointer += sizeof(char);
	memcpy(size, *start + (void*)pointer, sizeof(int));
	pointer += sizeof(int);

	*start = pointer + *start;
	*buffersize = (oriSize - (*start - oriStart));//(pointer + *size -4);

	return 1;
}

/* binary search in memory */
int rushMemsearch(const char *hay, int haysize, const char *needle, int needlesize) {
	int haypos, needlepos;
	haysize -= needlesize;
	for (haypos = 0; haypos <= haysize; haypos++) {
		for (needlepos = 0; needlepos < needlesize; needlepos++) {
			if (hay[haypos + needlepos] != needle[needlepos]) {
				// Next character in haystack.
				break;
			}
		}
		if (needlepos == needlesize) {
			return haypos;
		}
	}
	return -1;
}


static void onData(dyad_Event *e)
{

	char send_buffer_300[400];


	int pSend;

	unsigned long int start, oriStart;
	int buffersize, oriSize;
	int size = 0;
	char flag;
	char command;
	int sentCount = 0;
	int x;


	//printf("%s", e->data);

	oriStart = start = e->data;
	oriSize = buffersize = e->size;
	command = 0;

	while (1)
	{
		if (rushSearchBuffer(&start, &buffersize, &size, &flag, oriStart, oriSize) >= 0)
		{
			switch (flag)
			{
			case E_CMD_FLG:
				memcpy(CMD_FLG, (void*)start, size);
				break;
			case E_CTR_FLG:
				memcpy(CTR_FLG, (void*)start, size);
				break;
			case E_FORCE_LIMIT:
				memcpy(FORCE_LIMIT, (void*)start, size);
				break;
			case E_AXS_TYPE:
				memcpy(AXS_TYPE, (void*)start, size);
				break;
			case E_AXS_NAM0:
				memcpy(AXS_NAM0, (void*)start, size);
				break;
			case E_AXS_NAM1:
				memcpy(AXS_NAM1, (void*)start, size);
				break;
			case E_AXS_NAM2:
				memcpy(AXS_NAM2, (void*)start, size);
				break;
			case E_AXS_NAM3:
				memcpy(AXS_NAM3, (void*)start, size);
				break;
			case E_AXS_NAM4:
				memcpy(AXS_NAM4, (void*)start, size);
				break;
			case E_AXS_NAM5:
				memcpy(AXS_NAM5, (void*)start, size);
				break;
			case E_AXS_NAM6:
				memcpy(AXS_NAM6, (void*)start, size);
				break;
			case E_AXS_NAM7:
				memcpy(AXS_NAM7, (void*)start, size);
				break;
			case E_AXS_NAM8:
				memcpy(AXS_NAM8, (void*)start, size);
				break;
			case E_AXS_NAM9:
				memcpy(AXS_NAM9, (void*)start, size);
				break;
			case E_REQ_STAT:
				memcpy(&command, (void*)start, size);
				switch (command){
				case E_NYCE_INIT:
					sys_case = SYS_INIT;
					break;
				case E_NYCE_STOP:
					sys_case = SYS_STOP;
					break;
				}
				break;
			}
		}
		else
			break;

		if (buffersize <= 0)
		{
			break;
		}
	}

	if (pShmem_data)
	{
		memcpy(&pShmem_data->FORCE_LIMIT[0], &FORCE_LIMIT[0], sizeof(FORCE_LIMIT));
	}

	NyceMainLoop();




//	char send_buffer_300[300];


//	int pSend;
//	int sentCount = 0;
//	int x;

	memset(send_buffer_300, 0, sizeof(send_buffer_300));

		pSend = 0;
		sentCount = 0;
		if (pShmem_data){
			for(x = 0 ; x<10 ; x++)
			{
				if(pShmem_data->STAT_FLG[x] != OLD_STAT_FLG[x])
				{
					rushMakeBuffer(send_buffer_300, &pShmem_data->STAT_FLG, &pSend, sizeof(unsigned int) * 10, E_STAT_FLG);
					memcpy(OLD_STAT_FLG,pShmem_data->STAT_FLG,sizeof(OLD_STAT_FLG));
					break;
				}
			}

			if (sentCount == 0)
			{
				rushMakeBuffer(send_buffer_300, &pShmem_data->VC_POS, &pSend, sizeof(float) * 20, E_VC_POS);
				for(x = 0 ; x<10 ; x++)
				{
					if(pShmem_data->NET_CURRENT[x] != OLD_NET_CURRENT[x])
					{
						rushMakeBuffer(send_buffer_300, &pShmem_data->NET_CURRENT, &pSend, sizeof(float) * 10, E_NET_CURRENT);
						memcpy(OLD_NET_CURRENT,pShmem_data->NET_CURRENT,sizeof(OLD_NET_CURRENT));
						break;
					}
				}

			}
			sentCount++;
			if (sentCount >= 70)
			{
				sentCount = 0;
			}
		}

//		for(x = 0 ; x<10 ; x++)
//		{
//			if(CMD_FLG[x] != OLD_CMD_FLG[x])
//			{
//				rushMakeBuffer(send_buffer_300, CMD_FLG, &pSend, sizeof(float) * 10, E_CMD_FLG);
//				memcpy(OLD_CMD_FLG,CMD_FLG,sizeof(OLD_CMD_FLG));
//				break;
//			}
//		}


		rushMakeBuffer(send_buffer_300, &sys_case, &pSend, sizeof(char) * 1, E_SYS_CASE);
		dyad_write(e->stream, send_buffer_300, pSend);



}

static void onAccept(dyad_Event *e) {
	dyad_addListener(e->remote, DYAD_EVENT_DATA, onData, NULL);
	//dyad_addListener(e->remote, DYAD_EVENT_DATA, onReady, NULL);
	int opt = 1;
	dyad_setNoDelay(e->remote, opt);
	int socket = dyad_getSocket(e->remote);
	setsockopt(socket, SOL_SOCKET, SO_DONTROUTE, &opt, sizeof(opt));

	dyad_writef(e->remote, "echo server\r\n");
}

static void onError(dyad_Event *e) {
	printf("server error: %s\n", e->msg);
}

static void onReady(dyad_Event *e){
;
}


