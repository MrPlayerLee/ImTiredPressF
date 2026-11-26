"""Check a no-MCP build via the normal native UI and a direct pipe connection attempt."""
from ctypes import WinDLL,c_void_p,c_uint,byref,get_last_error,create_unicode_buffer
from pathlib import Path
import subprocess,sys,time
exe=Path(sys.argv[1]).resolve()/"ImTiredPressF.exe"
si=subprocess.STARTUPINFO();si.dwFlags=1;si.wShowWindow=0
app=subprocess.Popen([str(exe)],startupinfo=si)
u=WinDLL("user32");k=WinDLL("kernel32",use_last_error=True);k.CreateFileW.restype=c_void_p
try:
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        h=0
        while True:
            h=u.FindWindowExW(0,h,"ImTiredPressFWindow",None)
            if not h:break
            pid=c_uint();u.GetWindowThreadProcessId(h,byref(pid))
            if pid.value==app.pid:break
        if h and u.IsWindowEnabled(u.GetDlgItem(h,1002)):break
        time.sleep(.08)
    assert h and u.IsWindowEnabled(u.GetDlgItem(h,1002)),"Signed core did not load"
    pipe=k.CreateFileW(r"\\.\pipe\ImTiredPressF.UI."+str(app.pid),0x100003,0,None,3,0,None)
    assert pipe==c_void_p(-1).value and get_last_error()==2,"MCP endpoint exists"
    u.PostMessageW(h,0x10,0,0);assert app.wait(timeout=10)==0
    print("PASS no-MCP UI loads signed core; named pipe absent.")
finally:
    if app.poll() is None:app.terminate();app.wait()

