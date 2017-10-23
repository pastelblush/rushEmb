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

/* Include MY_UDSX_ARGS type and USR error codes */
#include "rushEmb.h"



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
#if defined(SO)
#define USR_ERR_NOT_INITIALIZED             ((NYCE_STATUS)((NYCE_ERROR_MASK)|((N4K_SS_USR<<NYCE_SUBSYS_SHIFT)|3)))
#endif

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

//////start UDSX
void StartUdsx(NHI_NODE nodeId, const char* udsxFilePath);


//nyce node
NHI_NODE    nodeId;

//mutex
pthread_mutex_t lock;

#if defined(BIN)
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

volatile sig_atomic_t g_stop;

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

int sys_case;
char logstr[80];

int main(void)
{
	 initLogFile();
	//init buffer mutex


    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    logging(100,(float)0,"Init mutex","success"); ///// log


    NYCE_STATUS retVal;


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


	//start server
	stop_eth = 0;
	pthread_create(&serverpth,NULL,server,"in thread");
	logging(100,0,"Start ETH server","success");  ////////////////log


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
				break;
			case SYS_READY:
				//nyce main loop
				logging(123,sys_case,"system ready","status"); ///// log
				//pthread_mutex_lock(&lock);
				//	NyceMainLoop();
				//pthread_mutex_unlock(&lock);
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
		usleep(500000);


    }

        /*
         * Terminate the shared memory before we stop the application.
         */

      stop_eth = 1;
      puts("terminating eth");
      sleep(1); // wait all eth close
      logging(100,1,"stopping ETH","success");  ////////////////log

      //in case of force termination

      NyceDisconnectAxis();
      logging(100,1,"disconnect axis","success");  ////////////////log


      EndForceUDSX();


      retVal = NhiDisconnect(nodeId);
      if ( NyceError(retVal) )
      {

           printf("NhiDisconnect Error %s\n", NyceGetStatusString(retVal));
           return 0;
      }

      logging(100,1,"Nhi disconnected",NyceGetStatusString(retVal));  ////////////////log


      retVal = NyceTerm();
      if (NyceError(retVal))
      {
    	  printf("NyceTerm Error %s\n", NyceGetStatusString(retVal));
    	  return 0;
      }
      logging(100,1,"nyce terminated",NyceGetStatusString(retVal));  ////////////////log

      //disable mutex
      pthread_mutex_destroy(&lock);

      closeLogFile();


    return (int)retVal;
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


	//-----------------
    // Connect the axes
    //----------------
//    for ( ax = 0; ax < 10; ax++ )
//    {
//		if (Axis_Type[ax] != NA)
//		{
//			StatusSConnect[ax] = SacConnect(Axis_Name[ax], &sacAxis[ax]);
//
//
//			if (NyceSuccess(StatusSConnect[ax]))
//			{
//				if(NyceSuccess(SacReset(sacAxis[ax])))
//				{
//					SacSynchronize(sacAxis[ax],SAC_REQ_RESET,3);
//				}
//
//				if(NyceSuccess(SacShutdown(sacAxis[ax])))
//				{
//					SacSynchronize(sacAxis[ax],SAC_REQ_SHUTDOWN,3);
//				}
//
//				if(NyceSuccess(SacInitialize(sacAxis[ax], SAC_USE_FLASH)))
//				{
//					SacSynchronize(sacAxis[ax],SAC_REQ_INITIALIZE,3);
//					printf("axis %d connected\n",ax);
//					SacConnected[ax] = 255;
//				}
//			}
//		}
//	}

    for ( ax = 0; ax < 10; ax++ )
    {
		if (Axis_Type[ax] != NA)
		{
			StatusSConnect[ax] = SacConnect(Axis_Name[ax], &sacAxis[ax]);

			if (StatusSConnect[ax] == 0)
			{
				//SacAlignMotor(sacAxis[ax]);
				//SacSynchronize(sacAxis[ax], SAC_REQ_ALIGN_MOTOR, 5.0);
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
									SacOpenLoop(sacAxis[ax]);
									StatusOpenLoop[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_OPEN_LOOP,3);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
						 			break;

								case AxisLock:
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"open loop","turret");  ////////////////log
									if (NyceError(SacLock(sacAxis[ax])))
									{
										printf("Lock Error Axis: %d\n",ax);
									}
									StatusLock[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_LOCK,3);
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
									SacOpenLoop(sacAxis[ax]);
									StatusOpenLoop[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_OPEN_LOOP,3);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;

								case AxisLock:
									puts("axis lock");
									logging(ax,(float)pShmem_data->Shared_StatFlag[ax],"lock","pusher");  ////////////////log
									StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_RAMP,CTR_FLG[17]);
                                    StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_VALUE,0);
									SacLock(sacAxis[ax]);
									StatusLock[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_LOCK,3);
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
					logging(ax,pShmem_data->Shared_StatFlag[ax],"Shared_StatFlag"," NyceMainLoop");  ////////////////log

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

		waitforUDSX(resp_cmd);
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
	char return_filename[50];

	BOOL udsxRun;

	Terminate();
	//printf("Stopping node id is: %d \n",nodeId);
    //NhiUdsxGetInfo(nodeId, &udsxRun, return_filename, NYCE_MAX_PATH_LENGTH);
    //if(udsxRun)
    //{
    	return_stat = NhiUdsxStop(nodeId);

    	logging(1,(float)return_stat,"NhiUdsxStop","EndForceUDSX");  ////////////////log
    	//printf("UDSX Stop Stat : %s \n",NyceGetStatusString(return_stat));
    //}
    //else
    //{
    	//printf("No UDSX Running\n");
    //}
 }
#endif

#if defined(SO)
#include <udsxapi.h>

NYCE_STATUS g_retVal = USR_ERR_NOT_INITIALIZED;

NYCE_STATUS UdsxInitialize(const void* argument, uint32_t argumentSize)
{
    /*
     * Because the creation of the shared memory causes a switch from the Xenomai domain
     * to the linux domain, the creation of the shared memory must be performed from
     * within the linux domain. The loading of the shared object is performed within the
     * linux domain. Here we can only return the result of that function execution.
     */
    UNUSED(argument);
    UNUSED(argumentSize);

    return g_retVal;
}

void UdsxExecuteAtSampleStart(void)
{
    /*
     * The UdsxExecuteAtSampleStart is not needed for this example,
     * but an empty function is used to prevent a warning from NhiUdsxStart.
     */
}

void UdsxExecuteAtSampleEnd(void)
{
    /*
     * Check whether the shared memory is actually available.
     */
    if (pShmem_data)
    {
        /*
         * Increase the counter.
         */
        (pShmem_data->Shared_StatFlag[0])++;
    }
}

void UdsxTerminate(void)
{
    /*
     * Termination of the shared memory must be performed in linux domain.
     * Within Xenomai domain no termination is needed within this example.
     */
}

/**
 *  @brief  On load function that is executed when the shared object is loaded
 *
 *  Because of the constructor attribute that is set for this function, this
 *  function is executed when the shared object is loaded, which is done within
 *  the linux domain.
 */
__attribute__((constructor)) void OnLoad(void)
{
    g_retVal = Initialize(TRUE);
}

/**
 *  @brief  On unload function that is executed when the shared object is unloaded
 *
 *  Because of the destructor attribute that is set for this function, this
 *  function is executed when the shared object is unloaded, which is done within
 *  the linux domain.
 */
__attribute__((destructor)) void OnUnload(void)
{
    Terminate();
}

#endif

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

void StartUdsx(NHI_NODE nodeId, const char* udsxFilePath)
{
    NYCE_STATUS     retVal;
    MY_UDSX_ARGS    udsxArgs;

    /* Let the UDSX check analog input 0 of the drive module in slot 0 */
    udsxArgs.anInputvarId = NHI_VAR_AN_IN0_VALUE_SLOT0;
    udsxArgs.anInErrorThreshold   = 2.5;
    udsxArgs.anInWarningThreshold = 5.0;

    retVal = NhiUdsxStart(nodeId, udsxFilePath , &udsxArgs, (uint32_t)sizeof(udsxArgs));

    if (retVal == NYCE_OK)
    {
        printf("UDSX started successfully.\n");
    }
    else
    {
        /* Check for USR errors/warnings (see my_udsx.h) */
        switch (retVal)
        {
            case MY_UDSX_ERR_INVALID_ARG:
                printf("ERROR: UDSX not started: invalid argument.\n");
                break;

            case MY_UDSX_ERR_AN_IN_LEVEL_TOO_LOW:
                printf("ERROR: UDSX not started: analog input too low.\n");
                break;

            case MY_UDSX_WRN_AN_IN_LEVEL_LOW:
                printf("WARNING: UDSX running, but analog input is lower than %gV.\n", udsxArgs.anInWarningThreshold);
                break;

            default:
                printf("Failed to start UDSX: %s\n", NyceGetStatusString(retVal));  /* Assume retVal is an error */
        }
    }
}

void *server(void *arg)
{
	char *str;
	struct ThreadArgs *threadArgs;
	int servSock,clntSock,true_val = 1;
	
	
	str = (char *)arg;
	printf("%s\n",str);

	pthread_detach(pthread_self());
	
	
	////////////////////////////Multiport Support////////////////////////////////////////
	int i,j;
	int addrlen,new_socket;
	struct sockaddr_in address;
	int opt = TRUE;
	int max_sd,sd,activity;
	
	char* echoBuffer; /* Buffer for echo message */
	struct cmd_buff cmdBuffer;
	int recvMsgSize;             /* Size of received message */
	struct resp_buff nyceStatusBuffer;
	int statusBufferSize;
	int pingCmd;
	
	
	for (i = 0; i < max_clients; i++) 
    {
        client_socket[i] = 0;
    }
	
	for(i=0; i < max_ports;i++)
	{
		ports[i] = 0;
		master_socket[i] = 0;
	}
	
	ports[0] = 6666;
	ports[1] = 9999;

	
	//create a master socket
	for(i = 0;i < max_ports;i++)
	{
		if(ports[i]!=0)
		{
			if( (master_socket[i] = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
			{
				perror("socket failed");
				exit(EXIT_FAILURE);
			}
			
			//set master socket to allow multiple connections , this is just a good habit, it will work without this
			if( setsockopt(master_socket[i], SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
			{
				perror("setsockopt");
				exit(EXIT_FAILURE);
			}
		  
			//type of socket created
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = INADDR_ANY;
			address.sin_port = htons( ports[i] );
			  
			//bind the socket to localhost port 8888
			if (bind(master_socket[i], (struct sockaddr *)&address, sizeof(address))<0) 
			{
				perror("bind failed");
				exit(EXIT_FAILURE);
			}
			printf("Listener on port %d \n", ports[i]);
			logging(177,ports[i],"Listener on port","server");
			 
			//try to specify maximum of 3 pending connections for the master socket
			if (listen(master_socket[i], 3) < 0)
			{
				perror("listen");
				exit(EXIT_FAILURE);
			}
		}
	}
	
	//accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
	
	
	echoBuffer = malloc(sizeof(struct cmd_buff) + 256);
	memset(echoBuffer,0,sizeof(struct cmd_buff) + 256);
	resp_cmd = 0;
	max_sd = 0;
	
	while(TRUE) 
    {
        //clear the socket set
        FD_ZERO(&readfds);
  
        //add master socket to set
		for(i = 0;i < max_ports;i++)
		{
			//socket descriptor
			sd = master_socket[i];

			//if valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET( sd , &readfds);

			//highest file descriptor number, need it for the select function
			if(sd > max_sd)
				max_sd = sd;
		}
         
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            //socket descriptor
            sd = client_socket[i];
             
            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);
             
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }
  
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);
    
        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
          
		for(i=0;i<max_ports;i++)
		{
			if(master_socket[i] != 0)
			{
				//If something happened on the master socket , then its an incoming connection
				if (FD_ISSET(master_socket[i], &readfds)) 
				{
					if ((new_socket = accept(master_socket[i], (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
					{
						perror("accept");
						exit(EXIT_FAILURE);
					}
				  
					//inform user of socket number - used in send and receive commands
					printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
					  
					//add new socket to array of sockets
					for (j = 0; j < max_clients; j++) 
					{
						//if position is empty
						if( client_socket[j] == 0 )
						{
							client_socket[j] = new_socket;
							server_port_list[j] = ports[i];
							printf("Adding to list of sockets as %d\n" , i);
							 
							break;
						}
					}
				}
			}
		}
          
		  
        //else its some IO operation on some other socket :)
        for (i = 0; i < max_clients; i++) 
        {
            sd = client_socket[i];
              
            if (FD_ISSET( sd , &readfds)) 
            {
                //Check if it was for closing , and also read the incoming message
                if ((recvMsgSize = recv(sd, echoBuffer, sizeof(struct cmd_buff) + 256, 0)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                      
                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }
                  
                //Echo back the message that came in
                else
                {
					memcpy(&cmdBuffer,echoBuffer,sizeof(cmdBuffer));
					handleBuffer(&cmdBuffer,&resp_cmd);
					
					if(resp_cmd)
					{
						NyceMainLoop();
						memset(&nyceStatusBuffer,0,sizeof(nyceStatusBuffer));
						prepareStatusBuffer(&nyceStatusBuffer,&statusBufferSize);
						if(send(sd, &nyceStatusBuffer, statusBufferSize, 0)  < 0)
									DieWithError("send() failed");

						if(!(resp_cmd == 16 || resp_cmd == 17))
						{
							logging(99,(float)resp_cmd,"loop send","HandleTCPClient"); /////logging
						}

					}
					else
					{
						pingCmd = E_PING;
						if(send(sd, &pingCmd, sizeof(pingCmd), 0)  < 0)
							DieWithError("send() failed");
					}
                }
            }
        }
		
        if(!resp_cmd)
        {
        	//run nyce main loop
        	NyceMainLoop();
        	resp_cmd = 0;
        }

		if(stop_eth)
		{
			//close(servSock);
			break;
		}

		usleep(200);
    }
	
	
	//////////////////////////////////////////////////////////////////////////////////////
	

//		for(;;) /* Run forever */
//		{
//			/* Set the size of the in-out parameter */
//			clntLen = sizeof(echoClntAddr);
//			/* Wait for a client to connect */
//			if((clntSock = accept(servSock, (struct sockaddr*)&echoClntAddr, &clntLen)) < 0)
//			{
//				logging(177,177,"accept failed","server");
//				DieWithError("accept() failed");
//			}else
//			{
//				logging(177,177,"accept done","server");
//				puts("client Accepted");
//			}
//
//			/* Create separate memory for client argument */
//			if ((threadArgs= (struct ThreadArgs*) malloc(sizeof(struct ThreadArgs))) == NULL)
//				DieWithError("failed to create client handle");
//
//			threadArgs-> clntSock = clntSock;
//
//			/* clntSock is connected to a client! */
//			printf("Handling Client %s\n", inet_ntoa(echoClntAddr.sin_addr));
//				pthread_create(&pth,NULL,clientThread,(void*)threadArgs);
//			puts("client thread created");
//
//			if(stop_eth)
//			{
//				close(servSock);
//				break;
//			}
//		}
	return 0;
    /* NOT REACHED */
}

void *clientThread(void *arg)
{
	int clntSock;

	logging(177,177,"client thread created","clientThread");
	pthread_detach(pthread_self());

	clntSock = ((struct ThreadArgs*)arg)->clntSock;
	free(arg);

	socketSetUp(clntSock);
	HandleTCPClient(clntSock);

	return NULL;
}

void socketSetUp(int socket)
{
		unsigned int recvBuffSize;
		unsigned int sockOptSize;
		struct timeval tv;
		int true_val = 1;

		//check buffer size
		sockOptSize= sizeof(recvBuffSize);
		if(getsockopt(socket,SOL_SOCKET,SO_RCVBUF,&recvBuffSize,&sockOptSize) < 0)
			DieWithError("getsockopt() failed");

		printf("InitialReceive Buffer Size: %d\n", recvBuffSize);

		recvBuffSize = MAX_BUFFER_SIZE * 20;
		/* Set the buffer size to new value */
		if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &recvBuffSize, sizeof(recvBuffSize)) < 0)
			DieWithError("setsockopt() failed");


		tv.tv_sec = 3000;  /* 3 Secs Timeout */
		setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
		setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
		setsockopt(socket, IPPROTO_TCP,SO_KEEPALIVE, (void *)&true_val, sizeof(true_val));
}

void HandleTCPClient(int clntSocket)
{
	char* echoBuffer; /* Buffer for echo message */
	struct cmd_buff cmdBuffer;
	int recvMsgSize;             /* Size of received message */
	struct resp_buff nyceStatusBuffer;
	int statusBufferSize,count;

	puts("system data exchange ready");
	/* Receive message from client */


	echoBuffer = malloc(sizeof(struct cmd_buff) + 256);

	resp_cmd = 0;
	memset(echoBuffer,0,sizeof(struct cmd_buff) + 256);

	logging(99,(float)resp_cmd,"initial recieving","HandleTCPClient");///////////////////logging
	if((recvMsgSize = recv(clntSocket, echoBuffer, sizeof(struct cmd_buff) + 256, 0)) < 0)
		DieWithError("recv() failed");

	memcpy(&cmdBuffer,echoBuffer,sizeof(cmdBuffer));
	handleBuffer(&cmdBuffer,&resp_cmd);

	//pthread_mutex_lock(&lock);
	for(count = 0;count < 1;count++)
	{
		NyceMainLoop();
	}
	//pthread_mutex_unlock(&lock);



	/* Send received string and receive again until end of transmission */
	while(recvMsgSize > 0) /* zero indicates end of transmission */
	{


		if(resp_cmd){
			memset(&nyceStatusBuffer,0,sizeof(nyceStatusBuffer));
			prepareStatusBuffer(&nyceStatusBuffer,&statusBufferSize);
			if(send(clntSocket, &nyceStatusBuffer, statusBufferSize, 0)  < 0)
						DieWithError("send() failed");

			if(!(resp_cmd == 16 || resp_cmd == 17))
			{
				logging(99,(float)resp_cmd,"loop send","HandleTCPClient"); /////logging
			}

		}else
		{
			resp_cmd = E_PING;
			if(send(clntSocket, &resp_cmd, sizeof(resp_cmd), 0)  < 0)
									DieWithError("send() failed");
		}

		//pthread_mutex_lock(&lock);
		for(count = 0;count < 0;count++)
		{
			NyceMainLoop();
		}
		//pthread_mutex_unlock(&lock);


		resp_cmd = 0;
		memset(echoBuffer,0,sizeof(struct cmd_buff) + 256);
		/* See if there is more data to receive */


		if((recvMsgSize = recv(clntSocket, echoBuffer, sizeof(struct cmd_buff) + 256, 0)) < 0)
			DieWithError("connection terminated");

		memcpy(&cmdBuffer,echoBuffer,sizeof(cmdBuffer));
		handleBuffer(echoBuffer,&resp_cmd);

		for(count = 0;count < 1;count++)
		{
			NyceMainLoop();
		}

		if(!(resp_cmd == 16 ||resp_cmd == 17))
		{
			logging(99,(float)resp_cmd,"loop recieved","HandleTCPClient");/////////////////logging
		}


		if(stop_eth)
		{
			close(clntSocket); // if close event happened
			break;
		}
	}
	close(clntSocket); /* Close client socket */
	printf("client socket close \n");
}

int handleBuffer(void *arg, int* resp_cmd)
{

	//pthread_mutex_lock(&lock);
	struct cmd_buff *buffer;
	int retVar;
	buffer = (struct cmd_buff *)arg;
	int cmdFlg;

	retVar = 0;

	cmdFlg = buffer->cmd/10000;
	*resp_cmd = 0;

	switch(cmdFlg){
	default:
		if(buffer->cmd == E_CMD_FLG)
			{
				memcpy(CMD_FLG,buffer->fbuff,buffer->size);
			}
			else if(buffer->cmd == E_CTR_FLG)
			{
				memcpy(CTR_FLG,buffer->fbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM0)
			{
				memcpy(AXS_NAM0,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM1)
			{
				memcpy(AXS_NAM1,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM2)
			{
				memcpy(AXS_NAM2,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM3)
			{
				memcpy(AXS_NAM3,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM4)
			{
				memcpy(AXS_NAM4,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM5)
			{
				memcpy(AXS_NAM5,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM6)
			{
				memcpy(AXS_NAM6,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM7)
			{
				memcpy(AXS_NAM7,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM8)
			{
				memcpy(AXS_NAM8,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM9)
			{
				memcpy(AXS_NAM9,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_TYPE)
			{
				memcpy(AXS_TYPE,buffer->ibuff,buffer->size);
			}
			else if((buffer->cmd == E_FORCE_LIMIT) && pShmem_data)
			{
				memcpy(pShmem_data->FORCE_LIMIT,buffer->fbuff,buffer->size);
			}
			else if(buffer->cmd == E_NYCE_INIT)
			{
				sys_case = SYS_INIT;
				puts("SYSTEM INIT");
			}
			else if(buffer->cmd == E_NYCE_STOP)
			{
				sys_case = SYS_STOP;
				puts("SYSTEM STOP");
			}
			else
			{
				retVar = -1;
				//pthread_mutex_unlock(&lock);
			}
		break;

	case 2:
		*resp_cmd = buffer->cmd - (2 * 10000);
		break;
	case 3:
		*resp_cmd = buffer->cmd - (3 * 10000);
		buffer->cmd = *resp_cmd;

		if(buffer->cmd == E_CMD_FLG)
			{
				memcpy(CMD_FLG,buffer->fbuff,buffer->size);
			}
			else if(buffer->cmd == E_CTR_FLG)
			{
				memcpy(CTR_FLG,buffer->fbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM0)
			{
				memcpy(AXS_NAM0,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM1)
			{
				memcpy(AXS_NAM1,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM2)
			{
				memcpy(AXS_NAM2,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM3)
			{
				memcpy(AXS_NAM3,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM4)
			{
				memcpy(AXS_NAM4,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM5)
			{
				memcpy(AXS_NAM5,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM6)
			{
				memcpy(AXS_NAM6,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM7)
			{
				memcpy(AXS_NAM7,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM8)
			{
				memcpy(AXS_NAM8,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_NAM9)
			{
				memcpy(AXS_NAM9,buffer->cbuff,buffer->size);
			}
			else if(buffer->cmd == E_AXS_TYPE)
			{
				memcpy(AXS_TYPE,buffer->ibuff,buffer->size);
			}
			else if((buffer->cmd == E_FORCE_LIMIT) && pShmem_data)
			{
				memcpy(pShmem_data->FORCE_LIMIT,buffer->fbuff,buffer->size);
			}
			else if(buffer->cmd == E_NYCE_INIT)
			{
				sys_case = SYS_INIT;
				puts("SYSTEM INIT");
			}
			else if(buffer->cmd == E_NYCE_STOP)
			{
				sys_case = SYS_STOP;
				puts("SYSTEM STOP");
			}
			else
			{
				retVar = -1;
				//pthread_mutex_unlock(&lock);
			}
	}


	//pthread_mutex_unlock(&lock);
	return retVar;
}




void DieWithError(char* errorMessage)
{
	printf("%s \n",errorMessage);
	//exit(1);
}


void prepareStatusBuffer(void *statBuff, int* buffersize)
{
	struct resp_buff *statusBuffer;
	int count;

	statusBuffer = (struct resp_buff *)statBuff;
	statusBuffer->status = 0; //reset flag

	if (pShmem_data)
	{
		for(count = 0; count < 20;count++)
		{
				statusBuffer->VC_POS[count] = pShmem_data->VC_POS[count];
				statusBuffer->status |= 0x01;
		}

		for(count = 0; count < 10;count++)
		{

				statusBuffer->Shared_StatFlag[count] = pShmem_data->STAT_FLG[count];
				statusBuffer->status |= 0x02;

		}

		for(count = 0; count < 10;count++)
			{
					statusBuffer->NET_CURRENT[count] = pShmem_data->NET_CURRENT[count];
					statusBuffer->status |= 0x04;
			}


		for(count = 0; count < 10;count++)
			{
					statusBuffer->CMD_FLG[count]= CMD_FLG[count];
					statusBuffer->status |= 0x10;
			}

	}


		statusBuffer->sys_case = sys_case;
		statusBuffer->status |= 0x08;

		*buffersize = sizeof(struct resp_buff);

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


int logging(int axis,float payload,char* msg, char* retval)
{

char logmsg[180];
time_t t = time(NULL);
struct tm tm = *localtime(&t);

if(debug)
	{
		sprintf(logmsg,"Axis :%d ,Payload : %.3f, Log Msg: %s, Return: %s",axis,payload,msg,retval);

		if(strcmp(logmsg,oldlogmsg) != 0)
		{
			fprintf(logfile,"%d-%d-%d %d:%d:%d : %s \n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,logmsg);
			memcpy(oldlogmsg,logmsg,sizeof(logmsg));
		}
	}
	return 1;
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
