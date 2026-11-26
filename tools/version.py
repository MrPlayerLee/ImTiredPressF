import argparse
from pathlib import Path
import re
import sys
import os
import tempfile

manifest = Path(__file__).resolve().parents[1] / "VERSION_AGENT.md"
parts = ("major", "feature", "hotfix")

def version(text, component):
    field = r"(?:0|[1-9][0-9]?)"
    found = re.findall(r"^" + component + r" = (" + field + r"\." + field + r"\." + field + r")$", text, re.M)
    if len(found) != 1:
        raise ValueError("Missing, duplicate or invalid version for " + component)
    return tuple(map(int, found[0].split(".")))

def next_version(current, kind):
    index = parts.index(kind)
    if 99 in current[1:] or current[index] >= 99:
        raise ValueError("Version limit reached. Stop work and obtain an explicit version decision.")
    return (*current[:index], current[index] + 1, *(0 for _ in current[index + 1:]))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("check","bump","generate"))
    parser.add_argument("component", choices=("client","core"))
    parser.add_argument("value")
    args = parser.parse_args()
    # Serialize writers to the shared manifest; an interrupted writer never truncates it.
    lock = manifest.with_suffix(".lock")
    if args.action == "bump":
        os.close(os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
    try:
        text = manifest.read_text("utf-8-sig")
        versions = {c: version(text,c) for c in ("client","core")}
        current = versions[args.component]
        dotted = ".".join(map(str,current))
        if args.action == "generate":
            out = Path(args.value)
            out.mkdir(parents=True, exist_ok=True)
            ext = "exe" if args.component=="client" else "dll"
            fields = ",".join(map(str,(*current,0)))
            rc = f'''#include <winver.h>
1 VERSIONINFO
FILEVERSION {fields}
PRODUCTVERSION {fields}
FILEOS VOS_NT_WINDOWS32
FILETYPE {"VFT_APP" if ext=="exe" else "VFT_DLL"}
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904B0"
  BEGIN
   VALUE "FileVersion", "{dotted}\\0"
   VALUE "ProductVersion", "{dotted}\\0"
   VALUE "OriginalFilename", "ImTiredPressF.{ext}\\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x0409, 1200
 END
END
'''
            dest=out/"version.rc"
            if not dest.exists() or dest.read_text("ascii")!=rc: dest.write_text(rc,"ascii")
            return
        if args.value not in parts: raise ValueError("Expected major, feature or hotfix")
        nxt = ".".join(map(str,next_version(current,args.value)))
        if args.action=="bump":
            updated = re.sub("^"+args.component+r" = [0-9.]+$",args.component+" = "+nxt,text,flags=re.M)
            with tempfile.NamedTemporaryFile(mode="w",encoding="utf-8",dir=manifest.parent,delete=False) as f:
                f.write(updated)
            os.replace(f.name,manifest)
        print(f"{args.component} {dotted} -> {nxt}")
    finally:
        if args.action=="bump": lock.unlink()
if __name__=="__main__":
    try: main()
    except (ValueError,OSError) as e: sys.exit(str(e))
