#!../../bin/linux-x86_64/wavefs

## You may have to change wavefs to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet("STREAM_PROTOCOL_PATH", "../protocols")

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/wavefs.dbd"
wavefs_registerRecordDeviceDriver pdbbase

drvAsynIPPortConfigure("SCAN1", "134.93.133.208:54321",0,0,0)

asynSetTraceIOMask("SCAN1", -1, 0x2)
asynSetTraceMask("SCAN1", -1, 0x9)

## Load record instances
dbLoadRecords("db/scanner.db","P=mesa:,R=scan:,DEVICE=SCAN1,N=500")

#dbLoadTemplate("db/wedls.subs")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=epicsHost"

