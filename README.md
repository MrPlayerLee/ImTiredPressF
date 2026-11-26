# ImTiredPressF
Genshin Impact Auto F Clicker. But No more focus.
Powered by ChatGPT Codex. (GPT-6.2 Astra)

## 일반 사용자 · Windows x64 설치

게임에서 F 입력을 자동화하는 Windows용 앱입니다. 직접 버튼을 눌러 사용하려면 **`ImTiredPressF.exe`**를 실행하세요. **MCP(Model Context Protocol)**는 연결한 AI가 사용자 요청에 따라 앱의 상태를 읽고 시작·중지 같은 버튼을 조작하는 방식입니다. 이 기능을 쓰려면 `ImTiredPressF(MCP).exe`를 실행하세요. MCP판은 허용된 조작마다 앱 안에서 추가 승인을 묻지 않습니다.

1. [Windows x64용 다운로드](https://github.com/MrPlayerLee/ImTiredPressF/releases/latest/download/ImTiredPressF-win-x64.zip)를 눌러 최신 설치 ZIP을 받습니다.
2. ZIP을 우클릭해 **모두 압축 풀기**를 선택하고 로컬 폴더에 풉니다. 버전 1.3.0부터 아래 다섯 파일이 한 폴더에 들어 있습니다. 관리자 권한으로 쓸 설치 폴더는 신뢰하는 관리자만 수정할 수 있는 위치를 선택하세요.
3. 원하는 클라이언트 하나를 실행합니다. 창에서 코어 서명이 확인되면 게임을 열고 **시작**을 누릅니다. **중지**로 멈출 수 있습니다. 실행 방식을 바꿀 때는 현재 클라이언트를 먼저 닫으세요.

| 파일 | 용도 |
| --- | --- |
| `ImTiredPressF.exe` | 일반판. 화면에서 직접 조작하며 MCP 연결 기능은 컴파일에서 제외됩니다. |
| `ImTiredPressF(MCP).exe` | AI 연동판. 같은 Windows 로그온 세션에서 연결한 MCP 도구가 상태 조회, 버튼 조작, 오버레이 크기 조정을 할 수 있습니다. |
| `ImTiredPressF.dll` | 두 클라이언트가 공유하는 게임 감지·입력·오버레이 코어입니다. |
| `ImTiredPressF.dll.sig` | 클라이언트가 DLL을 로드하기 전에 확인하는 서명입니다. DLL과 함께 두세요. |
| `Update_Installer.exe` | 업데이트를 선택하면 클라이언트가 호출하는 설치 도우미입니다. |

### 업데이트

**업데이트 확인**을 눌러 현재 버전과 최신 버전을 비교합니다. **클라이언트 업데이트**는 실행 중인 종류의 EXE를 교체하고 같은 종류로 재시작합니다. 다른 EXE도 업데이트하려면 그 클라이언트를 실행해 같은 절차를 따르세요. **코어 업데이트**는 클라이언트를 열어 둔 채 공용 DLL을 언로드하고 교체한 뒤 서명을 확인해 다시 로드합니다. 한 번에 하나를 선택하며 순서는 자유롭습니다. Windows에서 권한을 요청하면 설치를 계속할지 선택하세요.

기존 1.2.0 일반판은 기존 업데이트 버튼으로 일반판을 갱신할 수 있습니다. MCP판을 처음 추가하려면 두 실행 파일이 든 배포 ZIP을 받으세요. DLL 또는 서명이 없다는 메시지가 뜨면 같은 Release의 ZIP을 새 폴더에 다시 풀고 그 폴더에서 실행하세요.

### Windows 문제를 보고할 때

오버레이 카드와 고정 버튼을 함께 공유하려면 Discord에서 게임이 보이는 **모니터 전체 화면**을 선택하세요. 게임 창만 공유하면 별도 창으로 그린 오버레이가 빠질 수 있습니다. 코어 1.3.4부터는 전체 화면 공유와 `Win+Shift+S` 캡처에 두 창을 포함할 수 있습니다. 로그의 `overlay.capture card`와 `overlay.capture pin`에서 `affinity=0x00000000`은 Windows에 캡처 제한이 설정되지 않았다는 뜻입니다.

클라이언트와 공용 코어를 모두 업데이트한 뒤 오류를 한 번 재현하세요. **Win+R**에서 `%LOCALAPPDATA%\ImTiredPressF`를 열고 `ImTiredPressF.log`를 전달하면 됩니다. 줄 앞의 `Z`는 UTC이고 `local=`은 해당 PC의 현지 시각입니다. 로그는 로컬에 저장되며 스크린샷이나 OCR로 읽은 대화 내용은 기록하지 않습니다.

단축키 등록 오류에는 실제로 실패한 조합(Alt+F 또는 Alt+G)과 Windows 오류 코드가 남습니다. 1409는 해당 조합이 이미 등록되어 있다는 뜻입니다. Alt+G만 등록에 실패하면 Alt+F로 시작과 중지를 모두 할 수 있습니다. 게임 창은 창 제목의 언어와 관계없이 실행 파일을 확인해 찾습니다. 오버레이를 확인하려면 게임을 화면에 표시하고 클라이언트를 열어 둔 상태로 몇 초간 기다리세요. 게임 창 탐색, 숨김 조건, 창 생성 및 그리기 실패가 진단 대상입니다. 같은 렌더링 오류는 로그가 매 프레임 늘지 않도록 반복 기록을 제한합니다.

게임을 실행하고 실제 플레이 화면을 띄웠는데 오버레이가 보이지 않으면 10초 정도 기다린 후 `overlay.search`를 확인하세요. `supported game process not found`는 검사 시점에 이 PC에서 지원하는 게임 실행 파일을 찾지 못했다는 뜻입니다. `game windows excluded`에는 숨겨졌거나 너무 작은 창 등 제외 사유별 개수가 함께 표시됩니다.

### 일반판 실행에 필요한 구성 요소

Windows x64에서는 Release ZIP을 한 폴더에 모두 풀어 사용합니다. OpenCV와 C++ 런타임은 빌드에 포함되며 오버레이 글꼴도 내장되어 있습니다. 일반판을 실행하기 위해 Visual Studio, Python, 별도 글꼴이나 VC++ 재배포 패키지를 설치할 필요는 없습니다. 오버레이는 같은 Windows 데스크톱에 열린 로컬 원신 게임 창(`GenshinImpact.exe` 또는 `YuanShen.exe`)에 표시됩니다.

대화 자동 감지는 Windows 문자 인식(OCR)을 사용합니다. 로그에 `OCR.engine ready`가 있으면 인식 엔진이 준비된 상태입니다. `OCR.no installed recognizer languages` 또는 `OCR.no engine for profile languages`가 나오면 Windows 기본 설정 언어 목록의 언어와 일치하는 OCR 기능을 설치하고 앱을 다시 실행하세요. 한국어 Windows에서 한국어가 기본 설정 언어인 경우 관리자 PowerShell에서 다음으로 확인할 수 있습니다.

```powershell
Get-WindowsCapability -Online -Name 'Language.OCR~~~ko-KR~0.0.1.0'
```

`State`가 `NotPresent`이면 `Add-WindowsCapability -Online -Name 'Language.OCR~~~ko-KR~0.0.1.0'`로 설치합니다. 다른 언어를 사용하는 경우 해당 언어의 OCR 기능을 선택하세요. [Microsoft의 OCR 언어 설치 안내](https://learn.microsoft.com/windows/powertoys/text-extractor#install-an-ocr-language-pack)

## 개발자 · 소스에서 빌드

### 준비

- **Windows x64와 Visual Studio 2022 또는 Build Tools**. Visual Studio Installer에서 **C++를 사용한 데스크톱 개발**을 선택하고 **MSVC v143 x64/x86**, **Windows SDK**, **Windows용 C++ CMake 도구**를 설치합니다. OpenCV 정적 빌드는 Visual Studio 17 2022 CMake 생성기를 사용합니다. [Visual Studio 설치 안내](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170)
- **Python 3.9 이상과 Windows PowerShell**. 터미널에서 `python --version`과 `powershell -NoProfile -Command "Get-Host"`가 실행되는지 확인합니다.
- **OpenCV 4.12.0 SDK와 같은 버전의 소스**. `OPENCV_ROOT`는 `include/opencv2`가 있는 SDK 폴더, `OPENCV_SOURCE`는 최상위 `CMakeLists.txt`가 있는 소스 폴더를 가리킵니다. 기본 SDK 위치는 `E:\OpenCV\opencv\build`이고 기본 소스 위치는 그 옆의 `sources`입니다. [OpenCV 4.12.0 다운로드](https://github.com/opencv/opencv/releases/tag/4.12.0)

### 빌드 및 검증

1. 저장소를 클론하거나 **Code → Download ZIP**으로 소스를 받아 압축을 풉니다. 포크에서 업데이트를 배포하려면 첫 빌드 전에 `BuildConfig.props`의 `RepositoryUrl`을 `https://github.com/<소유자>/<저장소>` 형식으로 바꿉니다.
2. `build.bat`과 솔루션이 있는 폴더에서 PowerShell을 엽니다. OpenCV를 다른 위치에 설치했다면 아래 예시 경로를 자신의 경로로 바꿉니다.

   ```powershell
   $env:OPENCV_ROOT = 'D:\OpenCV\opencv\build'
   $env:OPENCV_SOURCE = 'D:\OpenCV\opencv\sources'
   .\build.bat
   ```

3. 첫 빌드에서는 필요한 OpenCV 모듈을 정적으로 빌드해 사용자 캐시에 저장하고, Windows CNG 키 저장소에 내보내기 금지 개발 키를 생성합니다. 이후 같은 Windows 사용자와 저장소 주소로 빌드하면 같은 키를 재사용합니다. 공개키 사본은 `build/obj/ImTiredPressF/Release/release-public.blob`에 생성됩니다. 다른 개발자의 키로 서명한 DLL은 그 키를 내장한 클라이언트에서 사용합니다.
4. 한 번의 빌드로 일반판과 MCP판, 공용 DLL과 도우미를 생성합니다. 빌드 중에는 앱을 닫아 두세요. 서명·업데이트, 두 클라이언트의 MCP 연결 여부와 중복 실행 경고를 실제 실행으로 검사합니다. 성공하면 `build/bin/Version_<클라이언트 버전>/`에서 두 실행 파일을 볼 수 있습니다. 첫 설치용 ZIP은 `build/ImTiredPressF-win-x64.zip`입니다.

### MCP 개발 연결

`ImTiredPressF(MCP).exe`를 실행한 뒤 사용하는 AI 앱의 **stdio MCP 서버** 설정에서 실행 명령을 `python`으로, 인수를 `-I`와 소스의 `tools/mcp_server.py` 절대 경로로 지정합니다. 연결 후 `get_ui`로 실행 상태를 읽고, `click_button`으로 활성 버튼을 누르거나 `set_overlay_size`로 오버레이 크기를 조정할 수 있습니다. Windows의 UAC 요청은 업데이트 도우미가 별도로 처리합니다.

### GitHub Release 배포

1. [버전 규칙](VERSION_AGENT.md)에 따라 클라이언트와 코어 버전을 관리합니다. 일반판과 MCP판은 하나의 클라이언트 버전을 공유합니다.
2. 변경을 로컬 `main`에 커밋합니다. `origin`과 `BuildConfig.props`가 같은 GitHub 저장소를 가리키고, Git Credential Manager로 해당 저장소에 푸시·Release 게시 권한이 있는 계정에 로그인되어 있어야 합니다.
3. 배포할 새 태그를 지정해 **`.\build.bat --release v1.3.0`**처럼 실행합니다. 이 명령이 빌드·서명·테스트를 마친 뒤 main을 푸시하고, Release 초안에 설치 ZIP 및 자동 업데이트용 아홉 파일을 올립니다. GitHub에 저장된 각 파일의 SHA-256을 확인한 뒤 Latest로 공개합니다. 코어만 변경하는 배포에도 새 태그를 사용합니다.
4. README의 다운로드 주소는 그대로 둡니다. 이전 배포물은 태그별 Release에서 받을 수 있습니다. 포크에서는 README의 다운로드 주소도 자신의 저장소로 바꾸세요.

하드웨어 키를 사용하는 오프라인 서명과 이전 파일 복구 방법은 [빌드·서명 안내](SECURITY_BUILD.md)에 있습니다.
