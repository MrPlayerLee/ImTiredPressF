"""Exercise the real helper protocol with signed local assets, without requesting interactive UAC."""
from ctypes import WinDLL, byref, c_ulonglong, get_last_error
from ctypes import wintypes as w
from pathlib import Path
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

def exercise(work, component):
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        folder = Path(scratch)
        for name in ("manifest", "manifest.sig", "2.dll", "2.dll.sig", "2.exe"):
            shutil.copy2(work / name, folder / name)
        for suffix in ("exe", "dll", "dll.sig"):
            shutil.copy2(work / ("1." + suffix), folder / ("ImTiredPressF." + suffix))
        exe = folder / "ImTiredPressF.exe"
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
            for suffix in ("exe", "dll", "dll.sig"):
                changed = (component == "client") == (suffix == "exe")
                assert (folder / ("ImTiredPressF." + suffix)).read_bytes() == (work / (("2." if changed else "1.") + suffix)).read_bytes()
            assert (folder / "restarted.pid").exists() == (component == "client")
            if component == "dll": assert parent.poll() is None
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
    for component in ("dll", "client"): exercise(work, component)
    print("PASS independent DLL update, client exit before download, replacement and restart.")
