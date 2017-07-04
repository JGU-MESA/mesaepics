#!../../bin/linux-x86_64/IPcomm

## You may have to change IPcomm to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../protocols")

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/IPcomm.dbd"
IPcomm_registerRecordDeviceDriver pdbbase

drvAsynIPPortConfigure("kiste1","134.93.133.208:54321",0,0,0)

## Load record instances
#dbLoadRecords("db/xxx.db","user=epicsHost")
dbLoadTemplate("db/wedls.subs")


cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=epicsHost"

