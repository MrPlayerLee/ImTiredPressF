# 빌드와 서명

Windows x64에서 `build.bat`을 실행합니다. 기존 Visual Studio C++ v143, Windows SDK, Python 3.9 이상, Windows PowerShell과 OpenCV 4.12.0을 사용합니다. 첫 빌드는 Visual Studio의 CMake로 OpenCV core와 imgproc를 정적으로 빌드합니다. OpenCV의 헤더·SDK 위치는 `OPENCV_ROOT`, 소스 위치는 `OPENCV_SOURCE`로 지정할 수 있습니다. 기본 소스 위치는 SDK 폴더와 같은 수준의 `sources`입니다.

| 위치 | 내용 |
| --- | --- |
| `BuildConfig.props` | 포크의 GitHub 주소. build.bat은 일반판과 MCP판을 모두 생성 |
| `VERSION_AGENT.md` | 클라이언트와 코어의 독립 버전 및 변경 규칙 |
| `build/bin/Version_<client>` | 일반판·MCP판 EXE, 공용 코어, DLL 서명, Update_Installer.exe |
| `build/release-assets` | GitHub Release에 첨부할 두 EXE, 공용 DLL과 서명, 도우미 및 종류별 매니페스트·서명 (총 9개) |
| `build/ImTiredPressF-win-x64.zip` | 첫 설치용 다섯 파일 |

개발용 개인키는 Windows 사용자 CNG 키 저장소에 내보내기 금지로 생성됩니다. 같은 Windows 사용자와 저장소 주소로 재빌드하면 기존 키를 재사용합니다. 공개키만 EXE에 포함됩니다. 키 저장소를 잃거나 다른 키로 변경하면 기존 사용자에게 신뢰할 수 있는 방법으로 새 EXE를 전달해야 합니다. 개발 계정에서 실행되는 다른 프로그램도 계정 권한으로 개발용 서명을 요청할 수 있습니다.

## 공식 배포

공식 P-256 키는 별도 보호된 서명 PC의 Microsoft Smart Card Key Storage Provider에 미리 발급합니다. 개발 PC에는 72바이트 CNG ECCPUBLICBLOB 형식의 공개키만 복사합니다. 스마트카드와 PIN은 서명 담당자가 관리합니다.

1. 빌드 PC에서 `build.bat --official-public release-public.blob --request`를 실행합니다. `build/signing-request`의 네 바이너리를 보호된 PC에 전달합니다.
2. 담당자가 검토·승인한 소스와 바이너리인지 확인합니다. 보호된 PC의 같은 소스 버전에서 `powershell -NoProfile -File tools/sign.ps1 offline X:\request -PublicFile X:\release-public.blob -SmartCardKey "컨테이너 이름"`을 실행합니다.
3. 도우미를 포함한 아홉 파일을 받아 `build.bat --official-public release-public.blob --finalize X:\request`를 실행합니다. 공개키 검증, 빌드 설정에 맞는 통합 검사, 패키징이 성공하면 배포할 수 있습니다. 릴리스 게시는 수동입니다.

## 클라이언트의 검증

설치는 로컬 고정 드라이브의 일반 디렉터리를 사용합니다. 관리자 권한으로 사용할 배포본은 관리자만 쓸 수 있는 설치 경로에서 실행합니다. 최초 EXE의 출처와 설치 디렉터리 접근 권한이 신뢰의 출발점입니다. 검증기 자체를 교체할 수 있는 계정이나 서명 권한을 가진 계정은 이 구조의 보호 범위를 벗어납니다.

시작·로드 버튼·MCP가 같은 검증 함수를 이용합니다. 클라이언트가 DLL과 서명을 읽고, SHA-256 해시 및 ECDSA P-256 서명과 최소 코어 버전을 확인합니다. 검증부터 로드까지 파일과 상위 디렉터리 핸들을 유지하고, 재분석 지점과 다중 하드링크 파일은 거부합니다. 코어는 시스템 디렉터리에서 종속 라이브러리를 찾습니다. 코어 언로드가 완료되면 DLL과 서명을 함께 교체할 수 있습니다.

MCP 활성 빌드는 같은 로컬 로그온 세션에서 일반 무결성 수준 이상의 프로세스가 허용된 버튼과 슬라이더를 확인창 없이 조작할 수 있습니다. DLL 경로와 신뢰 설정은 빌드에 고정됩니다. MCP 비활성 빌드는 브리지 번역 단위를 컴파일에서 제외합니다.

## 업데이트와 복구

클라이언트는 설정한 저장소의 최신 Release에서 매니페스트와 서명을 가져옵니다. 서명은 저장소 주소, 양쪽 버전, EXE·DLL의 SHA-256을 함께 인증합니다. 클라이언트 업데이트는 이 서명으로 인증한 EXE 해시를 확인합니다. 코어 업데이트는 DLL 자체의 분리 서명도 확인합니다. PE 버전, 매니페스트 버전, 설치 버전을 비교한 뒤에만 교체합니다.

클라이언트는 선택된 구성 요소 하나에 대해 `Update_Installer.exe --type client` 또는 `--type dll`을 `runas`로 실행하고 자신의 PID를 전달합니다. 도우미에는 독립 버전이 없습니다. 클라이언트의 인증된 리소스와 바이트가 같은 도우미만 실행합니다. 이전 설치에 도우미가 없거나 오래된 경우에는 내장 사본을 사용자 캐시에서 실행합니다. 권한 상승이 취소되면 기존 클라이언트가 계속 실행됩니다.

도우미는 호출한 프로세스와 설치 폴더를 확인하고 한 번에 하나의 업데이트만 허용합니다. 클라이언트 교체는 기존 프로세스가 종료된 뒤 진행되며 이전 실행 파일은 일반판의 경우 `ImTiredPressF.previous.exe`, MCP판의 경우 `ImTiredPressF(MCP).previous.exe`로 남습니다. 새 파일의 해시를 다시 확인한 뒤 관리자 실행을 요청합니다. UAC 창 표시는 현재 프로세스 권한과 Windows 정책에 따릅니다. 재시작이 취소되면 설치 폴더에서 직접 실행할 수 있습니다.

코어 교체 중에는 이전 및 새 DLL에 유효한 서명을 함께 보관합니다. 도우미 종료 후 클라이언트가 설치된 DLL의 서명과 ABI를 확인하고 로드합니다. 새 버전 초기화가 실패하면 클라이언트를 종료하고 `ImTiredPressF.previous.dll`과 `ImTiredPressF.previous.dll.sig`를 기본 파일명으로 되돌릴 수 있습니다. ABI 1의 최소 코어 버전은 1.2.0입니다. 호환 가능한 클라이언트·코어는 서로 다른 버전으로 설치할 수 있습니다.

온라인 릴리스가 없어도 로컬 DLL 검증과 실행은 가능합니다. 사설 서명은 앱 내부의 신뢰 검증이며 Windows의 SmartScreen 평판이나 Authenticode 게시자 표시는 별도입니다.
