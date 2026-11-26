# ImTiredPressF
Genshin Impact Auto F Clicker. But No more focus.

## Windows x64 설치

[최신 Release](https://github.com/MrPlayerLee/ImTiredPressF/releases/latest)에서 Windows x64 배포 ZIP을 받아 로컬 폴더에 풉니다. 네 파일을 같은 폴더에 둔 상태로 `ImTiredPressF.exe`를 실행합니다. 관리자 권한으로 실행할 설치 폴더는 신뢰할 수 있는 관리자만 수정할 수 있는 위치를 사용합니다.

**업데이트 확인**을 누르면 현재 버전과 서명이 확인된 최신 클라이언트·코어 버전이 표시됩니다. 업데이트할 구성 요소 하나를 선택하고 Windows 권한 요청을 승인합니다. 클라이언트를 선택하면 종료 후 새 버전이 실행됩니다. 코어를 선택하면 클라이언트가 열린 채로 DLL을 언로드하고 교체가 끝나면 서명을 확인해 다시 로드합니다. 두 구성 요소의 업데이트 순서는 자유롭게 선택할 수 있습니다.

## 소스 빌드와 배포

1. 포크 또는 소스 ZIP에서 자체 업데이트를 배포하려면 `BuildConfig.props`의 `RepositoryUrl`에 자신의 GitHub 저장소 주소를 입력합니다.
2. Windows x64에서 Visual Studio C++ v143, Windows SDK, Python 및 OpenCV 4.12.0을 준비하고 `build.bat`을 실행합니다. 일반 사용자용 빌드는 `build.bat --public`으로 만듭니다.
3. 첫 빌드에서 Windows 사용자별 개발 서명 키를 생성합니다. 공개키는 클라이언트와 도우미에 내장되고 `build/obj/ImTiredPressF/Release/release-public.blob`에도 생성됩니다. 자체 서명한 코어는 같은 키를 신뢰하는 자체 클라이언트로 실행합니다.
4. 배포 시 GitHub Releases에 새 태그의 초안을 만들고 `build/release-assets`의 여섯 파일을 **원래 이름 그대로 개별 첨부**합니다. 첫 설치용으로 `build/bin/Version_…` 안의 네 파일을 묶은 ZIP도 첨부할 수 있습니다. 첨부가 끝나면 정식 Release로 게시하고 **Latest**로 지정합니다. 클라이언트는 해당 Release의 서명된 매니페스트를 읽습니다.

[빌드 환경, 서명 방식과 복구 절차](SECURITY_BUILD.md) · [구성 요소별 버전 규칙](VERSION_AGENT.md)
