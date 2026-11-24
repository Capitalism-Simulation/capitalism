# Capitalism

돈과 연애 사이에서 7일을 버텨야 하는 B급 코미디 텍스트 어드벤처입니다.
직업(고등학생/대학생/직장인)을 고르고, 하루 세 번(아침·오후·밤) 찾아오는 랜덤 이벤트에서 Y/N으로 선택합니다.
선택에 따라 잔고와 네 사람(빨강·파랑·초록·검정 머리)의 호감도가 바뀌고, 마지막 날 고백 결과로 엔딩이 갈립니다.

## 폴더 구조

```
data/game.json     모든 게임 텍스트와 밸런스 값 (웹판과 콘솔판이 함께 사용)
web/               웹 버전 (index.html, style.css, game.js)
index.html         저장소 루트로 접속하면 web/으로 이동
Header/, Source/   콘솔 버전 엔진 (C++17)
  Json             JSON 읽기
  GameData         game.json -> 구조체 변환과 검증
  Console          출력(타자기 효과), 입력, 화면 지우기
  Game             게임 진행, 이벤트 추첨, 엔딩 판정
Story/             아직 게임에 넣지 않은 스토리 초안
legacy/            리팩터링 전 원본 코드 (빌드에 포함되지 않음)
```

## 웹 버전

빌드 과정이 없는 정적 사이트입니다. 브라우저가 `fetch`로 `data/game.json`을 읽기 때문에 파일을 더블클릭해서 열면 동작하지 않고, 로컬 서버가 필요합니다.

**로컬 실행** (저장소 루트에서)
```
python -m http.server 5173
```
브라우저에서 `http://localhost:5173/web/`을 엽니다.

**GitHub Pages 배포 (자동)**
- `main`에 `web/` 또는 `data/` 변경이 올라가면 `.github/workflows/pages.yml`이 데이터를 검증한 뒤 `web/` + `data/`만 모아 배포합니다.
- 저장소 `Settings > Pages > Source`는 `GitHub Actions`여야 합니다.
- PR과 push마다 `.github/workflows/check.yml`이 데이터 검증(`tools/check_data.py`)과 콘솔판 빌드·자동 플레이를 실행합니다.
- 브랜치에서 바로 배포하는 방식(`Deploy from a branch`)을 쓰면 루트의 `index.html`이 `web/`으로 이동시켜 줍니다.

`web/` 폴더만 따로 배포한다면(예: Vercel에서 루트를 `web`으로 지정) `data/` 폴더를 `web/data/`로 복사해 주세요. 게임은 `data/game.json`과 `../data/game.json`을 차례로 찾습니다.

**기능**
- 타자기 효과 (클릭, Enter, Space로 건너뛰기)
- 키보드 조작: Y/N 선택, 고백 상대는 1~4
- 엔딩 도감: 직업별 13개, 브라우저(localStorage)에 저장
- 결과 화면에서 호감도 공개, 결과 공유(모바일 공유 시트 또는 클립보드), 결과 이미지(PNG) 저장
- 라이트/다크 모드, 모바일 화면 대응
- 테스트용 주소 옵션: `?fast`(타자기 효과 끄기), `?seed=42`(이벤트 순서 고정)

## 콘솔 버전 빌드와 실행

**Visual Studio (CMake)**
1. Visual Studio에서 `파일 > 열기 > 폴더`로 이 폴더를 엽니다.
2. 시작 항목으로 `Capitalism.exe`를 선택하고 실행합니다. 빌드할 때 `data/` 폴더가 실행 파일 옆으로 자동 복사됩니다.

**기존 .vcxproj 프로젝트를 쓸 경우**
- 프로젝트에서 예전 소스 파일을 빼고 `Source/*.cpp`를 추가합니다.
- 추가 포함 디렉터리에 `Header`, C++ 언어 표준에 C++17 이상을 지정합니다.
- 게임은 작업 디렉터리나 실행 파일 위치(상위 폴더 포함)에서 `data/game.json`을 찾습니다.

**실행 옵션**
```
Capitalism.exe --fast          타자기 효과 끄기
Capitalism.exe --seed 42       랜덤 시드 고정 (같은 이벤트 순서 재현)
Capitalism.exe --data 경로     다른 데이터 파일 사용
```

## 이벤트 추가하기

`data/game.json`의 `jobs[].events`에 항목을 추가하면 코드를 고치지 않아도 바로 게임에 나옵니다.

```json
{
  "id": "hs_custom1",
  "slot": "night",
  "phase": "late",
  "text": "본문\n\n선택지 질문?\n",
  "choices": {
    "y": { "text": "Y를 골랐을 때\n", "money": -10000, "affection": [{ "target": 0, "amount": 3 }] },
    "n": { "text": "N을 골랐을 때\n", "money": 0 }
  }
}
```

- `slot`: `morning` / `afternoon` / `night`
- `phase`: `early` (Day1~3, 돈 이벤트) / `late` (Day4~7, 호감도 이벤트)
- `affection.target`: 0 빨강, 1 파랑, 2 초록, 3 검정, `"all"`은 네 명 모두
- 선택지가 없는 이벤트는 `choices` 대신 `"outcome": { "text": "", "money": -20000 }`을 씁니다.
- 같은 판에서 이벤트는 반복되지 않습니다. 해당 칸의 이벤트를 다 쓰면 그때부터 반복을 허용합니다.
- 직업마다 공통 엔딩 내용을 덮어쓸 수 있습니다 (직장인 편 참고). 모두 선택 항목이에요.
  - `confess`: 고백 대사 (연인 수만큼)
  - `endings`: `true` / `noMoney` / `noLove` / `broke` 중 바꿀 것만
  - `trueEnding.minBalance`: 트루 엔딩에 필요한 잔고
- 시작할 때 **플레이어 성별**(남성/여성)과 **상대 캐릭터 성별**(남성/여성/정하지 않음)을 고릅니다. 모든 텍스트에서 아래 표기를 쓸 수 있어요.
  - `{P:남성일 때|여성일 때}`: 플레이어 성별에 따라 바뀜
  - `{L:남성일 때|여성일 때|정하지 않았을 때}`: 상대 캐릭터 성별에 따라 바뀜. 예: `{L:사촌 누나였다|사촌 언니였다|사촌이었다}`
  - `{연상호칭}`처럼 `genderWords`에 등록한 단어: 두 성별 조합에 따라 형/누나/오빠/언니/선배로 바뀜
  - 받침에 따라 조사가 달라지는 경우(`남자친구가` / `애인이`)는 조사까지 선택지에 넣어 주세요.
  - `tools/check_data.py`가 선택지 개수와 등록되지 않은 단어를 검사합니다.
- **난이도** (`difficulties`)
  - `eventPoolRatio`: 각 칸(시간대 × 초반/후반)의 이벤트 중 몇 %를 쓸지. 엔진이 목록을 **호감도 이벤트 → 돈 손해가 큰 이벤트 → 돈 되는 이벤트** 순으로 정렬한 뒤 앞에서부터 자르기 때문에, 어려울수록 돈 되는 이벤트가 빠지고 호감도 이벤트는 항상 남습니다. 파일 안의 순서는 신경 쓰지 않아도 됩니다.
  - `lossMultiplier` / `gainMultiplier`: 손실·수입 배율 (100원 단위 반올림)
  - `minAffectionDelta`: 트루 엔딩에 필요한 호감도에 더할 값
  - `description`: 설정 화면에 보이는 설명
- 직업별 `trueEnding.minBalance` / `trueEnding.minAffection`으로 트루 엔딩 기준을 따로 정할 수 있습니다.

## 리팩터링하면서 고친 버그

- 호감도가 저장되지 않아 트루 엔딩(하트 엔딩)에 도달할 수 없던 문제
- 대학생 편 두 이벤트(공강·카페)에서 호감도 값만 정해 두고 실제로 적용하지 않던 문제
- 이벤트를 다시 뽑을 때마다 `srand(time(NULL))`을 호출해 같은 값이 나오는 동안 프로그램이 멈추던 문제
- 이전 입력의 줄바꿈이 남아 Y/N 질문에서 "잘못된 입력" 메시지가 뜨던 문제
- 엔딩 번호 입력에 문자를 넣으면 무한 반복되던 문제
- 두 헤더에서 `MAX_LENGTH`를 서로 다른 값으로 정의하던 문제
- CP949 인코딩 의존: 이제 텍스트는 모두 UTF-8(`game.json`)이고, 한글 이름 입력도 지원합니다.
