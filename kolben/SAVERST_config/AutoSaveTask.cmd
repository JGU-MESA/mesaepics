#
### Start up the save_restore task and tell it what to do.
# The task is actually named "save_restore".
#
# save positions every five seconds
#create_monitor_set("auto_positions.req", 5, "P=$(P), R=$(R)")
# save other things every thirty seconds
#create_monitor_set("auto_settings.req", 30, "P=$(P), R=$(R)")
#
# Handle autosave ‘commands’ contained in loaded databases.
#Search through the EPICS database (that is, all EPICS records
#loaded into an IOC) for info nodes named ‘autosaveFields’ and
#‘autosaveFields_pass0’; construct lists of PV names from the
#associated info values, and write the PV names to the files
#‘info_settings.req’ and ‘info_positions.req’, respectively.
makeAutosaveFiles()
#
#create_monitor_set("info_positions.req", 5, "P=$(P)")
#create_monitor_set("info_settings.req", 10, "P=$(P)")

create_monitor_set("info_positions.req", 5, "$(K)")
#create_monitor_set("info_positions.req", 5, "$(K_2)")

#create_monitor_set("info_settings.req", 10, "$(K_1)")
#create_monitor_set("info_settings.req", 10, "$(K_2)")
####

