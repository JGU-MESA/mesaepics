#!../../bin/linux-x86_64/toellner

## You may have to change toellner to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../../db")
epicsEnvSet "P" "$(P=melba_030:)"
epicsEnvSet "R" "$(R=a:)"

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/toellner.dbd"
toellner_registerRecordDeviceDriver pdbbase

< ./iocBoot/$(IOC)/AutoSaveSetup.cmd

#Configure devices
#drvAsynIPPortConfigure("L0","10.32.240.76:4002",0,0,0)
drvAsynIPPortConfigure("L0","10.32.240.66:5025",0,0,0)    #STEAM.AlphaMagnet 
#drvAsynIPPortConfigure("L0","134.93.133.208:5025",0,0,0)

## Debugging mode
#asynSetTraceMask(portName,addr,mask)
#asynSetTraceIOMask(portName,addr,mask)
#addr: In integer specifying the address of the device. For multiDevice ports a value of -1 means the port itself.
#mask: The mask value to set. See the mask bit definitions in asynDriver.h

asynSetTraceIOMask("L0",-1,0x2)
asynSetTraceMask("L0",-1,0x9)
## End Debugging mode

## Load record instances
#dbLoadRecords("db/asynRecord.db","P=schwalb:,R=asyn,PORT=L0,ADDR=24,IMAX=100,OMAX=100")
dbLoadRecords("db/toellner.db", "P=$(P), R=$(R), PORT=L0")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(P)$(R)")


cd "${TOP}/iocBoot/${IOC}"
iocInit

< AutoSaveTask.cmd

## Start any sequence programs
#seq sncxxx,"user=kreidelHost"

