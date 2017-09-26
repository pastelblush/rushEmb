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

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <sactypes.h>
#include "sysapi.h"
#include "udsxapi.h"

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

int main(void)
{
	//init buffer mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    NYCE_STATUS retVal;

    //connect to NYCE
    retVal = NyceInit(NYCE_ETH);
    if (NyceError(retVal))
    {
    	printf("NyceInit Error %s\n", NyceGetStatusString(retVal));
    	return 0;
    }

	while((!NyceSuccess(SysSynchronize(SYS_REQ_NETWORK_OPERATIONAL,0,10))))
	{
		puts("waiting for SysSynchronize");
	}
    puts("SysSynchronize done");


    retVal = NhiConnect("localhost", &nodeId);
    if (NyceError(retVal))
    {
       printf("NhiConnect Error %s\n", NyceGetStatusString(retVal));
       return 0;
    }

    printf("original node id is: %d \n",nodeId);


    if ( NyceError(retVal) )
    {

         printf("NhiConnect Error %s\n", NyceGetStatusString(retVal));
         return 0;
    }

    //End previous udsx
    EndForceUDSX();

    //initialize buffer
	initializeBuffer();

	//start server
	stop_eth = 0;
	pthread_create(&serverpth,NULL,server,"in thread");


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
				//init all axis
				AxisInit();

				CTR_FLG[19] = 255;	//Terminate sequence flag
				sys_case = SYS_READY;
				break;
			case SYS_READY:
				//nyce main loop
				pthread_mutex_lock(&lock);
					NyceMainLoop();
				pthread_mutex_unlock(&lock);
				if(CTR_FLG[19] != 255)
				{
					sys_case = SYS_STOP;
				}
				break;
			case SYS_STOP:
				puts("stop");
				CTR_FLG[19] = 0;
				NyceDisconnectAxis();
				EndForceUDSX();
				sys_case = SYS_IDLE;
				break;

		}
		usleep(10);


    }

        /*
         * Terminate the shared memory before we stop the application.
         */
      stop_eth = 1;
      puts("terminating eth");
      sleep(1); // wait all eth close

      //in case of force termination
      NyceDisconnectAxis();
      EndForceUDSX();

      retVal = NhiDisconnect(nodeId);
      if ( NyceError(retVal) )
      {

           printf("NhiDisconnect Error %s\n", NyceGetStatusString(retVal));
           return 0;
      }


      retVal = NyceTerm();
      if (NyceError(retVal))
      {
    	  printf("NyceTerm Error %s\n", NyceGetStatusString(retVal));
    	  return 0;
      }

      //disable mutex
      pthread_mutex_destroy(&lock);


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
 	 }

 	//Initialize the shared memory.
 	retVal = Initialize(FALSE);
 	if ( NyceError(retVal) )
 	   {
 		printf("Initialize Error %s\n", NyceGetStatusString(retVal));
 	   }
 	else
 	{
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
    for ( ax = 0; ax < 10; ax++ )
    {
		if (Axis_Type[ax] != NA)
		{
			StatusSConnect[ax] = SacConnect(Axis_Name[ax], &sacAxis[ax]);


			if (NyceSuccess(StatusSConnect[ax]))
			{
				if(NyceSuccess(SacReset(sacAxis[ax])))
				{
					SacSynchronize(sacAxis[ax],SAC_REQ_RESET,3);
				}

				//if(NyceSuccess(SacShutdown(sacAxis[ax])))
				//{
				//	SacSynchronize(sacAxis[ax],SAC_REQ_SHUTDOWN,3);
				//}

				if(NyceSuccess(SacInitialize(sacAxis[ax], SAC_USE_FLASH)))
				{
					SacSynchronize(sacAxis[ax],SAC_REQ_INITIALIZE,3);
					printf("axis %d connected\n",ax);
					SacConnected[ax] = 255;
				}
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
									ExeParabolicProfile(ax);
									break;

								case OpenLoop:
									SacOpenLoop(sacAxis[ax]);
									StatusOpenLoop[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_OPEN_LOOP,3);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;

								case AxisLock:
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
									ExeParabolicProfile(ax);
									break;

								case ChgWorkPos:
									CMD_FLG[ax] = CMD_FLG[ax] - ChgWorkPos*10000;
									CTR_FLG[ax] = CMD_FLG[ax];

									ExeParabolicProfile(ax);
									break;

								case OpenLoop:
									StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_RAMP,CTR_FLG[17]);
                                    StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_VALUE,CTR_FLG[18]);
									SacOpenLoop(sacAxis[ax]);
									StatusOpenLoop[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_OPEN_LOOP,3);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;

								case AxisLock:
									puts("axis lock");
									StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_RAMP,CTR_FLG[17]);
                                    StatusWParameter[ax] = SacWriteParameter(sacAxis[ax],SAC_PAR_OPEN_LOOP_VALUE,0);
									SacLock(sacAxis[ax]);
									StatusLock[ax] = SacSynchronize(sacAxis[ax],SAC_REQ_LOCK,3);
									pShmem_data->Shared_StatFlag[ax] = 0x01;
									break;
							}

							break;

						case STD_ABS:
							m_sacPtpPars[ax] .positionReference = SAC_ABSOLUTE;
							ExeParabolicProfile(ax);
							break;

						case STD_REL:
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

			if (pShmem_data)
			{
				pShmem_data->Shared_CtrFlag[ax] = CTR_FLG[ax];
				pShmem_data->Shared_CtrFlag[ax + 10] = CTR_FLG[ax + 10];
				pShmem_data->Shared_CtrFlag[ax + 50] = CTR_FLG[ax + 50];
				pShmem_data->Shared_CtrFlag[ax + 60] = CTR_FLG[ax + 60];
			}
		}
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

void initializeBuffer(void)
{
	int j;
	for(j=0;j<10;j++)
	{
		F_CMD_FLG_1[j] = 0;
		F_AXS_TYPE_1[j]= 0;
	}

	for(j=0;j<80;j++)
	{
		F_CTR_FLG_1[j] = 0;
	}
		//float F_CTR_FLG_1[40];

	for(j=0;j<20;j++)
	{

		F_AXS1_NAM_0[j]= 0;
		F_AXS1_NAM_1[j]= 0;
		F_AXS1_NAM_2[j]= 0;
		F_AXS1_NAM_3[j]= 0;
		F_AXS1_NAM_4[j]= 0;
		F_AXS1_NAM_5[j]= 0;
		F_AXS1_NAM_6[j]= 0;
		F_AXS1_NAM_7[j]= 0;
		F_AXS1_NAM_8[j]= 0;
		F_AXS1_NAM_9[j]= 0;
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
	int servSock,clntSock;



	str = (char *)arg;
	printf("%s\n",str);

	pthread_detach(pthread_self());


		echoServPort = PORT;
		/* Create socket for incoming connections */
		if((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		DieWithError("socket() failed");

		/* Construct local address structure */
		memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
		echoServAddr.sin_family = AF_INET;                /* Internet address family */
		echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
		echoServAddr.sin_port = htons(echoServPort);      /* Local port */

		/* Bind to the local address */
		if(bind(servSock, (struct sockaddr*)&echoServAddr, sizeof(echoServAddr)) < 0)
			DieWithError("bind() failed");

		puts("bind done");


		/* Mark the socket so it will listen for incoming connections */
		if(listen(servSock, MAXPENDING) < 0)
			DieWithError("listen() failed");

		puts("listen done");
		//check buffer size

		for(;;) /* Run forever */
		{
			/* Set the size of the in-out parameter */
			clntLen = sizeof(echoClntAddr);
			/* Wait for a client to connect */
			if((clntSock = accept(servSock, (struct sockaddr*)&echoClntAddr, &clntLen)) < 0)
				DieWithError("accept() failed");
			puts("client Accepted");

			/* Create separate memory for client argument */
			if ((threadArgs= (struct ThreadArgs*) malloc(sizeof(struct ThreadArgs))) == NULL)
				DieWithError("failed to create client handle");

			threadArgs-> clntSock = clntSock;

			/* clntSock is connected to a client! */
			printf("Handlingclient %s\n", inet_ntoa(echoClntAddr.sin_addr));
			pthread_create(&pth,NULL,clientThread,(void*)threadArgs);
			puts("client thread created");

			if(stop_eth)
			{
				close(servSock);
				break;
			}
		}
	return 0;
    /* NOT REACHED */
}

void *clientThread(void *arg)
{
	int clntSock;
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
		setsockopt( socket, IPPROTO_TCP,0x0001, (void *)&true_val, sizeof(true_val));
}

void HandleTCPClient(int clntSocket)
{
	int32_t echoBuffer[MAX_BUFFER_SIZE]; /* Buffer for echo message */
	int recvMsgSize;             /* Size of received message */
	int32_t nyceStatusBuffer[MAX_BUFFER_SIZE/4];
	int statusBufferSize;


	puts("system dataexchange ready");
	/* Receive message from client */
	if((recvMsgSize = recv(clntSocket, echoBuffer, sizeof(echoBuffer), 0)) < 0)
		DieWithError("recv() failed");

		handleBuffer(echoBuffer);
		memset(echoBuffer,0,sizeof(echoBuffer));

		//NyceMainLoop();



	/* Send received string and receive again until end of transmission */
	while(recvMsgSize > 0) /* zero indicates end of transmission */
	{
		usleep(50);
		/* Echo message back to client */
		memset(nyceStatusBuffer,0,sizeof(nyceStatusBuffer));
		prepareStatusBuffer(nyceStatusBuffer,&statusBufferSize);

		if(send(clntSocket, nyceStatusBuffer, statusBufferSize, 0)  < 0)
			DieWithError("send() failed");

		//to put the handler
		memset(nyceStatusBuffer,0,sizeof(nyceStatusBuffer));

		/* See if there is more data to receive */
		if((recvMsgSize = recv(clntSocket, echoBuffer, sizeof(echoBuffer), 0)) < 0)
			DieWithError("connection terminated");

		handleBuffer(echoBuffer);
		memset(echoBuffer,0,sizeof(echoBuffer));

		//NyceMainLoop();

		if(stop_eth)
		{
			close(clntSocket); // if close event happened
			break;
		}
	}
	close(clntSocket); /* Close client socket */
	printf("client socket close \n");
}

int handleBuffer(void *arg)
{
	int32_t *buffer;
	float temporaryBuffer[500];
	int count,retVar;
	buffer = (int32_t *)arg;

	retVar = 0;
	pthread_mutex_lock(&lock);

	if(buffer[0] == E_CMD_FLG)
	{
		for(count = 0;count < buffer[1];count++)
		{
			temporaryBuffer[count] = (float)buffer[count + 2] / 10000.00;
		}
		memcpy(CMD_FLG,temporaryBuffer,buffer[1]);
		memset(temporaryBuffer,0,500);
	}
	else if(buffer[0] == E_CTR_FLG)
	{
		for(count = 0;count < buffer[1];count++)
		{
			temporaryBuffer[count] = (float)buffer[count + 2] / 10000.00;
		}
		memcpy(CTR_FLG,temporaryBuffer,buffer[1]);
		memset(temporaryBuffer,0,500);
	}
	else if(buffer[0] == E_AXS_NAM0)
	{
		memcpy(AXS_NAM0,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM1)
	{
		memcpy(AXS_NAM1,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM2)
	{
		memcpy(AXS_NAM2,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM3)
	{
		memcpy(AXS_NAM3,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM4)
	{
		memcpy(AXS_NAM4,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM5)
	{
		memcpy(AXS_NAM5,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM6)
	{
		memcpy(AXS_NAM6,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM7)
	{
		memcpy(AXS_NAM7,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM8)
	{
		memcpy(AXS_NAM8,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_NAM9)
	{
		memcpy(AXS_NAM9,&buffer[2],buffer[1]);
	}
	else if(buffer[0] == E_AXS_TYPE)
	{
		memcpy(AXS_TYPE,&buffer[2],buffer[1]);
	}
	else if((buffer[0] == E_FORCE_LIMIT) && pShmem_data)
	{
		for(count = 0;count < buffer[1];count++)
		{
			temporaryBuffer[count] = (float)buffer[count + 2] / 10000.00;
		}
		memcpy(pShmem_data->FORCE_LIMIT,temporaryBuffer,buffer[1]);
		memset(temporaryBuffer,0,500);
		//printf("force limit : %.2f \n",pShmem_data->FORCE_LIMIT[5]);
	}
	else if(buffer[0] == E_NYCE_INIT)
	{
		sys_case = SYS_INIT;
		puts("SYSTEM INIT");
	}
	else if(buffer[0] == E_NYCE_STOP)
	{
		sys_case = SYS_STOP;
		puts("SYSTEM STOP");
	}
	else if(buffer[0] == E_PING)
	{
		;//no action
	}
	else
	{
		retVar = -1;
	}

	pthread_mutex_unlock(&lock);

	return retVar;
}




void DieWithError(char* errorMessage)
{
	printf("%s \n",errorMessage);
	//exit(1);
}


void prepareStatusBuffer(void *statBuff, int* buffersize)
{
	int32_t *statusBuffer;
	int count;
//	static int update;

	statusBuffer = (int32_t *)statBuff;
	statusBuffer[0] = 0; //reset flag

	if (pShmem_data)
	{
		for(count = 0; count < 20;count++)
		{
			//if(LAST_VC_POS[count] != pShmem_data->VC_POS[count]||(update >= 200))
			//{
			//	LAST_VC_POS[count] = pShmem_data->VC_POS[count];
				statusBuffer[count + 1] = (int32_t)(pShmem_data->VC_POS[count] * 10000);
				statusBuffer[0] |= 0x01;
			//}
		}

		for(count = 0; count < 10;count++)
		{
			//if((LAST_STAT_FLG[count] != pShmem_data->Shared_StatFlag[count]) || (update >= 200))
			//{
			//	LAST_STAT_FLG[count] = pShmem_data->Shared_StatFlag[count];
				statusBuffer[count + 20 + 1] = (int32_t)(pShmem_data->Shared_StatFlag[count]);
				statusBuffer[0] |= 0x02;
			//	update = 0;
			//}
		}

		for(count = 0; count < 10;count++)
			{
				//if(LAST_NET_CURRENT[count] != pShmem_data->NET_CURRENT[count]||(update >= 200))
				//{
				//	LAST_NET_CURRENT[count] = pShmem_data->NET_CURRENT[count];
					statusBuffer[count + 20 + 10 + 1] = (int32_t)(pShmem_data->NET_CURRENT[count] * 10000);
					statusBuffer[0] |= 0x04;
				//}
			}

		//Tell system is ready
		if(sys_case == SYS_READY)
		{
			statusBuffer[0] |= 0x08;
		}

		for(count = 0; count < 10;count++)
			{
				//if(LAST_CMD_FLG[count] != CMD_FLG[count]||(update >= 200))
				//{
				//	LAST_CMD_FLG[count] = CMD_FLG[count];
					statusBuffer[count + 10 + 20 + 10 + 1] = (int32_t)(CMD_FLG[count] * 10000);
					if(CMD_FLG[count]>0)
						printf("%d\n",statusBuffer[count + 10 + 20 + 10 + 1]);
					statusBuffer[0] |= 0x10;
				//}
			}
	}

//if(statusBuffer[0] & 0x10)
//	{
		*buffersize = (10 + 10 + 10 + 20 + 1)*sizeof(int32_t);
//	}
//if(statusBuffer[0] & 0x04)
//	{
//		*buffersize = (10 + 10 + 20 + 1)*sizeof(int32_t);
//	}
//	else if(statusBuffer[0] & 0x02)
//	{
//		*buffersize = (10 + 20 + 1)*sizeof(int32_t);
//	}
//	else if(statusBuffer[0] & 0x01)
//	{
//		*buffersize = (20)*sizeof(int32_t);
//	}
//	else
//	{
//		*buffersize = (1)*sizeof(uint32_t);
//	}
//
//	update++;
}
