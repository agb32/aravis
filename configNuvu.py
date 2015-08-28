#darc, the Durham Adaptive optics Real-time Controller.
#Copyright (C) 2013 Alastair Basden.

#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU Affero General Public License as
#published by the Free Software Foundation, either version 3 of the
#License, or (at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU Affero General Public License for more details.

#You should have received a copy of the GNU Affero General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.
#This is a configuration file for CANARY.
#Aim to fill up the control dictionary with values to be used in the RTCS.

#import correlation
import string
import FITS
import tel
import numpy

nacts=54#97#54#+256
ncam=1

ncamThreads=numpy.ones((ncam,),numpy.int32)*1
npxly=numpy.zeros((ncam,),numpy.int32)
npxly[:]=128#134  #Email from nuvu says 134, but aravis says 128!
npxlx=npxly.copy()
npxlx[:]=128#136
nsuby=npxlx.copy()
nsuby[:]=30#for config purposes only... not sent to rtc
nsubx=nsuby.copy()#for config purposes - not sent to rtc
nsub=nsubx*nsuby#This is used by rtc.
nsubaps=nsub.sum()#(nsuby*nsubx).sum()
individualSubapFlag=tel.Pupil(30,15,2,30).subflag.astype("i")
subapFlag=numpy.zeros((nsubaps,),"i")
for i in range(ncam):
    tmp=subapFlag[nsub[:i].sum():nsub[:i+1].sum()]
    tmp.shape=nsuby[i],nsubx[i]
    tmp[:]=individualSubapFlag
#ncents=nsubaps*2
ncents=subapFlag.sum()*2
npxls=(npxly*npxlx).sum()

fakeCCDImage=None

bgImage=None
darkNoise=None
flatField=None

subapLocation=numpy.zeros((nsubaps,6),"i")
nsubapsCum=numpy.zeros((ncam+1,),numpy.int32)
ncentsCum=numpy.zeros((ncam+1,),numpy.int32)
for i in range(ncam):
    nsubapsCum[i+1]=nsubapsCum[i]+nsub[i]
    ncentsCum[i+1]=ncentsCum[i]+subapFlag[nsubapsCum[i]:nsubapsCum[i+1]].sum()*2

# now set up a default subap location array...
#this defines the location of the subapertures.
subx=(npxlx)/nsubx
suby=(npxly)/nsuby
xoff=[0]*ncam
yoff=[0]*ncam
for k in range(ncam):
    for i in range(nsuby[k]):
        for j in range(nsubx[k]):
            indx=nsubapsCum[k]+i*nsubx[k]+j
            subapLocation[indx]=(yoff[k]+i*suby[k],yoff[k]+i*suby[k]+suby[k],1,xoff[k]+j*subx[k],xoff[k]+j*subx[k]+subx[k],1)
print "Max subap extend: ",subapLocation[:,1].max(),subapLocation[:,4].max()
pxlCnt=numpy.zeros((nsubaps,),"i")
# set up the pxlCnt array - number of pixels to wait until each subap is ready.  Here assume identical for each camera.
for k in range(ncam):
    # tot=0#reset for each camera
    for i in range(nsub[k]):
        indx=nsubapsCum[k]+i
        #n=(subapLocation[indx,1]-1)*npxlx[k]+subapLocation[indx,4]
        n=subapLocation[indx,1]*npxlx[k]#whole rows together...
        pxlCnt[indx]=n

#pxlCnt[-5]=128*256
#pxlCnt[-6]=128*256
#pxlCnt[nsubaps/2-5]=128*256
#pxlCnt[nsubaps/2-6]=128*256

#The params are dependent on the interface library used.
"""
  //Parameters are:
  //bpp[ncam]
  //blocksize[ncam]
  //offsetX[ncam]
  //offsetY[ncam]
  //prio[ncam]
  //affinElSize
  //affin[ncam*elsize]
  //length of names (a string with all camera IDs, semicolon separated).
  //The names as a string.
  //recordTimestamp
"""
camList=["Pleora Technologies Inc.-","Imperx, inc.-110240","Imperx, inc.-110323","Imperx, inc.-110324","Imperx, inc.-110325","Imperx, inc.-110525","Imperx, inc.-110526","Imperx, inc.-110527","Imperx, inc.-110528"][:ncam]
camNames=string.join(camList,";")#"Imperx, inc.-110323;Imperx, inc.-110324"
print camNames
while len(camNames)%4!=0:
    camNames+="\0"
namelen=len(camNames)
cameraParams=numpy.zeros((10*ncam+3+(namelen+3)//4,),numpy.int32)
cameraParams[0:ncam]=16#16 bpp
cameraParams[ncam:2*ncam]=2176#block size
cameraParams[2*ncam:3*ncam]=0#x offset
cameraParams[3*ncam:4*ncam]=0#y offset
cameraParams[4*ncam:5*ncam]=npxlx#campxlx
cameraParams[5*ncam:6*ncam]=npxly#campxly
cameraParams[6*ncam:7*ncam]=0#byteswapints
cameraParams[7*ncam:8*ncam]=0#reorder
cameraParams[8*ncam:9*ncam]=50#priority
cameraParams[9*ncam]=1#affin el size
cameraParams[9*ncam+1:10*ncam+1]=-1#affinity
cameraParams[10*ncam+1]=namelen#number of bytes for the name.
cameraParams[10*ncam+2:10*ncam+2+(namelen+3)//4].view("c")[:]=camNames
cameraParams[10*ncam+2+(namelen+3)//4]=0#record timestamp

rmx=numpy.random.random((nacts,ncents)).astype("f")

camCommand="ProgFrameTimeEnable=true;ProgFrameTimeAbs=50000;"


control={
    "switchRequested":0,#this is the only item in a currently active buffer that can be changed...
    "pause":0,
    "go":1,
    "maxClipped":nacts,
    "refCentroids":None,
    "centroidMode":"CoG",#whether data is from cameras or from WPU.
    "windowMode":"basic",
    "thresholdAlgo":1,
    "reconstructMode":"simple",#simple (matrix vector only), truth or open
    "centroidWeight":None,
    "v0":numpy.ones((nacts,),"f")*32768,#v0 from the tomograhpcic algorithm in openloop (see spec)
    "bleedGain":0.0,#0.05,#a gain for the piston bleed...
    "actMax":numpy.ones((nacts,),numpy.uint16)*65535,#4095,#max actuator value
    "actMin":numpy.zeros((nacts,),numpy.uint16),#4095,#max actuator value
    "nacts":nacts,
    "ncam":ncam,
    "nsub":nsub,
    #"nsubx":nsubx,
    "npxly":npxly,
    "npxlx":npxlx,
    "ncamThreads":ncamThreads,
    "pxlCnt":pxlCnt,
    "subapLocation":subapLocation,
    "bgImage":bgImage,
    "darkNoise":darkNoise,
    "closeLoop":1,
    "flatField":flatField,#numpy.random.random((npxls,)).astype("f"),
    "thresholdValue":0.,#could also be an array.
    "powerFactor":1.,#raise pixel values to this power.
    "subapFlag":subapFlag,
    "fakeCCDImage":fakeCCDImage,
    "printTime":0,#whether to print time/Hz
    "rmx":rmx,#numpy.random.random((nacts,ncents)).astype("f"),
    "gain":numpy.ones((nacts,),"f"),
    "E":numpy.zeros((nacts,nacts),"f"),#E from the tomoalgo in openloop.
    "threadAffinity":None,
    "threadPriority":numpy.ones((ncamThreads.sum()+1,),numpy.int32)*10,
    "delay":0,
    "clearErrors":0,
    "camerasOpen":1,
    "camerasFraming":1,
    "cameraName":"libcamAravis.so",#"camfile",
    "cameraParams":cameraParams,
    "mirrorName":"libmirror.so",
    "mirrorParams":None,
    "mirrorOpen":0,
    "frameno":0,
    "switchTime":numpy.zeros((1,),"d")[0],
    "adaptiveWinGain":0.5,
    "corrThreshType":0,
    "corrThresh":0.,
    "corrFFTPattern":None,#correlation.transformPSF(correlationPSF,ncam,npxlx,npxly,nsubx,nsuby,subapLocation),
#    "correlationPSF":correlationPSF,
    "nsubapsTogether":1,
    "nsteps":0,
    "addActuators":0,
    "actuators":None,#(numpy.random.random((3,52))*1000).astype("H"),#None,#an array of actuator values.
    "actSequence":None,#numpy.ones((3,),"i")*1000,
    "recordCents":0,
    "pxlWeight":None,
    "averageImg":0,
    "slopeOpen":1,
    "slopeParams":None,
    "slopeName":"librtcslope.so",
    "actuatorMask":None,
    "averageCent":0,
    "calibrateOpen":1,
    "calibrateName":"librtccalibrate.so",
    "calibrateParams":None,
    "corrPSF":None,
    "centCalData":None,
    "centCalBounds":None,
    "centCalSteps":None,
    "figureOpen":0,
    "figureName":"figureSL240",
    "figureParams":None,
    "reconName":"libreconmvm.so",
    "fluxThreshold":0,
    "printUnused":1,
    "useBrightest":0,
    "figureGain":1,
    "decayFactor":None,#used in libreconmvm.so
    "reconlibOpen":1,
    "maxAdapOffset":0,
    "version":" "*120,
    #"lastActs":numpy.zeros((nacts,),numpy.uint16),
    }
for i in range(ncam):
    control["aravisCmd%d"%i]=camCommand
#control["pxlCnt"][-3:]=npxls#not necessary, but means the RTC reads in all of the pixels... so that the display shows whole image
