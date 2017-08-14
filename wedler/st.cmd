#!../../bin/linux-x86_64/wedler

## You may have to change wedler to something else
## everywhere it appears in this file

< envPaths
#epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../protocols")
epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../../db")
#epicsEnvSet "P" "$(P=melba:wedlposu01:)"
epicsEnvSet "K_1" "$(K_1=melba:wedlposu01:)"
epicsEnvSet "K_2" "$(K_2=buna_r5:wedlposu01:)"

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/wedler.dbd"
wedler_registerRecordDeviceDriver pdbbase

< ./iocBoot/$(IOC)/AutoSaveSetup.cmd

#drvAsynIPPortConfigure(portName, hostInfo, priority, noAutoConnect, noProcessEos)
#drvAsynIPPortConfigure("L0","10.32.240.72:5025",0,0,0) #L0 = LNull

#drvAsynIPPortConfigure("melba:wedlposu01:","10.32.240.10:23",0,0,0)     #win-css1
#drvAsynIPPortConfigure("K_2_PORT","10.32.168.216:23",0,0,0)             #wedlerkiste neue
drvAsynIPPortConfigure("K_1_PORT","10.32.240.74:23",0,0,0)               #wedlerkiste alte melba
#drvAsynIPPortConfigure("buna_r5:wedlposu01:","10.32.240.10:23",0,0,0)   #win10-css-1.mesa.kph
#drvAsynIPPortConfigure("buna_r5:wedlposu01:","10.32.240.11:23",0,0,0)   #win10-css-2.mesa.kph

## Debugging mode
#asynSetTraceMask(portName,addr,mask)
#asynSetTraceIOMask(portName,addr,mask)
#addr: In integer specifying the address of the device. For multiDevice ports a value of -1 means the port itself.
#mask: The mask value to set. See the mask bit definitions in asynDriver.h

#asynSetTraceIOMask("K_1_PORT",-1,0x2)
#asynSetTraceMask("K_1_PORT",-1,0x9)

#asynSetTraceIOMask("K_2_PORT",-1,0x2)
#asynSetTraceMask("K_2_PORT",-1,0x9)
## End Debugging mode

## Load record instances
#dbLoadRecords( char * dbfile, char *substitutions)
#dbLoadRecords("db/wedlerkiste.db", "WEDLKI=buna_r5:wedlposu01:")
#dbLoadRecords("db/wedlerkiste.db", "WEDLKI=$(K_2), PORT=K_2_PORT")
#dbLoadRecords("db/wedlerkiste.db", "WEDLKI=melba:wedlposu01:, PORT=K_1_PORT")
dbLoadRecords("db/wedlerkiste.db", "WEDLKI=$(K_1), PORT=K_1_PORT")

dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(K_1)")
#dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(K_2)")
#dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "$(K_1)")
#dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "$(K_2)")

dbLoadTemplate("db/wedls.subs")

#traceIocInit
cd "${TOP}/iocBoot/${IOC}"
iocInit

< AutoSaveTask.cmd

## Start any sequence programs
#seq sncExample ("elmnt=melba_020:sh, device=melba:wedlposu01")


