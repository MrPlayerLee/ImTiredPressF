"""Local MCP stdio server for the ImTiredPressF control panel. Python 3.9+, Windows."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import sys
import time

k=c.WinDLL("kernel32",use_last_error=True)
u=c.WinDLL("user32",use_last_error=True)
k.CreateFileW.argtypes=[w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
k.CreateFileW.restype=w.HANDLE
k.CloseHandle.argtypes=[w.HANDLE]
k.ReadFile.argtypes=k.WriteFile.argtypes=[w.HANDLE,c.c_void_p,w.DWORD,c.POINTER(w.DWORD),c.c_void_p]
k.PeekNamedPipe.argtypes=[w.HANDLE,c.c_void_p,w.DWORD,c.c_void_p,c.POINTER(w.DWORD),c.c_void_p]
k.GetNamedPipeServerProcessId.argtypes=[w.HANDLE,c.POINTER(w.ULONG)]
u.FindWindowExW.argtypes=[w.HWND,w.HWND,w.LPCWSTR,w.LPCWSTR]
u.FindWindowExW.restype=w.HWND
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
BUTTONS=["minimize","maximize","close","start","stop","hotkeys","load","unload","check_versions","update_client","update_core"]
def schema(properties, required=()):
    return {"type":"object","properties":properties,"required":list(required),"additionalProperties":False}
TOOLS=[
    {"name":"get_ui","description":"Read Korean labels, button states, slider, core and component versions.","inputSchema":schema({})},
    {"name":"click_button","description":"Click one enabled ImTiredPressF button.","inputSchema":schema({"button":{"type":"string","enum":BUTTONS}},["button"])},
    {"name":"set_overlay_size","description":"Set the overlay slider percentage.","inputSchema":schema({"percent":{"type":"integer","minimum":60,"maximum":200}},["percent"])},
]
def target(selected):
    hwnd=None; matches=[]
    while True:
        hwnd=u.FindWindowExW(None,hwnd,"ImTiredPressFWindow",None)
        if not hwnd: break
        pid=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(pid))
        if selected is None or selected==pid.value: matches.append(pid.value)
    if len(matches)!=1:
        raise ValueError("Expected one running ImTiredPressF window. Found "+str(matches)+". Select a pid if multiple windows are open.")
    return matches[0]

def exchange(pid,command):
    name=r"\\.\pipe\ImTiredPressF.UI."+str(pid)
    deadline=time.monotonic()+3
    while True:
        # Identification only: a server cannot impersonate this bridge to perform actions.
        pipe=k.CreateFileW(name,0x100003,0,None,3,0x00110000,None)
        if pipe!=c.c_void_p(-1).value: break
        err=c.get_last_error()
        if err not in (2,231) or time.monotonic()>deadline: raise c.WinError(err)
        time.sleep(.04)
    try:
        owner=w.ULONG()
        if not k.GetNamedPipeServerProcessId(pipe,c.byref(owner)) or owner.value!=pid: raise ValueError("Unexpected server identity")
        n=w.DWORD()
        if not k.WriteFile(pipe,command,len(command),c.byref(n),None): raise c.WinError(c.get_last_error())
        deadline=time.monotonic()+120
        while True:
            if not k.PeekNamedPipe(pipe,None,0,None,c.byref(n),None): raise c.WinError(c.get_last_error())
            if n.value: break
            if time.monotonic()>deadline: raise TimeoutError("UI response timed out")
            time.sleep(.025)
        if n.value>16384: raise ValueError("Response too large")
        buf=c.create_string_buffer(n.value)
        if not k.ReadFile(pipe,buf,len(buf),c.byref(n),None): raise c.WinError(c.get_last_error())
        return json.loads(buf.raw[:n.value])
    finally: k.CloseHandle(pipe)

def call(name,args,default_pid):
    args=dict(args)
    pid=args.pop("pid",default_pid)
    if pid is not None and (type(pid)!=int or pid<=0): raise ValueError("Invalid PID")
    if name=="get_ui" and not args: cmd="state"
    elif name=="click_button" and set(args)=={"button"} and args["button"] in BUTTONS: cmd="click "+args["button"]
    elif name=="set_overlay_size" and set(args)=={"percent"} and type(args["percent"])==int and 60<=args["percent"]<=200:
        cmd="scale "+str(args["percent"])
    else: raise ValueError("Unknown tool or invalid parameters")
    return exchange(target(pid),cmd.encode("ascii"))

for t in TOOLS: t["inputSchema"]["properties"]["pid"]={"type":"integer","minimum":1,"description":"Optional target when several clients are running."}

def serve(pid):
    for raw in sys.stdin.buffer:
        rid=None
        try:
            request=json.loads(raw); rid=request.get("id"); method=request.get("method")
            if "id" not in request: continue
            if method=="initialize":
                ver=request.get("params",{}).get("protocolVersion","")
                supported=("2025-03-26","2025-06-18","2025-11-25")
                try:
                    ui=exchange(pid if pid is not None else target(None),b"state")
                    client_version=ui.get("running_client_version") or ui["labels"]["client_current"]["text"]
                except (OSError,ValueError,KeyError,TimeoutError):
                    client_version="unattached"
                result={"protocolVersion":ver if ver in supported else supported[-1],"capabilities":{"tools":{}},
                        "instructions":"Use get_ui to read the running client version for the selected PID. If multiple windows are open, specify pid.","serverInfo":{"name":"ImTiredPressF","version":client_version}}
            elif method=="ping": result={}
            elif method=="tools/list": result={"tools":TOOLS}
            elif method=="tools/call":
                try:
                    params=request["params"]; out=call(params["name"],params.get("arguments",{}),pid)
                except (ValueError,KeyError,OSError,TypeError) as exc: out={"ok":False,"error":str(exc)}
                result={"content":[{"type":"text","text":json.dumps(out,ensure_ascii=False)}],"isError":not out.get("ok")}
            else: raise LookupError("Method not found")
            response={"jsonrpc":"2.0","id":rid,"result":result}
        except (ValueError,AttributeError,LookupError,TypeError) as exc:
            response={"jsonrpc":"2.0","id":rid,"error":{"code":-32601 if isinstance(exc,LookupError) else -32600,"message":str(exc)}}
        sys.stdout.buffer.write((json.dumps(response,ensure_ascii=False)+"\n").encode());sys.stdout.buffer.flush()
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--pid",type=int);serve(parser.parse_args().pid)

