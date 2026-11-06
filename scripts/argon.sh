#!/usr/bin/env bash
# Delega uma tarefa bem definida ao agy (Antigravity) num git worktree isolado.
# Uso:  scripts/argon.sh <nome-da-tarefa> <arquivo-de-briefing.md>
# Env:  ARGON_MODEL    modelo (veja `agy models`); vazio = padrao do agy
#       ARGON_EFFORT   low|medium|high|xhigh|max  (padrao: high)
#       ARGON_TIMEOUT  limite do turno, ex. 30m   (padrao: 30m)
#       ARGON_DRY_RUN  1 = so imprime o comando
set -euo pipefail

name="${1:?uso: argon.sh <nome> <briefing.md>}"
briefing="${2:?uso: argon.sh <nome> <briefing.md>}"
[[ "$name" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "nome invalido (use letras, numeros, _ e -)" >&2; exit 2; }
[[ -f "$briefing" ]] || { echo "briefing nao encontrado: $briefing" >&2; exit 2; }
command -v agy >/dev/null || { echo "agy nao encontrado no PATH" >&2; exit 2; }

repo="$(git rev-parse --show-toplevel)"
branch="argon/$name"
worktree="$repo/../$(basename "$repo")-argon/$name"
out_dir="$repo/.argon/out"
report="$out_dir/$name.json"
effort="${ARGON_EFFORT:-high}"
timeout="${ARGON_TIMEOUT:-30m}"

footer='

---
REGRAS DE EXECUCAO (obrigatorias):
- Leia CLAUDE.md, AGENTS.md e docs/ARCHITECTURE.md antes de editar. Respeite as camadas e os contratos.
- Altere somente os arquivos necessarios para a tarefa; nao refatore fora do escopo.
- Nao altere formatos de arquivo (.room/.rules) nem remova leitura de chaves antigas.
- Rode build e testes (cmake --build ... && ctest --output-on-failure) e corrija o que quebrar.
- Termine com um relatorio curto: 1) arquivos alterados, 2) o que foi feito, 3) resultado de build/testes (comandos e saida resumida), 4) duvidas ou riscos.'

prompt="$(cat "$briefing")$footer"
cmd=(agy -p "$prompt" --output-format json --mode accept-edits
     --effort "$effort" --print-timeout "$timeout")
[[ -n "${ARGON_MODEL:-}" ]] && cmd+=(--model "$ARGON_MODEL")

if [[ "${ARGON_DRY_RUN:-0}" == "1" ]]; then
  echo "worktree: $worktree (branch $branch)"; echo "relatorio: $report"
  printf 'cmd:'; printf ' %q' "${cmd[0]}" "${cmd[1]}" '<briefing+regras>' "${cmd[@]:3}"; echo
  exit 0
fi

if [[ -n "$(git -C "$repo" status --porcelain)" ]]; then
  echo "AVISO: ha mudancas nao commitadas; o Argon so ve o HEAD (commit) atual." >&2
fi
mkdir -p "$out_dir"
git -C "$repo" worktree add -b "$branch" "$worktree" HEAD >&2

status=0
( cd "$worktree" && "${cmd[@]}" ) > "$report" 2> "$out_dir/$name.err" || status=$?

if [[ -n "$(git -C "$worktree" status --porcelain)" ]]; then
  git -C "$worktree" add -A
  git -C "$worktree" commit -q -m "argon: $name" || true
fi

echo "== argon: $name  (exit $status)"
echo "branch:   $branch"
echo "worktree: $worktree"
echo "-- alteracoes vs HEAD do repo:"
git -C "$repo" diff --stat "HEAD...$branch" | tail -n 25
echo "-- relatorio do Argon (resumo, ate 60 linhas):"
python3 - "$report" <<'PY' | head -n 60
import json, sys
raw = open(sys.argv[1], encoding="utf-8", errors="replace").read()
text = None
try:
    data = json.loads(raw)
    if isinstance(data, dict):
        for key in ("result", "response", "text", "output", "message", "final"):
            if isinstance(data.get(key), str):
                text = data[key]; break
    if text is None:
        text = json.dumps(data, ensure_ascii=False, indent=1)
except Exception:
    text = raw
print(text[:6000])
PY
echo "-- relatorio completo: $report   stderr: $out_dir/$name.err"
exit $status
