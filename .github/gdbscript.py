def my_signal_handler (event):
  if (isinstance(event, gdb.SignalEvent)):
    gdb.execute("thread apply all bt")
    gdb.execute("q 2")
def my_exit_handler (event):
    if isinstance(event, gdb.ExitedEvent):
        if hasattr(event, 'exit_code'):
            gdb.execute("q " + str(int(event.exit_code)))

gdb.events.stop.connect(my_signal_handler)
gdb.events.exited.connect(my_exit_handler)
gdb.execute("set confirm off")
gdb.execute("set pagination off")
gdb.execute("set disable-randomization off")
gdb.execute("r")
gdb.execute("q 1")
