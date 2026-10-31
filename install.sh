#!/bin/sh
# Installs ReactX:
#
#   curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh
#
# 1. the prebuilt `reactx` CLI from GitHub Releases into ~/.local/bin
#    (REACTX_BIN changes the folder, REACTX_VERSION=v0.8.2 picks a release);
# 2. `reactx install`: the compiler (Emscripten) and, if needed, CMake and
#    Ninja into ~/.reactx — no sudo, like npm.
set -eu

repo=sargisis/ReactX
os=$(uname -s)
arch=$(uname -m)
if [ "$os" != Linux ] || [ "$arch" != x86_64 ]; then
  echo "reactx: the prebuilt CLI is for Linux x86-64; on $os $arch build it from source:"
  echo "  https://github.com/$repo#quick-start  (or use the Docker image ghcr.io/sargisis/reactx)"
  exit 1
fi
for tool in curl tar; do
  command -v "$tool" > /dev/null || { echo "reactx: $tool is needed"; exit 1; }
done

tag=${REACTX_VERSION:-}
if [ -z "$tag" ]; then
  tag=$(curl -fsSL "https://api.github.com/repos/$repo/releases/latest" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n 1)
fi
[ -n "$tag" ] || { echo "reactx: can't find the latest release of $repo"; exit 1; }
version=${tag#v}

# Not on PATH yet: the line for the user's own shell (zsh doesn't read ~/.bashrc).
# Printed last, so it isn't lost above the output of `reactx install`.
path_hint() {
  case ":$PATH:" in
    *":$bin:"*) ;;
    *)
      case "${SHELL:-}" in
        */zsh) rc=~/.zshrc ;;
        */fish) rc= ;;
        *) rc=~/.bashrc ;;
      esac
      echo
      echo "Add $bin to PATH (once), then reactx works in this and every new terminal:"
      if [ -n "$rc" ]; then
        echo "  echo 'export PATH=\"$bin:\$PATH\"' >> $rc && . $rc"
      else
        echo "  fish_add_path $bin"
      fi
      ;;
  esac
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
echo "Downloading reactx $version..."
curl -fL --progress-bar "https://github.com/$repo/releases/download/$tag/reactx-$version-linux-x86_64.tar.gz" | tar xz -C "$tmp"

bin=${REACTX_BIN:-$HOME/.local/bin}
mkdir -p "$bin"
cp "$tmp"/reactx-*/bin/* "$bin/"
echo "Installed reactx $version into $bin"
# The compiler and tools (reactx install asks nothing, so a pipe is fine).
# Releases before 0.8.2 have no `reactx install`: say what to do instead.
if "$bin/reactx" help install > /dev/null 2>&1; then
  "$bin/reactx" install || echo "reactx: run \`reactx install\` again to finish"
else
  echo "reactx $version can't install the compiler itself: install Emscripten 6+ (https://emscripten.org/docs/getting_started/downloads.html),"
  echo "or get a newer release:  curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh"
fi
path_hint
