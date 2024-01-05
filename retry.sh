#!/bin/sh
# Retry a command up to a given number of times.

if [ "x$2" = "x" ]; then
  echo "usage: $0 <n> <command line>" >&2
  echo "" >&2
  echo "This runs the given command until it succeeds or it has failed" >&2
  echo "<n> times." >&2
  exit 2
fi

# Number of remaining retries.
num_tries="$1"

# Shift the first argument so what remains, "$@", is the command to run.
shift

# Save the original number of retries so we can say at the end how many
# there were.
orig_num_tries="$num_tries"

# Exit code of the last attempt.
exit_code=0

while [ "$num_tries" -gt 0 ]; do
  # Run the program in a sub-shell so it can do things like set
  # variables or invoke 'exit' without affecting this parent shell
  # process.
  ("$@")

  # Save the exit code immediately, before testing with 'test' (called
  # '[' here), since that overwrites the code.
  exit_code="$?"

  # If the program ran successfully, exit the loop.
  if [ "$exit_code" -eq 0 ]; then
    exit 0
  fi

  # Inform the user that the program failed.  Print the exit code
  # because only the last attempt's code is otherwise propagated to the
  # caller.
  echo "retry: Command failed with exit code $exit_code: $*" >&2

  # Consume the retry.
  num_tries=$(expr "$num_tries" - 1)

  # If we still have retries left, say we are running it again.
  if [ "$num_tries" -gt 0 ]; then
    echo "retry: Retrying, $num_tries attempts remain..." >&2
  fi
done

# Exhausted the retries.
echo "retry: Exiting with code $exit_code because command failed $orig_num_tries times: $*" >&2

# Exit with the same code as the final attempt.
exit $exit_code

# EOF
