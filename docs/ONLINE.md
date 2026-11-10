# Putting the console online

The console was built to run on your own machine. It can also run on a host such as Railway, in **online mode**, so the
planning pages (Designer, Direction, Planner, Decisions, Presets, Patterns, Agents, Docs) are reachable from anywhere.

Status: online mode is implemented and tested locally (password check, blocked actions). It has **not** been deployed to
Railway from here; treat the Railway steps as a checklist to follow, not a record of a working deployment.

## What online mode changes

Start it with `tools/sfa console --online` (or `SFA_ONLINE=1`).

| | Local | Online |
|---|---|---|
| Listens on | 127.0.0.1:8590 | 0.0.0.0:`$PORT` |
| Who gets in | this machine only (Host check) | anyone with the password (`SFA_PASSWORD`, HTTP Basic, any user name) |
| Sim, render, report, flash | yes | **off** (no toolchain, no USB) |
| USB check, send kit to a unit, flash log | yes | **off** |
| Designer, Planner, Decisions, Presets, Patterns | saved to `hardware/prototype/plan.json` | saved to `$SFA_DATA/plan.json` |
| Analyzer, Sample Lab | yes | yes, in the browser (drop files in; nothing is uploaded) |
| Agents | review always; chat with `ANTHROPIC_API_KEY` | the same |

It refuses to start online without an `SFA_PASSWORD` of at least 8 characters.

## Environment variables

| Name | Needed | What |
|---|---|---|
| `SFA_ONLINE` | yes (or `--online`) | `1` |
| `SFA_PASSWORD` | yes | the password for the whole console |
| `PORT` | set by the host | the port to listen on |
| `SFA_DATA` | recommended | a folder on a persistent volume, e.g. `/data`. Without it, edits live in the container and are lost on redeploy |
| `ANTHROPIC_API_KEY` | optional | turns agent chat on; usage is billed to that key |
| `SFA_AGENT_MODEL` | optional | the model the agents use |

## Railway checklist

1. New project → Deploy from GitHub repo → `SoulandCode69/sfa` (the repo is private; Railway needs access to it).
2. Variables: `SFA_ONLINE=1`, `SFA_PASSWORD=<long password>`, `SFA_DATA=/data`.
3. Add a volume mounted at `/data`.
4. The start command is in `railway.json` (`python3 tools/sfa.py console --online`); `Procfile` says the same for other hosts.
5. Generate a domain, open it, log in with any user name and the password.
6. Copy your current plan up once: paste the contents of `hardware/prototype/plan.json` into `/data/plan.json` (Railway shell), or
   just start designing there.

`base/` is a git submodule and is not needed online. If the host fails on it, turn submodule checkout off.

## Things to know before you rely on it

* **One password, one user.** There are no accounts. Anyone with the password can edit the plan.
* **Two copies of the plan.** The online plan (`$SFA_DATA/plan.json`) and the one in the repo do not sync. Pick one as the
  source of truth; the Designer's JSON export and the Presets export move things by hand.
* **Renders are not there.** The Units page shows no renders online, and the Sound agent has nothing to read, unless a
  `renders/` folder is deployed (it is git-ignored on purpose).
* **HTTPS comes from the host.** The password is sent with every request; do not serve online mode over plain HTTP.
* **The site** (`site/`) is static and separate: it can go on any static host without any of this.
