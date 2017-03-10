#!../../bin/linux-x86_64/wedler

## You may have to change wedler to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../protocols")

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/wedler.dbd"
wedler_registerRecordDeviceDriver pdbbase
#drvAsynIPPortConfigure(portName, hostInfo, priority, noAutoConnect, noProcessEos)
#hier ist der portName der Rekordname (buna_r5:wedlposu01), beim Toellner ist L0
#drvAsynIPPortConfigure("L0","10.32.240.72:5025",0,0,0) #L0 = LNull

#drvAsynIPPortConfigure("buna_r5:wedlposu01:","134.93.133.215:23",0,0,0) #schwalbapc2
#drvAsynIPPortConfigure("buna_r5:wedlposu01:","10.32.168.216:23",0,0,0)  #wedlerkiste
#drvAsynIPPortConfigure("buna_r5:wedlposu01:","10.32.240.10:23",0,0,0)   #win10-css-1.mesa.kph
drvAsynIPPortConfigure("buna_r5:wedlposu01:","10.32.240.11:23",0,0,0)    #win10-css-2.mesa.kph


## Load record instances
#dbLoadRecords( char * dbfile, char *substitutions)
#dbLoadRecords("db/wedlerkiste.db", "PORT=buna_r5:wedlposu01:")
dbLoadRecords("db/wedlerkiste.db", "WEDLKI=buna_r5:wedlposu01:")
dbLoadTemplate("db/wedls.subs")

cd "${TOP}/iocBoot/${IOC}"
iocInit


## Start any sequence programs
#seq sncxxx,"user=epicsHost"
