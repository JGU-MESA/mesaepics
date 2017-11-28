#!../../bin/linux-x86_64/serialTest

## You may have to change serialTest to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet("STREAM_PROTOCOL_PATH", ".:../protocols")
epicsEnvSet "P" "$(P=TOE8951)"
epicsEnvSet "R" "$(R=TEST:)"
#
cd "${TOP}"
#
## Register all support components
dbLoadDatabase("dbd/serialTest.dbd",0,0)
serialTest_registerRecordDeviceDriver pdbbase
#
######## autoSaveRestore setup ########
#
#Increase to get more informational messages printed to the console.
save_restoreSet_Debug(0)                    # 0=initially
#
# status-PV prefix, so save_restore can find its status PV's.
#save_restoreSet_status_prefix("xxx:")
save_restoreSet_status_prefix("$(P)$(R)")
#
# In the restore operation, a copy of the save file will be written.  The
# file name can look like "auto_settings.sav.bu", and be overwritten every
# reboot (0), or it can look like "auto_settings.sav_020306-083522" (this is wha
t
# is meant by a dated backup file) and every reboot will write a new copy (1).
save_restoreSet_DatedBackupFiles(1)
#
# sepecify where save files should go
set_savefile_path("$(TOP)/iocBoot/$(IOC)", "autosave")
#
# specify where request files can be found
# current directory
set_requestfile_path("$(TOP)/iocBoot/$(IOC)", "")
#
# Specify what save files should be restored when.
# Up to eight files can be specified for each pass.
# pass0 before record initialization (positions)
# pass1 after  record initialization (settings)
#
set_pass0_restoreFile("auto_positions.sav")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")
#
set_pass0_restoreFile("info_positions.sav")
set_pass0_restoreFile("info_settings.sav")
set_pass1_restoreFile("info_settings.sav")
#
# Number of sequenced backup files (e.g., 'auto_settings.sav0') to write
save_restoreSet_NumSeqFiles(3)             # 3=initially
#
# Time interval between sequenced backups 
save_restoreSet_SeqPeriodInSeconds(600)    #60=initially 
#
# Time between failed .sav-file write and the retry.
save_restoreSet_RetrySeconds(60)           #60=initially

# Ok to retry connecting to PVs whose initial connection attempt failed?
save_restoreSet_CAReconnect(1)
#
######## end autoSaveRestore setup #######
#
##Configure devices
drvAsynIPPortConfigure("L0","134.93.133.250:5025",0,0,0)

## Load record instances
#dbLoadRecords("db/xxx.db","user=epicsHost")
dbLoadRecords("db/asynRecord.db","P=schwalb:,R=asyn,PORT=L0,ADDR=24,IMAX=100,OMA
X=100")
dbLoadRecords("db/toellner.db", "P=$(P), R=$(R), PORT=L0")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(P)$(R)")
#dbLoadRecords("$(AUTOSAVE)/asApp/Db/configMenu.db", "P=$(P)$(R), CONFIG=scan1")
 #configMenu
#
########  IOC Init ########
cd "${TOP}/iocBoot/${IOC}"
iocInit
#
### Start up the save_restore task and tell it what to do.
# The task is actually named "save_restore".
#
# save positions every five seconds
create_monitor_set("auto_positions.req", 5, "P=$(P), R=$(R)")
# save other things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(P), R=$(R)")
#
# Handle autosave 'commands' contained in loaded databases.
#Search through the EPICS database (that is, all EPICS records 
#loaded into an IOC) for info nodes named 'autosaveFields' and
#'autosaveFields_pass0'; construct lists of PV names from the 
#associated info values, and write the PV names to the files 
#'info_settings.req' and 'info_positions.req', respectively. 
makeAutosaveFiles()
#
create_monitor_set("info_positions.req", 5, "P=$(P), R=$(R)")
create_monitor_set("info_settings.req", 30, "P=$(P), R=$(R)")
#
#create_manual_set("scan1Menu.req", "P=$(P)$(R), CONFIG=scan1, CONFIGMENU=1") #c
onfigMenu

## Start any sequence programs
#seq sncxxx,"user=epicsHost"
