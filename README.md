# Karon

> Unreal Engine 5.7 기반 액션 전투·베이스 빌딩 결합 하이브리드 싱글플레이 게임

**[▶ 시연 영상]([Karon Play Video - YouTube](https://www.youtube.com/watch?v=hIdEdXDC8Ps&list=LL&index=1&t=44s))**

---

## 프로젝트 개요

|     |     |
| --- | --- |
| **기간** | 2026.05 ~ 2026.08 (4개월) |
| **팀 구성** | 8명 (최종 7명) |
| **본인 역할** | 팀장 · 클라이언트(적 AI/전투) · 기획 일부 |
| **엔진 / 언어** | Unreal Engine 5.7 · C++ |
| **주요 기술** | GAS, Behavior Tree, EQS, Motion Warping, Niagara, CommonUI/UMG, GMRouter, Perforce |

> 팀 프로젝트입니다. 본 레포는 원본 저장소의 포크본이며, 아래 "본인 구현 범위"에 명시한 부분이 직접 작성한 코드입니다.

---

## 본인 구현 범위

**[전체 커밋 보기](https://github.com/khs081215/7th-Team5-Final-Project/commits?author=khs081215)**

| 영역  | 주요 파일 |
| --- | --- |
| 적 AI (기반 설계 · BT · EQS · 퍼셉션 · GA) | `Source/Karon/Character/Enemy/AKOBaseEnemy.cpp` |
| 군집 공격 토큰 시스템 | `Source/Karon/Character/Enemy/AKOEnemyCluster.cpp` |
| 패리·반격 메커니즘 (그로기 · 앞잡기 · Motion Warping) | `Source/Karon/Character/Enemy/` |
| 정예 에너미 (조건별 패턴 변화) | `Source/Karon/Character/Enemy/` |
| GMRouter 메시징 플러그인 리팩토링 | `Plugins/GMRouter/Source/` |
| 전투·드랍 텔레메트리 (SQLite 비동기 기록) | `Source/Karon/Subsystem/` |
| 튜토리얼 서브시스템 | `Source/Karon/Subsystem/KOTutorialSubsystem.cpp` |
| 에너미 아이템 드랍 · 드랍 UI | `Source/Karon/Character/Enemy/`, `Source/Karon/UI/` |

> 그 외: 레벨 디자인(설산 마을 · 플랫포머 맵), 레벨 시퀀스(컷신 · 자막 · 엔딩크레딧), 영어 로컬라이제이션 — 팀장으로서 미할당 작업을 맡아 처리했습니다.

---

## 핵심 구현

### 1. 군집 단위 공격 토큰 시스템

**문제**  
각 적 AI가 자기 Blackboard만 보고 독립 판단하여 군집이 동시에 근접 공격, 플레이어가 일방적으로 포위당하는 난이도 왜곡 발생.

**해결**  
`AKOEnemyCluster`에 근·원거리 공격 토큰을 두어 개별 AI의 공격 권한을 중앙에서 조정. 1초 주기 `EvaluateAttackers`가 플레이어와의 거리 제곱 기반 우선순위로 토큰을 배분해, 동시 공격자를 근접 2·원거리 1명으로 상한.

**설계 판단**  
필요한 상위 인원만 뽑으면 되므로 전체 정렬 대신 min-heap top-K 추출(Heapify + K회 Pop)을 선택. 이전 주기 Winners와 diff를 계산해 상태가 바뀐 적에게만 공격 허가/취소 델리게이트를 실행, 매 주기 전체 재브로드캐스트를 제거.

**결과**  
동시 공격자 수가 통제되어 의도한 전투 난이도 유지. 상태 변경 AI에게만 델리게이트 실행으로 불필요한 브로드캐스트 제거.

---

### 2. GMRouter 커스텀 메시징 플러그인 리팩토링

**문제**  
개인적으로 만들어 둔 메시징 플러그인을 팀 프로젝트에 도입. 다만 기존 구조는 구독 해제를 구독자가 직접 호출해야 했고, 해제를 누락한 채 객체가 소멸하면 브로드캐스트 시점에 무효 리스너를 건드려 크래시 위험 존재.

**해결**  
핸들 기반 구독 관리(`FGameplayMessageHandle`, ID로 식별) 도입. GC 완료 콜백으로 소멸된 리스너 정리해 크래시 차단, 브로드캐스트 중 구독 목록 변경에 대비해 스냅샷 순회

**설계 판단**  
기존 구조는 구독 해제를 구독자가 직접 호출해야 했고, 해제를 누락한 채 객체가 소멸하면 브로드캐스트 시점에 무효 리스너를 건드려 크래시 위험이 있었다. 해제 책임이 구독자 쪽에 남아 있는 한 같은 사고가 반복된다고 보고, 구독 시점에 발급한 핸들로 리스너를 식별하고 GC 완료 콜백에서 소멸된 리스너를 시스템이 스스로 정리하도록 전환. 브로드캐스트 도중 구독 상태가 바뀔 수 있어 배열을 직접 순회하는 대신 스냅샷을 떠서 순회하도록 함. 부가로 콘솔 명령 기반 디버그 로깅(`ECVF_Cheat`)을 추가.

**결과**  
구독 해제 크래시 위험 제거. 팀 내 전파 후 UI 팀에서 인터페이스로 추상화해 채택, 팀 전체가 GMRouter를 통해 결합 없이 이벤트 통신하는 구조로 정착.

---

### 3. 전투·드랍 텔레메트리 시스템 (SQLite 비동기 기록)

**문제**  
전투·드랍 데이터를 게임 스레드에서 동기 기록하면 IO 부하로 프레임 히칭 위험.

**해결**  
GMRouter 이벤트를 구독해 값 타입 이벤트를 SPSC 큐에 넣고, `FRunnable` 워커가 0.5초 주기로 트랜잭션 단위 배치 INSERT (PreparedStatement 재사용).

**설계 판단**  
적 밸런싱과 드랍 타이밍을 감각이 아니라 데이터로 검증하려면 수집한 기록을 조건별로 집계·질의할 수 있어야 해서, 로그 파일에 적재만 하는 방식 대신 SQLite에 기록. 생산자가 게임 스레드 하나, 소비자가 워커 하나로 고정되므로 SPSC 큐를 사용. 이벤트마다 개별 커밋 대신 0.5초 주기 트랜잭션 배치로 묶고 PreparedStatement를 재사용. 종료 시 잔여 큐를 flush해 유실을 방지하고, 개발·QA 밸런싱용이므로 `#if !UE_BUILD_SHIPPING`으로 출시 빌드에서 제외.

**결과**  
게임 스레드에서 직접 파일 IO를 수행하지 않도록 분리해, IO 대기로 인한 직접적인 블로킹을 제거. 적 밸런싱을 담당하면서 적·스킬별 피해량과 피격 빈도를 SQL로 정렬해, 어떤 적, 공격이 실제로 위협적인지를 근거로 수치를 조정. 앞서 도입한 공격 토큰 시스템도 일정 시간당 피격 횟수를 비교해 난이도 개선 효과를 확인하고 토큰 수를 다듬는 데 활용. 스키마는 가드로 막힌 타격을 제외하고 HP가 실제 감소한 경우만 기록해, 실피해를 준 공격을 추적하는 데 초점을 둠.

---

## 빌드 및 실행

```
엔진 버전 : Unreal Engine 5.7
필요 항목 : Visual Studio 2022, Perforce (에셋 동기화 시)
```

1. 레포를 클론합니다.
2. `Karon.uproject` 우클릭 → **Generate Visual Studio project files**
3. `Karon.sln` 빌드 후 에디터 실행

> UE 프로젝트 특성상 클론·빌드가 무겁습니다. 시연 영상으로 먼저 확인하시길 권장합니다. 또한 유료 에셋은 별도 공유되지 않습니다.

---

## 참고

- 원본 팀 저장소: [NBcampUnrealTrack/7th-Team5-Final-Project](https://github.com/NBcampUnrealTrack/7th-Team5-Final-Project)
