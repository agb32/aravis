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

#This one does N bobcats, and then the EVT.
#Also, does with square images... ie sub-windows the detectors.

import string
import FITS
import tel
import numpy
try:
    print prefix
except:
    prefix="bob"
    print prefix
nactXinetics=96
nacts=nactXinetics+1024
if prefix[:3]=="bob":
    if len(prefix)>3:
        try:
            ncam=int(prefix[3:])
        except:
            ncam=1
    else:
        ncam=1
else:
    try:
        ncam=int(prefix)
    except:
        ncam=4
ncam+=1 #Add the EVT.
print "Using %d cameras"%ncam
ncamThreads=numpy.ones((ncam,),numpy.int32)*1
ncamThreads[-1]=2
npxly=numpy.zeros((ncam,),numpy.int32)
npxly[:]=488
npxly[-1]=1088#EVT
npxlx=npxly.copy()
npxlx[:]=488#648
npxlx[-1]=1088#2048#EVT
nsuby=npxlx.copy()
nsuby[:]=31#for config purposes only... not sent to rtc
nsuby[-1]=62#the EVT
nsubx=nsuby.copy()#for config purposes - not sent to rtc
nsub=nsubx*nsuby#This is used by rtc.
nsubaps=nsub.sum()#(nsuby*nsubx).sum()
individualSubapFlag=tel.Pupil(31,15.5,2,31).subflag.astype("i")
subapFlag=numpy.zeros((nsubaps,),"i")
for i in range(ncam-1):
    tmp=subapFlag[nsub[:i].sum():nsub[:i+1].sum()]
    tmp.shape=nsuby[i],nsubx[i]
    tmp[:]=individualSubapFlag
#now the EVT
tmp=subapFlag[nsub[:-1].sum():nsub.sum()]
tmp.shape=nsuby[-1],nsubx[-1]
tmp[:31,:31]=individualSubapFlag
tmp[31:,:31]=individualSubapFlag
tmp[:31,31:]=individualSubapFlag
tmp[31:,31:]=individualSubapFlag
#ncents=nsubaps*2
ncents=subapFlag.sum()*2
npxls=(npxly*npxlx).sum()

fakeCCDImage=None#(numpy.random.random((npxls,))*20).astype("i")

bgImage=None#FITS.Read("shimgb1stripped_bg.fits")[1].astype("f")#numpy.zeros((npxls,),"f")
darkNoise=None#FITS.Read("shimgb1stripped_dm.fits")[1].astype("f")
flatField=None#FITS.Read("shimgb1stripped_ff.fits")[1].astype("f")

subapLocation=numpy.zeros((nsubaps,6),"i")
nsubapsCum=numpy.zeros((ncam+1,),numpy.int32)
ncentsCum=numpy.zeros((ncam+1,),numpy.int32)
for i in range(ncam):
    nsubapsCum[i+1]=nsubapsCum[i]+nsub[i]
    ncentsCum[i+1]=ncentsCum[i]+subapFlag[nsubapsCum[i]:nsubapsCum[i+1]].sum()*2

# now set up a default subap location array...
#this defines the location of the subapertures.

subx=numpy.array([14]*ncam)#(npxlx-nsubx*14)/nsubx
suby=numpy.array([14]*ncam)#(npxly-8)/nsuby
xoff=(npxlx-nsubx*subx)/2#[84]*ncam
yoff=(npxly-nsuby*suby)/2#[4]*ncam
#And now the EVT
#k=ncam-1
#subx[k]=(npxlx[k]-1088)/nsubx[k]
#suby[k]=(npxly[k]-128)/nsuby[k]
#xoff[k]=544
#yoff[k]=64
for k in range(ncam):
    for i in range(nsuby[k]):
        for j in range(nsubx[k]):
            indx=nsubapsCum[k]+i*nsubx[k]+j
            subapLocation[indx]=(yoff[k]+i*suby[k],yoff[k]+i*suby[k]+suby[k],1,xoff[k]+j*subx[k],xoff[k]+j*subx[k]+subx[k],1)

pxlCnt=numpy.zeros((nsubaps,),"i")
# set up the pxlCnt array - number of pixels to wait until each subap is ready.  Here assume identical for each camera.
for k in range(ncam):
    # tot=0#reset for each camera
    for i in range(nsub[k]):
        indx=nsubapsCum[k]+i
        #n=(subapLocation[indx,1]-1)*npxlx[k]+subapLocation[indx,4]
        n=subapLocation[indx,1]*npxlx[k]#whole rows together...
        pxlCnt[indx]=n
#set the last pixel counts to full image...
for k in range(ncam):
    sf=subapFlag[nsub[:k].sum():nsub[:k+1].sum()]
    indx=numpy.where(sf==1)[0][-1]
    pxlCnt[indx+nsub[:k].sum()]=npxlx[k]*npxly[k]
#pxlCnt[-13]=1088*2048
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
camList=["Imperx, inc.-110240","Imperx, inc.-110323","Imperx, inc.-110324","Imperx, inc.-110325"][:ncam-1]
camList.append("EVT-20007")
camNames=string.join(camList,";")#"Imperx, inc.-110323;Imperx, inc.-110324"
print camNames
while len(camNames)%4!=0:
    camNames+="\0"
namelen=len(camNames)
cameraParams=numpy.zeros((10*ncam+3+(namelen+3)//4,),numpy.int32)
cameraParams[0:ncam]=8#8 bpp - cam0, cam1
cameraParams[ncam:2*ncam]=15616#20736#block size - 32 rows
cameraParams[2*ncam-1]=65536#EVT block size.
cameraParams[2*ncam:3*ncam]=80#x offset
cameraParams[3*ncam-1]=480#x offset, EVT
cameraParams[3*ncam:4*ncam]=0#y offset
cameraParams[4*ncam:5*ncam]=npxlx#campxlx
cameraParams[5*ncam:6*ncam]=npxly#campxly
cameraParams[6*ncam:7*ncam]=0#byteswapints
cameraParams[7*ncam:8*ncam]=0#reorder
cameraParams[8*ncam:9*ncam]=50#priority
cameraParams[9*ncam]=1#affin el size
cameraParams[9*ncam+1:10*ncam+1]=0xfc0fc0#affinity
cameraParams[10*ncam+1]=namelen#number of bytes for the name.
cameraParams[10*ncam+2:10*ncam+2+(namelen+3)//4].view("c")[:]=camNames
cameraParams[10*ncam+2+(namelen+3)//4]=0#record timestamp

rmx=numpy.random.random((nacts,ncents)).astype("f")

bobcamCommand="ProgFrameTimeEnable=true;ProgFrameTimeAbs=50000;"
evtcamCommand="FrameRate=20;"
print ncents,nacts


host="10.0.2.10"
while len(host)%4!=0:
    host+='\0'
mirrorName="libmirrorPdAO32Socket.so"
mirrorParams=numpy.zeros((7+len(host)//4,),"i")
mirrorParams[0]=1#affin elsize
mirrorParams[1]=1#priority
mirrorParams[2]=-1#affinity
mirrorParams[3]=0#timeout
mirrorParams[4]=4288#port on receiver
mirrorParams[5]=0#send prefix
mirrorParams[6]=0#as float
mirrorParams[7:]=numpy.fromstring(host,dtype="i")
actInit=None#numpy.ones((96,),numpy.uint16)*32768
actMin=numpy.zeros((nacts,),numpy.uint16)
actMin[:nactXinetics]=32768#don't let xinetics go below 0V.
actMax=numpy.ones((nacts,),numpy.uint16)*65535
actOffset=None
actMapping=None
actSource=None
actScale=None
actPower=None



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
    "actMax":actMax,
    "actMin":actMin,
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
    "mirrorName":"libmirrorPdAO32Socket.so",
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
    "actInit":actInit,
    "actMapping":actMapping,
    "actSource":actSource,
    "actOffset":actOffset,
    "actPower":actPower,
    "actScale":actScale,
    "nactInitPdao32":None,
    "nactPdao32":nactXinetics,
    }
for i in range(ncam-1):
    control["aravisCmd%d"%i]=bobcamCommand
control["aravisCmd%d"%(ncam-1)]=evtcamCommand
#control["pxlCnt"][-3:]=npxls#not necessary, but means the RTC reads in all of the pixels... so that the display shows whole image
