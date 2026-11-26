"""Exercise the real EXE through MCP, including replacing a previously loaded DLL."""
from pathlib import Path
import ctypes
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

build=Path(sys.argv[1]).resolve()
repo=Path(__file__).resolve().parents[1]
def run():
    with tempfile.TemporaryDirectory() as tmp:
        folder=Path(tmp); exe=folder/"ImTiredPressF.exe"; dll=exe.with_suffix(".dll")
        shutil.copy2(build/exe.name,exe)
        si=subprocess.STARTUPINFO();si.dwFlags=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
        app=subprocess.Popen([str(exe)],cwd=folder,startupinfo=si)
        server=subprocess.Popen([sys.executable,"-I",str(repo/"tools/mcp_server.py"),"--pid",str(app.pid)],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,encoding="utf-8")
        seq=0
        def rpc(method,params):
            nonlocal seq
            seq+=1
            server.stdin.write(json.dumps(dict(jsonrpc="2.0",id=seq,method=method,params=params))+"\n");server.stdin.flush()
            reply=json.loads(server.stdout.readline());assert reply["id"]==seq;return reply["result"]
        def call(name,**args):
            result=rpc("tools/call",{"name":name,"arguments":args})
            return json.loads(result["content"][0]["text"])
        def state(): return call("get_ui")
        def wait(test):
            deadline=time.monotonic()+12
            while time.monotonic()<deadline:
                s=state()
                if s.get("ok") and test(s): return s
                time.sleep(.06)
            raise AssertionError(s)
        try:
            hello=rpc("initialize",{"protocolVersion":"2025-06-18"})
            assert hello["capabilities"]=={"tools":{}}
            server.stdin.write('{"jsonrpc":"2.0","method":"notifications/initialized"}\n');server.stdin.flush()
            assert len(rpc("tools/list",{})["tools"])==3
            s=wait(lambda s:not s["core_loaded"])
            assert hello["serverInfo"]["version"]==s["running_client_version"]==s["labels"]["client_current"]["text"]==".".join(map(str,manifest_version("client")))
            assert not s["buttons"]["start"]["enabled"] and s["buttons"]["load"]["enabled"]
            assert not call("click_button",button="stop")["ok"]
            assert s["signature_status"]=="dll_missing",s["signature_status"]
            side=dll.with_suffix(".dll.sig")
            shutil.copy2(build/dll.name,dll)
            def denied(code):
                reply=call("click_button",button="load")
                assert not reply["ok"] and reply["error"]==code,reply
                status=state()
                assert status["signature_status"]==code and not status["core_loaded"],status
            denied("signature_missing")
            good_sig=(build/side.name).read_bytes()
            side.write_bytes(good_sig[:-1]+bytes([good_sig[-1]^1]))
            denied("untrusted_signature")
            side.write_bytes(good_sig)
            with dll.open("ab") as file: file.write(b"modified")
            denied("integrity")
            # The native button and MCP dispatch the same command handler.
            u=ctypes.windll.user32
            h=0
            while True:
                h=u.FindWindowExW(0,h,"ImTiredPressFWindow",None)
                if not h:raise AssertionError("UI not found")
                pid=ctypes.c_uint();u.GetWindowThreadProcessId(h,ctypes.byref(pid))
                if pid.value==app.pid:break
            u.SendMessageW(u.GetDlgItem(h,1103),0xf5,0,0)
            assert state()["signature_status"]=="integrity" and not state()["core_loaded"]
            shutil.copy2(build/dll.name,dll)
            foreign=Path(sys.argv[2]) if len(sys.argv)>2 else None
            if foreign:
                side.write_bytes(foreign.read_bytes());denied("untrusted_signature");side.write_bytes(good_sig)
            for turn in range(3):
                reply=call("click_button",button="load"); assert reply["ok"],reply
                s=wait(lambda s:s["core_loaded"] and s["buttons"]["start"]["enabled"])
                assert s["signature_status"]=="verified"
                assert s["labels"]["core_current"]["text"]==".".join(map(str,manifest_version("core")))
                assert call("set_overlay_size",percent=125+turn)["overlay_scale"]["value"]==125+turn
                assert call("click_button",button="unload")["ok"]
                s=wait(lambda s:not s["core_loaded"] and not s["unloading"])
                # Opening without share flags proves the DLL has no outstanding image mapping.
                k=ctypes.windll.kernel32
                k.CreateFileW.restype=ctypes.c_void_p
                h=k.CreateFileW(str(dll),0x40000000,0,None,3,0,None)
                assert h!=ctypes.c_void_p(-1).value,"unloaded DLL remains locked"
                k.CloseHandle(ctypes.c_void_p(h))
                dll.unlink();shutil.copy2(build/dll.name,dll)
            reply=call("click_button",button="load"); assert reply["ok"],reply
            assert call("click_button",button="close")["ok"]
            assert app.wait(timeout=15)==0
            print("PASS: stdio MCP discovery, labels and controls, missing core, 3 unload/exclusive-write/replace/reload cycles, loaded shutdown.")
        finally:
            server.stdin.close();server.wait(timeout=4)
            if app.poll() is None: app.terminate();app.wait(timeout=5)
def manifest_version(component):
    sys.path.insert(0,str(repo/"tools"))
    from version import version
    return version((repo/"VERSION_AGENT.md").read_text("utf-8-sig"),component)
if __name__=="__main__": run()

