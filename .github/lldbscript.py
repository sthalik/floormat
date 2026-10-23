import os
import lldb

# Not lldb's own -k: on macOS it skips a crash off the main thread (llvm#58525).
crash_reasons = (lldb.eStopReasonSignal, lldb.eStopReasonException)

def __lldb_init_module(debugger, internal_dict):
    debugger.SetAsync(False)
    debugger.HandleCommand("settings set use-color false")
    debugger.HandleCommand("settings set target.disable-aslr false")
    target = debugger.GetSelectedTarget()
    # Not "run": it builds its own launch info and drops these flags.
    info = target.GetLaunchInfo()
    # Otherwise lldb runs the program in a ConPTY, which turns runs of spaces into cursor moves.
    # Not LLDB_LAUNCH_FLAG_USE_PIPES: a static-CRT liblldb.dll never sees os.environ.
    if os.name == "nt":
        info.SetLaunchFlags(info.GetLaunchFlags() | lldb.eLaunchFlagUsePipes)
    error = lldb.SBError()
    process = target.Launch(info, error)
    if not error.Success():
        print(error)
        debugger.HandleCommand("quit 1")
        return
    while process.GetState() == lldb.eStateStopped:
        if any(t.GetStopReason() in crash_reasons for t in process):
            for t in process:
                print(t)
                for f in t:
                    print("   ", f)
            debugger.HandleCommand("quit 2")
            return
        process.Continue()
    exited = process.GetState() == lldb.eStateExited
    debugger.HandleCommand("quit %d" % (process.GetExitStatus() if exited else 1))
