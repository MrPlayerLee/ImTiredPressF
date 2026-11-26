"""Exercise the real helper protocol with signed local assets, without requesting interactive UAC."""
from ctypes import WinDLL, byref, c_ulonglong, get_last_error
from ctypes import wintypes as w
from pathlib import Path
from contextlib import contextmanager
import shutil
import subprocess
import sys
import tempfile
import time

k = WinDLL("kernel32", use_last_error=True)
u = WinDLL("user32", use_last_error=True)
k.CreateEventW.argtypes = [w.LPVOID, w.BOOL, w.BOOL, w.LPCWSTR]
k.CreateEventW.restype = w.HANDLE
k.GetProcessTimes.argtypes = [w.HANDLE] + [w.LPVOID] * 4
k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
k.CloseHandle.argtypes = [w.HANDLE]
u.FindWindowExW.argtypes = [w.HWND, w.HWND, w.LPCWSTR, w.LPCWSTR]
u.FindWindowExW.restype = w.HWND
u.GetWindowThreadProcessId.argtypes = [w.HWND, w.LPDWORD]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]

def close_window(pid):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        hwnd = None
        while hwnd := u.FindWindowExW(None, hwnd, "ImTiredPressFWindow", None):
            owner = w.DWORD()
            u.GetWindowThreadProcessId(hwnd, byref(owner))
            if owner.value == pid:
                assert u.PostMessageW(hwnd, 0x10, 0, 0)
                return
        time.sleep(.05)
    raise RuntimeError(f"Window missing for test process {pid}")

@contextmanager
def install_folder(work):
    scratch = tempfile.TemporaryDirectory(dir=work)
    try:
        yield Path(scratch.name)
    finally:
        # Windows can briefly keep an image section open after the process exits.
        deadline = time.monotonic() + 5
        while True:
            try:
                scratch.cleanup()
                break
            except OSError as error:
                if error.winerror not in (5, 32, 267) or time.monotonic() >= deadline:
                    raise
                time.sleep(.15)

def exercise(work, component, mcp):
    with install_folder(work) as folder:
        for name in ("manifest", "manifest.sig", "mcp.manifest", "mcp.manifest.sig", "2.mcp.exe", "2.dll", "2.dll.sig", "2.exe"):
            shutil.copy2(work / name, folder / name)
        for suffix in ("exe", "dll", "dll.sig"):
            shutil.copy2(work / ("1." + suffix), folder / ("ImTiredPressF." + suffix))
        shutil.copy2(work / "1.mcp.exe", folder / "ImTiredPressF(MCP).exe")
        exe = folder / ("ImTiredPressF(MCP).exe" if mcp else "ImTiredPressF.exe")
        startup = subprocess.STARTUPINFO()
        startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        parent = subprocess.Popen([exe], cwd=folder, startupinfo=startup)
        helper = event = None
        try:
            times = [c_ulonglong() for _ in range(4)]
            assert k.GetProcessTimes(int(parent._handle), *[byref(val) for val in times])
            name = f"Local\\ImTiredPressF.Update.{parent.pid}.{times[0].value}"
            event = k.CreateEventW(None, True, False, name)
            if not event: raise OSError(get_last_error())
            helper = subprocess.Popen([work / "InstallerFlow.exe", "--type", component, "--pid", str(parent.pid)], cwd=folder)
            assert k.WaitForSingleObject(event, 15000) == 0
            if component == "client":
                assert not (folder / "fetched").exists()
                close_window(parent.pid)
                assert parent.wait(10) == 0
            assert helper.wait(20) == 0
            for suffix, installed in (("exe","ImTiredPressF.exe"),("mcp.exe","ImTiredPressF(MCP).exe"),("dll","ImTiredPressF.dll"),("dll.sig","ImTiredPressF.dll.sig")):
                changed = (component=="client" and exe.name==installed) or (component=="dll" and suffix.startswith("dll"))
                assert (folder/installed).read_bytes() == (work/(("2." if changed else "1.")+suffix)).read_bytes()
            assert (folder / "restarted.pid").exists() == (component == "client")
            if component == "dll":
                assert parent.poll() is None
                close_window(parent.pid)
                assert parent.wait(10) == 0
        finally:
            if helper and helper.poll() is None: helper.kill(); helper.wait(5)
            if parent.poll() is None: parent.terminate(); parent.wait(5)
            if event: k.CloseHandle(event)
            marker = folder / "restarted.pid"
            if marker.exists():
                pid = int(marker.read_text())
                k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
                k.OpenProcess.restype = w.HANDLE
                child = k.OpenProcess(0x100000, False, pid)
                try:
                    close_window(pid)
                    assert child and k.WaitForSingleObject(child, 10000) == 0
                finally:
                    if child: k.CloseHandle(child)

if __name__ == "__main__":
    work = Path(sys.argv[1]).resolve()
    for mcp in (False, True):
        for component in ("dll", "client"): exercise(work, component, mcp)
    print("PASS independent DLL update, client exit before download, replacement and restart.")
