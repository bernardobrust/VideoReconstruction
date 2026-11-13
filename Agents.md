# Agents.md

## 1. General Rules

- Do not run any of the following commands:
  - `git add`
  - `git commit`
  - `git push`
- Never delete files or directories without explicit permission
- Never discard, overwrite, or revert existing user changes
- Do not modify unrelated files or introduce unrelated changes
- Ask for clarification when requirements are ambiguous or when a change would significantly affect existing architecture

## 2. Project Context

This is a high-performance C codebase. Performance, simplicity, and predictability are important.

- Prefer straightforward implementations over unnecessary abstractions
- Avoid unnecessary heap allocations, especially in performance-critical paths
- Avoid unnecessary copying of large structures or buffers
- Preserve existing memory ownership, lifetime, and allocation conventions
- Avoid overly generic solutions and speculative generalization
- Prefer existing utilities and established patterns over introducing new ones
- Do not optimize based on intuition, profile before optimizing

## 3. Error Handling and Assertions

Use `assert()` whenever a programming error or violated internal invariant can occur

Examples:
- Invalid internal state
- Unexpected null pointers
- Invalid internal input ranges
- Violated preconditions or postconditions

Distinguish programming errors from expected runtime failures:

- Validate external input and malformed data before processing
- Handle recoverable syscall failures appropriately
- Do not use assertions as a substitute for normal error handling if recoverable
- Never silently ignore errors that should be reported

**Never put function calls with side effects inside assertions. Assertions are disabled in release builds, so their expressions must never be required for program correctness**

## 4. Coding Style

Follow GNU coding conventions, as configured in `src/.clang-format`.

Formatting scripts are also available in the repository (`src/format.(sh/bat)`. Prefer using the existing formatting tools rather than manually approximating the style

Additional conventions:

- Prefer declaring variables at the beginning of their scope (functions, loops, conditionals, etc.)
- Variables of the same type should preferably share a declaration
- Separate declarations from the main body of code with a blank line
- Use blank lines to visually separate different logical sections, particularly around control structures
- Leave a blank line before control-flow exits and synchronization operations when appropriate (e.g. returns, mutex unlocks and condition-variable signals)
- Mutex locking/unlocking and test-group initialization/termination statements must be isolated on their own lines
- Comments start with an uppercase letter. In multi-sentence comments, use periods between sentences, but omit the trailing period after the final sentence

Functions of approximately five lines or fewer may disregard the above spacing and declaration conventions. Prefer keeping them compact

## 5. Working With Existing Code

Before implementing a change:

1. Read the relevant documentation (especially in `docs/`) and inspect the existing implementation
2. Look for existing utilities, data structures, and patterns that solve similar problems
3. Understand the surrounding code before modifying it
4. Consider how the change interacts with existing ownership, threading, and platform-specific code
5. Comment the code whenever it is not obvious, or the algorithm or technique is obscure or uncommon

When implementing:

- Make the smallest and simplest, but complete, change that correctly solves the problem
- Preserve existing naming conventions and architectural decisions
- Avoid unnecessary refactoring
- Avoid introducing new abstractions when existing structures are sufficient
- Do not change public interfaces unless required by the task
- Do not duplicate functionality that already exists elsewhere

## 6. Verification

After making changes:

- Compile the affected code whenever practical
- Run relevant tests whenever practical
- Check for compiler warnings and errors
- Inspect the final diff for unintended modifications
- Do not claim that something works or has been tested unless it actually has

If verification cannot be performed, explicitly state what remains unverified

## 7. Codebase Structure

Read `src/README.md` before making significant changes

Other relevant files:

- `todo.md`: The project's TODO list and development roadmap
- `sources.md`: Main list of learning resources used in the development of this project