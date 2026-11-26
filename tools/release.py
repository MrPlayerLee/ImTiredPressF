"""Publish build.bat's verified files using the configured repository's Git credentials."""
import hashlib
import http.client
import json
import re
import subprocess
import urllib.parse
import xml.etree.ElementTree as ET


class Publisher:
    def __init__(self, root, tag):
        url = ET.parse(root/"BuildConfig.props").findtext(".//{*}RepositoryUrl")
        match = re.fullmatch(r"https://github\.com/([\w.-]+/[\w.-]+)", url or "", re.ASCII)
        if not match or not re.fullmatch(r"v\d+\.\d+\.\d+", tag, re.ASCII):
            raise ValueError("Expected a GitHub repository and a vX.Y.Z release tag")
        self.root, self.tag = root, tag
        self.repo = match[1]
        self.api = "/repos/" + self.repo
        self.head = self.git("rev-parse", "HEAD")
        self.check_checkout()
        self.token = None

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True).strip()

    def check_checkout(self):
        if self.git("branch", "--show-current") != "main" or self.git("status", "--porcelain"):
            raise ValueError("Commit changes on main before publishing")
        if self.git("rev-parse", "HEAD") != self.head:
            raise ValueError("HEAD changed during build")
        if self.git("remote", "get-url", "--push", "origin").removesuffix(".git") != "https://github.com/"+self.repo:
            raise ValueError("origin does not match BuildConfig.props")

    def request(self, method, path, body=None, upload=False):
        if self.token is None:
            query = f"protocol=https\nhost=github.com\npath={self.repo}.git\n\n"
            result = subprocess.run(["git","credential","fill"], cwd=self.root, input=query,
                                    capture_output=True, text=True, check=True)
            self.token = dict(line.split("=",1) for line in result.stdout.splitlines() if "=" in line)["password"]
        headers = {"User-Agent":"ImTiredPressF-release","Authorization":"Bearer "+self.token,
                   "Accept":"application/vnd.github+json"}
        if body is not None:
            headers["Content-Type"] = "application/octet-stream" if upload else "application/json"
            if not upload:
                body = json.dumps(body).encode()
        conn = http.client.HTTPSConnection("uploads.github.com" if upload else "api.github.com", timeout=90)
        try:
            conn.request(method, self.api+path, body, headers)
            response = conn.getresponse()
            data = response.read()
            if not 200 <= response.status < 300:
                raise RuntimeError(f"GitHub {method} {path}: HTTP {response.status}")
            return json.loads(data) if data else None
        finally:
            conn.close()

    def publish(self, paths):
        self.check_checkout()
        releases = self.request("GET","/releases?per_page=100")
        draft = next((r for r in releases if r["tag_name"]==self.tag), None)
        if draft and (not draft["draft"] or draft["target_commitish"]!=self.head):
            raise ValueError("Tag belongs to an existing release")
        if self.git("ls-remote","--tags","origin","refs/tags/"+self.tag):
            raise ValueError("Use a new release tag")
        self.git("push","origin",self.head+":refs/heads/main")
        if not draft:
            draft = self.request("POST","/releases",{
                "tag_name":self.tag, "target_commitish":self.head, "draft":True,
                "name":"ImTiredPressF "+self.tag,
                "body":"Windows x64용 설치 파일은 **ImTiredPressF-win-x64.zip**입니다. 압축을 풀고 일반판은 `ImTiredPressF.exe`, AI 연동판은 `ImTiredPressF(MCP).exe`를 실행하세요. 기존 설치에서는 **업데이트 확인**을 누르세요."})
        path = "/releases/"+str(draft["id"])
        existing = {a["name"]:a for a in self.request("GET",path+"/assets")}
        expected = {}
        if set(existing)-{p.name for p in paths}:
            raise ValueError("Unexpected files in release draft")
        for file in paths:
            payload = file.read_bytes()
            digest = "sha256:"+hashlib.sha256(payload).hexdigest()
            expected[file.name] = digest
            previous = existing.get(file.name)
            if previous and previous.get("digest")==digest:
                continue
            if previous:
                self.request("DELETE","/releases/assets/"+str(previous["id"]))
            self.request("POST",path+"/assets?"+urllib.parse.urlencode({"name":file.name}),payload,upload=True)
        stored = self.request("GET",path+"/assets")
        if {a["name"]:a.get("digest") for a in stored if a["state"]=="uploaded"} != expected:
            raise ValueError("Release asset verification failed")
        published = self.request("PATCH",path,{"draft":False,"make_latest":"true"})
        print("Published",published["html_url"])
