"""Build, sign, test and package the native client and core on Windows."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import msvcrt
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build"
BIN=ROOT/"x64/Release"
FILES=("ImTiredPressF.exe","ImTiredPressF.dll","ImTiredPressF.dll.sig","Update_Installer.exe")
ASSETS=(*FILES,"ImTiredPressF.manifest","ImTiredPressF.manifest.sig")

def run(*args,cwd=ROOT):
    subprocess.run([str(a) for a in args],cwd=cwd,check=True)

def ps(script,*args):
    run("powershell","-NoProfile","-ExecutionPolicy","Bypass","-File",ROOT/script,*args)

def publish(source,dest,names,strict=False):
    dest.mkdir(parents=True,exist_ok=True)
    if strict and {p.name for p in dest.iterdir()}-set(names):
        raise ValueError(f"Unexpected files in distribution directory: {dest}")
    for name in names:
        src=source/name; target=dest/name
        if target.exists() and src.read_bytes()==target.read_bytes(): continue
        with tempfile.NamedTemporaryFile(dir=dest,delete=False) as f: temp=Path(f.name)
        try:
            shutil.copy2(src,temp);os.replace(temp,target)
        finally:temp.unlink(missing_ok=True)

def check_installer(folder):
    # Read the client as resource data without running any code from the image.
    api=ctypes.WinDLL("kernel32",use_last_error=True)
    api.LoadLibraryExW.argtypes=[wintypes.LPCWSTR,wintypes.HANDLE,wintypes.DWORD]
    api.LoadLibraryExW.restype=wintypes.HMODULE
    api.FindResourceW.argtypes=[wintypes.HMODULE,ctypes.c_void_p,ctypes.c_void_p]
    api.FindResourceW.restype=wintypes.HANDLE
    api.LoadResource.argtypes=[wintypes.HMODULE,wintypes.HANDLE]
    api.LoadResource.restype=wintypes.HANDLE
    api.LockResource.argtypes=[wintypes.HANDLE]
    api.LockResource.restype=ctypes.c_void_p
    api.SizeofResource.argtypes=[wintypes.HMODULE,wintypes.HANDLE]
    api.FreeLibrary.argtypes=[wintypes.HMODULE]
    module=api.LoadLibraryExW(str(folder/FILES[0]),None,0x22)
    if not module:raise ctypes.WinError(ctypes.get_last_error())
    try:
        res=api.FindResourceW(module,310,10)
        length=api.SizeofResource(module,res) if res else 0
        address=api.LockResource(api.LoadResource(module,res)) if length else None
        if not address or ctypes.string_at(address,length)!=(folder/"Update_Installer.exe").read_bytes():
            raise ValueError("Packaged updater differs from the authenticated client resource")
    finally:api.FreeLibrary(module)

def audit(folder,mcp=None):
    check_installer(folder)
    for name in (*FILES[:2],FILES[3]):
        imports=subprocess.check_output(["dumpbin","/nologo","/imports",folder/name]).decode(errors="replace").lower()
        forbidden=("opencv_world","msvcp140","vcruntime140","ncryptsignhash","bcryptsignhash","ncryptexportkey")
        if any(word in imports for word in forbidden):raise ValueError(f"Unexpected import in {name}")
        payload=(folder/name).read_bytes()
        if b"-----BEGIN PRIVATE KEY-----" in payload or b"-----BEGIN EC PRIVATE KEY-----" in payload or re.search(rb"(?:gh[pousr]_[A-Za-z0-9]{32,}|github_pat_[A-Za-z0-9_]{50,})",payload):
            raise ValueError(f"Credential marker in {name}")
        if name==FILES[0] and mcp is not None:
            if ("createnamedpipew" in imports)!=mcp:raise ValueError("MCP compile-time exclusion failed")
            if not mcp and "ImTiredPressF.UI.".encode("utf-16le") in payload:raise ValueError("MCP endpoint retained")
    if (folder/FILES[2]).stat().st_size!=104:raise ValueError("Invalid detached signature size")

def msbuild(project,*extra):
    run("msbuild",project,"/m","/nologo","/v:minimal","/p:Configuration=Release","/p:Platform=x64",*extra)

def tests(binaries,enabled):
    work=BUILD/"security-tests";work.mkdir(parents=True,exist_ok=True)
    sdk=Path(os.environ.get("OPENCV_ROOT",r"E:\OpenCV\opencv\build"))
    lib=Path(os.environ["LOCALAPPDATA"])/"ImTiredPressF/opencv-4.12-static-v143/lib"
    ps("tests/key_reuse.ps1",work)
    ps("tests/sign_fixtures.ps1",work,binaries)
    def native(name,sources,link=(),includes=(),args=()):
        exe=work/(name+".exe")
        run("cl","/nologo","/std:c++17","/EHsc","/MT","/DUNICODE","/D_UNICODE","/DNOMINMAX","/wd4819",
            *["/I"+str(i) for i in includes],*[ROOT/s for s in sources],"/Fe:"+str(exe),"/link",*link,cwd=work)
        run(exe,*args,cwd=work)
    native("VersionFeedTests",["tests/VersionFeedTests.cpp"],args=[ROOT/"VERSION_AGENT.md"])
    native("FocusControllerTests",["tests/FocusControllerTests.cpp"],["user32.lib"])
    cv=["/LIBPATH:"+str(lib),"opencv_imgproc4120.lib","opencv_core4120.lib","zlib.lib","advapi32.lib","ole32.lib","oleaut32.lib"]
    overlay_resources=work/"OverlayPreview.res"
    run("rc","/nologo","/fo",overlay_resources,"PanelResources.rc",cwd=ROOT/"client/assets")
    native("OverlayTests",["tests/OverlayTests.cpp"],[overlay_resources,"user32.lib","gdi32.lib","gdiplus.lib"],[sdk/"include"],[work])
    native("PipelineTests",["tests/PipelineTests.cpp","core/Macro.cpp","core/Overlay.cpp","core/Vision_Init.cpp","core/Vision_Update.cpp"],
           ["user32.lib","gdiplus.lib","gdi32.lib","runtimeobject.lib","windowsapp.lib",*cv],[sdk/"include"])
    native("UpdateTrustTests",["tests/UpdateTrustTests.cpp","client/Trust.cpp"],["bcrypt.lib","version.lib","user32.lib"],[work],[work])
    run("cl","/nologo","/std:c++17","/EHsc","/MT","/DUNICODE","/D_UNICODE","/DNOMINMAX","/utf-8",
        "/I"+str(work),ROOT/"installer/main.cpp",ROOT/"tests/InstallerFixture.cpp",ROOT/"client/Trust.cpp",
        "/Fe:InstallerFlow.exe","/link","/subsystem:windows","bcrypt.lib","version.lib","user32.lib","shell32.lib",cwd=work)
    run(sys.executable,ROOT/"tests/installer_flow.py",work)
    if enabled:run(sys.executable,ROOT/"tests/client_lifecycle.py",binaries,work/"foreign.sig")
    else:
        # Exercise the four-file installation layout in a fresh user directory.
        with tempfile.TemporaryDirectory() as install:
            package=Path(install)
            publish(binaries,package,FILES,True)
            run(sys.executable,ROOT/"tests/no_mcp.py",package)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--version",metavar="X.Y.Z",help="publish the signed files to build/bin/Version_X.Y.Z")
    parser.add_argument("--official-public",type=Path)
    parser.add_argument("--request",action="store_true",help="build a public-key-only offline signing request")
    parser.add_argument("--finalize",type=Path,help="verify and package a returned, hardware-signed release directory")
    parser.add_argument("--public",action="store_true",help="package a client with the local MCP bridge disabled")
    parser.add_argument("--validate-all",action="store_true",help="add a separately isolated build with MCP disabled")
    args=parser.parse_args()
    if args.version is not None and not re.fullmatch(r"(?:0|[1-9][0-9]?)(?:\.(?:0|[1-9][0-9]?)){2}",args.version):
        parser.error("--version requires three numbers from 0 to 99 without leading zeros")
    if args.request and args.version is not None:
        parser.error("--version names a finished distribution; use it with --finalize for official builds")
    if bool(args.official_public)!=bool(args.request or args.finalize) or (args.request and args.finalize):parser.error("Use --official-public with exactly one of --request / --finalize.")
    enabled=ET.parse(ROOT/"BuildConfig.props").findtext(".//{*}EnableMcp").strip().lower()=="true"
    if args.public:enabled=False
    pub=["-PublicFile",args.official_public.resolve()] if args.official_public else []
    BUILD.mkdir(exist_ok=True)
    with (BUILD/"security-build.lock").open("a+b") as lock:
        lock.seek(0);msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
        output=BIN
        if args.finalize:
            output=args.finalize.resolve()
            ps("tools/sign.ps1","verify",output,*pub)
        else:
            extra=["/p:ReleasePublicKey="+str(args.official_public.resolve())] if pub else []
            if args.public:extra.append("/p:EnableMcp=false")
            msbuild(ROOT/"ImTiredPressF.sln",*extra)
            if args.request:
                dest=BUILD/"signing-request";publish(output,dest,(*FILES[:2],FILES[3]),True)
                print("Unsigned request for the separate hardware signer:",dest);return
            ps("tools/sign.ps1","release",output)
            ps("tools/sign.ps1","verify",output)
        audit(output,enabled)
        tests(output,enabled)
        if args.validate_all:
            off=BUILD/"mcp-disabled";obj=BUILD/"obj/no-mcp"
            msbuild(ROOT/"client/ImTiredPressF.vcxproj","/p:EnableMcp=false","/p:OutDir="+str(off)+os.sep,
                    "/p:IntDir="+str(obj)+os.sep,*(["/p:ReleasePublicKey="+str(args.official_public.resolve())] if pub else []))
            publish(output,off,FILES[1:]);audit(off,False);run(sys.executable,ROOT/"tests/no_mcp.py",off)
        publish(output,BUILD/"release-assets",ASSETS,True)
        from version import version,manifest
        versions=manifest.read_text("utf-8-sig")
        client,core=(".".join(map(str,version(versions,c))) for c in ("client","core"))
        package_version=args.version or (client+("_Core_"+core if core!=client else ""))
        folder=BUILD/"bin"/("Version_"+package_version)
        publish(output,folder,FILES,True)
        print("Signed distribution:",folder)
if __name__=="__main__":main()

