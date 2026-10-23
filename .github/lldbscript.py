import os
import lldb

# Not lldb's own -k: on macOS it skips a crash off the main thread (llvm#58525).
crash_reasons = (lldb.eStopReasonSignal, lldb.eStopReasonException)

def __lldb_init_module(debugger, internal_dict):
    # Otherwise lldb runs the program in a ConPTY, which turns runs of spaces into cursor moves.
    if os.name == "nt":
        os.environ["LLDB_LAUNCH_FLAG_USE_PIPES"] = "1"
    debugger.SetAsync(False)
    debugger.HandleCommand("settings set use-color false")
    debugger.HandleCommand("settings set target.disable-aslr false")
    debugger.HandleCommand("run")
    process = debugger.GetSelectedTarget().GetProcess()
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
