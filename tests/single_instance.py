"""Check the actual duplicate-launch dialog and lifetime of both desktop clients."""
import ctypes
from ctypes import wintypes as w
from pathlib import Path
import subprocess
import sys
import time

ui = ctypes.WinDLL("user32")
ui.FindWindowExW.argtypes = (w.HWND, w.HWND, w.LPCWSTR, w.LPCWSTR)
ui.FindWindowExW.restype = w.HWND
ui.GetWindowThreadProcessId.argtypes = (w.HWND, ctypes.POINTER(w.DWORD))
ui.GetWindowTextW.argtypes = (w.HWND, w.LPWSTR, ctypes.c_int)
ui.PostMessageW.argtypes = (w.HWND, w.UINT, w.WPARAM, w.LPARAM)

def window(app, cls):
    end = time.monotonic() + 12
    while time.monotonic() < end and app.poll() is None:
        hwnd = None
        while hwnd := ui.FindWindowExW(None, hwnd, cls, None):
            pid = w.DWORD()
            ui.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == app.pid:
                return hwnd
        time.sleep(.03)
    raise AssertionError(f"{cls} missing in PID {app.pid}")

def check(folder, first, second):
    hidden = subprocess.STARTUPINFO()
    hidden.dwFlags = subprocess.STARTF_USESHOWWINDOW
    hidden.wShowWindow = 0
    apps = []
    try:
        owner = subprocess.Popen([folder/first], startupinfo=hidden)
        apps.append(owner)
        main = window(owner, "ImTiredPressFWindow")
        duplicate = subprocess.Popen([folder/second], startupinfo=hidden)
        apps.append(duplicate)
        dialog = window(duplicate, "#32770")
        label = ui.FindWindowExW(dialog, None, "Static", first+" 가 실행중입니다.")
        assert label, f"Expected warning identifying {first}"
        deadline = time.monotonic() + 8
        while duplicate.poll() is None and time.monotonic() < deadline:
            ui.PostMessageW(dialog, 0x10, 0, 0)
            time.sleep(.1)
        assert duplicate.wait(1) == 1 and owner.poll() is None
        ui.PostMessageW(main, 0x10, 0, 0)
        assert owner.wait(10) == 0
    finally:
        for app in apps:
            if app.poll() is None:
                app.terminate()
                app.wait(5)

if __name__ == "__main__":
    names = ("ImTiredPressF.exe", "ImTiredPressF(MCP).exe")
    for first in names:
        for second in names:
            check(Path(sys.argv[1]).resolve(), first, second)
    print("PASS all four duplicate launch warnings and restart after exit")
