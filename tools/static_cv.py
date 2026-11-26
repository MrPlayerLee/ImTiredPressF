"""Build only the statically linked OpenCV modules used by capture and OCR."""
from pathlib import Path
import os, subprocess, sys, shutil
root=Path(os.environ.get("OPENCV_ROOT",r"E:\OpenCV\opencv\build"))
source=Path(os.environ.get("OPENCV_SOURCE",root.parent/"sources")).resolve()
prefix=Path(os.environ["LOCALAPPDATA"])/"ImTiredPressF"/"opencv-4.12-static-v143"
def main():
    lib=prefix/"lib"
    if all((lib/f"opencv_{m}4120.lib").exists() for m in ("core","imgproc")) and (prefix/"complete-private-config-v2").exists():return
    if not (source/"CMakeLists.txt").exists(): sys.exit("Set OPENCV_SOURCE to the OpenCV 4.12.0 source folder.")
    cmake=shutil.which("cmake") or str(Path(os.environ["VSINSTALLDIR"])/"Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")
    build=prefix/"obj"
    opts={"BUILD_LIST":"core,imgproc","BUILD_SHARED_LIBS":"OFF","BUILD_WITH_STATIC_CRT":"ON","BUILD_TESTS":"OFF",
          "BUILD_PERF_TESTS":"OFF","BUILD_EXAMPLES":"OFF","BUILD_opencv_apps":"OFF","BUILD_JAVA":"OFF",
          "WITH_FFMPEG":"OFF","WITH_IPP":"OFF","WITH_ITT":"OFF","WITH_OPENCL":"OFF","WITH_ADE":"OFF","WITH_LAPACK":"OFF",
          "WITH_EIGEN":"OFF","WITH_OPENMP":"OFF","WITH_TBB":"OFF","PARALLEL_ENABLE_PLUGINS":"OFF",
          "CPU_DISPATCH":"","OPENCV_EXTRA_MODULES_PATH":"","OPENCV_GENERATE_SETUPVARS":"OFF","BUILD_ZLIB":"ON"}
    subprocess.run([cmake,"-S",str(source),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64",*[f"-D{k}={v}" for k,v in opts.items()]],check=True)
    # getBuildInformation() embeds the machine's complete CMake configuration.
    # This generated include is the only input needed by OpenCV's runtime report.
    (build/"modules/core/version_string.inc").write_text('"OpenCV 4.12.0 (static core and imgproc)"\n',encoding="ascii")
    subprocess.run([cmake,"--build",str(build),"--config","Release","--target","opencv_imgproc","--parallel","4"],check=True)
    lib.mkdir(parents=True,exist_ok=True)
    for name in ("opencv_core4120.lib","opencv_imgproc4120.lib","zlib.lib"):
        found=list(build.rglob(name))
        if not found:raise RuntimeError("Static build missing "+name)
        shutil.copy2(found[0],lib/name)
    (prefix/"complete-private-config-v2").write_text("OpenCV 4.12.0 core + imgproc, MT",encoding="ascii")
if __name__=="__main__":main()
