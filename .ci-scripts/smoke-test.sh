#!/usr/bin/env bash

set -eux -o pipefail

# Release tags vX.Y.Z are considered stable.
# vX.Y.Z-rcN or other tags are considered unstable.
if [ -n "$(echo "${GITHUB_REF:-}" | grep -E 'refs/tags/v[0-9]+\.[0-9]+\.[0-9]+$')" ] ||
  [ -n "$(git tag --points-at HEAD | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$')" ]; then
  REGEX="qTox v.* (stable)"
else
  REGEX="qTox v.* (unstable)"
fi

"$@" --version | grep "$REGEX" || ("$@" --version && false)

# Verify that the version in --update-check matches --version.
QTOX_VERSION=$("$@" --version | grep -o 'v[0-9][^ ,]*' | head -n 1)
"$@" --update-check | grep "^Current version: $QTOX_VERSION" || ("$@" --update-check && false)

# Verify that --update-check successfully fetched a version from GitHub.
"$@" --update-check | grep "^Latest version: v[0-9]\+" || ("$@" --update-check && false)

# If QTOX_SCREENSHOT isn't set, don't take a screenshot.
if [ -z "${QTOX_SCREENSHOT:-}" ]; then
  exit 0
fi

# Otherwise, run the application, wait 10 seconds for it to start up, then send
# SIGUSR1 to take a screenshot.

set -m # Enable job control.

mkdir -p "$HOME/Pictures" # For the screenshot.
"$@" --portable "$PWD/test/resources/profile" --profile "qtox-test-user" 2>&1 | tee qtox.log &
sleep 10

# We need to get the real PID of the qtox process, which is a descendant of %1.
# %1 may be xvfb-run, wrapping bash, running AppRun, wrapping musl's ld.so.
QTOX_PID="$(grep -o ' : Debug: Process ID: [0-9]*' qtox.log | grep -o '[0-9]*')"
kill -USR1 "$QTOX_PID"

# Wait for another second to make sure the screenshot is taken.
sleep 1

# Kill the application with SIGINT.
kill -INT "$QTOX_PID"

# Wait for the application to exit gracefully. We use pgrep above, in case qtox
# is running inside xvfb-run. We use %1 here for job control (the & above).
fg %1

mv "$HOME/Pictures/$QTOX_SCREENSHOT" "$QTOX_SCREENSHOT"
