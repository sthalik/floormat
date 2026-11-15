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
# After target remote, as under qemu-user, the inferior exists and is stopped at its entry point.
attached = gdb.selected_inferior().pid != 0
gdb.execute("set confirm off")
gdb.execute("set pagination off")
if not attached:
    gdb.execute("set disable-randomization on")
gdb.execute("set print thread-events off")
gdb.execute("c" if attached else "r")
gdb.execute("q 1")
