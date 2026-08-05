#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
clean_everything_after=false

remove_safe_item() {
	local path="$1"
	local absolute

	[[ -e "$path" ]] || return 0
	absolute="$(cd "$(dirname "$path")" && pwd -P)/$(basename "$path")"
	case "$absolute" in
		"$repo_root"/*) ;;
		*)
			printf 'Refusing to clean path outside the repository: %s\n' "$absolute" >&2
			return 1
			;;
	esac

	printf '  removing %s\n' "${absolute#"$repo_root"/}"
	rm -rf -- "$absolute"
}

clean_group() {
	local label="$1"
	shift
	local paths=("$@")
	local answer
	local clean_this=false

	((${#paths[@]} > 0)) || return 0

	if $clean_everything_after; then
		clean_this=true
	else
		while true; do
			read -r -p "Clean $label? [y/n/e] " answer
			case "${answer,,}" in
				y)
					clean_this=true
					break
					;;
				n)
					break
					;;
				e)
					clean_this=true
					clean_everything_after=true
					break
					;;
				*) printf 'Please enter y, n, or e.\n' ;;
			esac
		done
	fi

	if $clean_this; then
		local path
		for path in "${paths[@]}"; do
			remove_safe_item "$path"
		done
	fi
}

game_outputs=()
[[ -d "$repo_root/build" ]] && game_outputs+=("$repo_root/build")
while IFS= read -r -d '' path; do game_outputs+=("$path"); done < <(
	find "$repo_root/bin" -maxdepth 1 -type f -name 'funkin_disc*' -print0 2>/dev/null || true
)
clean_group 'compiled game objects and executables' "${game_outputs[@]}"

tool_outputs=()
while IFS= read -r -d '' path; do tool_outputs+=("$path"); done < <(
	find "$repo_root/tools" -type f -name '*.exe' -print0 2>/dev/null || true
)
clean_group 'compiled asset tools' "${tool_outputs[@]}"

graphics=()
while IFS= read -r -d '' path; do graphics+=("$path"); done < <(
	find "$repo_root/iso" -type f \( -name '*.tim' -o -name '*.arc' \) -print0 2>/dev/null || true
)
clean_group 'converted TIM and ARC graphics' "${graphics[@]}"

charts=()
while IFS= read -r -d '' path; do charts+=("$path"); done < <(
	find "$repo_root/iso" -type f -name '*.cht' -print0 2>/dev/null || true
)
clean_group 'compiled CHT charts' "${charts[@]}"

audio=()
while IFS= read -r -d '' path; do audio+=("$path"); done < <(
	find "$repo_root/iso" -type f -name '*.xa' -print0 2>/dev/null || true
)
clean_group 'converted XA audio' "${audio[@]}"

movies=()
while IFS= read -r -d '' path; do movies+=("$path"); done < <(
	find "$repo_root/iso" -type f -name '*.str' -print0 2>/dev/null || true
)
clean_group 'converted STR movies' "${movies[@]}"

disc_images=()
while IFS= read -r -d '' path; do disc_images+=("$path"); done < <(
	find "$repo_root" -maxdepth 1 -type f \( -name 'funkin_disc[0-9]*.bin' -o -name 'funkin_disc[0-9]*.cue' \) -print0
)
clean_group 'generated disc BIN and CUE images' "${disc_images[@]}"

printf 'Asset clean complete.\n'
