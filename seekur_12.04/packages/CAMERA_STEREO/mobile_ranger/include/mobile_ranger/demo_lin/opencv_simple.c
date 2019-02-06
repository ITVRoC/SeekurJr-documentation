///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2009 Focus Robotics. All rights reserved. 
//
// Created by    :  Jason Peck
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 2 of the License, or (at your 
// option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////
#include "cv.h"
#include "highgui.h"
#include "frcam.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <sys/time.h>
#include <ctype.h>

void usageerror()
{
	puts("Usage: openvc_simple [-d <device>]\n\n\t-d <device>\tSelect nDepth device (card). Default is 0 for first device.");
	exit(2);
}

int main( int argc, char** argv )
{
	FrChannel *channel[3];
	IplImage *frame[3];
	char wndname[3][50];

	int i;
	int device = 0; 

	for(i = 1; i < argc; ++i)
	{
        	if(strcmp(argv[i], "-d") == 0)
           	{
			if(argc > i)
				device = atoi(argv[++i]);
			else
				usageerror();
	   	}
		else
		{
			usageerror();
		}
	}
	  
	printf("device %d\n", device);
 	  

	/* get unconfigured ndepth video channels, device supports up to 4 */
	for(i = 0; i < 2; i++) {
	  channel[i] = frOpenChannel(i,device);
		if (!channel[i]) {
			printf("demo: frOpen failed for channel %d on device %d\n",i, device);
			exit(1);			
		}
	}
	

	/* configure the channels as disparity and rt calibrated */
	if(frSetChannelInput(channel[0], FR_CHAN_INPUT_TYPE_DISP)) {
		printf("demo: set INPUT_TYPE failed for device 0\n");
		exit(1);
	}
	if(frSetChannelInput(channel[1], FR_CHAN_INPUT_TYPE_CAL_RT)) {
		printf("demo: set INPUT_TYPE failed for device 1\n");
		exit(1);
	}
	

	/* program calibration remap coords */
	char calibfile[128];
	if(device == 0)
		strcpy(calibfile, "/etc/fr/default.calib");
	else
		sprintf(calibfile, "/etc/fr/default%d.calib", device);
	FrCalibMap *map = frOpenCalibMap(calibfile);
	if(frProgRemapData(channel[0], map)) {
		fprintf(stderr, "RemapCoords failed\n");
		exit(1);
	}
	frCloseCalibMap(map);


	/* create display windows for each channel */
	sprintf(wndname[0], "Disparity");	
	sprintf(wndname[1], "Right Calib");
	cvNamedWindow(wndname[0], CV_WINDOW_AUTOSIZE);
	cvMoveWindow(wndname[0], 0, 0);
	cvNamedWindow(wndname[1], CV_WINDOW_AUTOSIZE);
	cvMoveWindow(wndname[1], 0, 520);

	IplImage *disp = cvCreateImage(cvSize(752, 480), IPL_DEPTH_8U, 1); // FIXME: don't hardcode image size

	/* add status/control interface */
	int frameCount = 0;
	int keypressed;

	FrStatusTool *stat = frOpenStatusTool(channel[0], "Status");
	
	frAddBinMon(stat);
	frAddExposureMon(stat);
	frAddGainMon(stat);
	frAddAEGCMon(stat);
	frAddFPSMon(stat, &frameCount);

	frAddDesiredBinTbar(stat);
	frAddManualExpTbar(stat);
	frAddManualGainTbar(stat);
	frAddMatchQualTbar(stat);
	
	frAddI2CKeypress(stat);

	// frGrabFrame will start the DMA for each channel 
	for (i = 0; i < 2; i++) {
		if(frGrabFrame(channel[i]))
			goto out;
	}		

	/* get and display the images */
	for(;;)	{	

		// frRetreiveFrame will map the DMA buffer to userspace 
		for (i = 0; i < 2; i++) {

			frame[i] = frRetrieveFrame(channel[i]);
			if (NULL == frame) 
				goto out;
		}

		// display the frames
		cvScale(frame[0], disp, 4.0, 0.0);
		cvShowImage(wndname[0], disp);
		cvShowImage(wndname[1], frame[1]);


		// frGrabFrame will start the DMA for each channel 
		for (i = 0; i < 2; i++) {
			if(frGrabFrame(channel[i]))
				goto out;
		}		

		frameCount++;

		// update monitors every 100 frames dma'd
		if(frameCount%10==0) {
		  frUpdateMonitors(stat);
		}

		// check pressed key, if any
		if(-1 == frCheckKeypress(stat, cvWaitKey(2)))
			break;

	}


out:
	frCloseStatusTool(stat);

	/* return ndepth video channels */
	for(i = 0; i < 2; i++) {
		frCloseChannel(channel[i]);
	}

}
