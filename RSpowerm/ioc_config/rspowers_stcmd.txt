#!../../bin/linux-x86_64/RSpowerm

## You may have to change RSpowerm to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../protocols")
epicsEnvSet "P" "$(P=RS:)"          # prefix of Rohde&Schwarz Powermeter
epicsEnvSet "R" "$(R=TEST:)"        # Name of Powermeter
epicsEnvSet "S" "$(S=MEEK:)"        # Area, where Sensors are connected
epicsEnvSet "M1" "$(M=S1:)"         # Device whose power measured by sensor A
epicsEnvSet "M2" "$(M=S3:)"         # Device whose power measured by sensor B

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/RSpowerm.dbd"
RSpowerm_registerRecordDeviceDriver pdbbase

##Configure devices
#drvAsynIPPortConfigure("L0","10.32.240.73:4002",0,0,0)
drvAsynIPPortConfigure("L0","134.93.133.208:4002",0,0,0)

asynSetTraceIOMask("L0", 0x6)
asynSetTraceMask("L0", 0x9)

## Load record instances
#dbLoadRecords("db/xxx.db","user=kreidelHost")
dbLoadRecords("db/RSpowerm.db", "P=$(P), R=$(R), PORT=L0")
dbLoadRecords("db/RSpowers.db", "P=$(S), R=$(M1), CHAN=1, PORT=L0")
dbLoadRecords("db/RSpowers.db", "P=$(S), R=$(M2), CHAN=2, PORT=L0")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=kreidelHost"
