from org.csstudio.opibuilder.scriptUtil import PVUtil

value = PVUtil.getDouble(display.getWidget("ramprate_box").getPV())
value2 = display.getWidget("rampchoice_box").getPV()

value2.setValue(value)