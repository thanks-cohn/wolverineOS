# Viability Plan for WolverineOS

This plan turns WolverineOS from a compelling prototype into a dependable product that users can adopt in real workflows.

## 1) Define one primary user and one primary job

Target user (first): **local developers/creators who need deterministic file integrity + metadata memory without cloud dependencies**.

Primary job-to-be-done:
- "I need to detect file damage/drift and recover the original state quickly, while keeping human context attached to files."

Success criteria (first 60 days):
- User can install, run a seal→verify→restore loop in <10 minutes.
- User can understand verify output and next actions without docs.
- No unexpected data deletion in normal flows.

## 2) Close CLI/docs mismatches immediately

Current mismatch: README shows `verify/restore/prune` examples with optional vault path, while the CLI usage currently requires explicit vault path arguments for those commands.

Action:
- Either implement `latest` default vault resolution for `verify`, `restore`, and `prune`, or update README examples to always include `<vault-folder>`.
- Add one canonical "happy path" that is copy/paste-safe.

## 3) Add release-grade test surface

Minimum viable test matrix:
- Unit tests for hash, manifest parsing, relative path behavior, and JSON memory validation.
- Integration tests for:
  - seal → verify clean
  - seal → mutate/delete/add → verify damage
  - restore returns to clean
  - prune quarantine vs delete mode behavior
- Fuzz/property tests for manifest loading and path handling.

Deliverable:
- `make test` target that runs quickly and returns non-zero on regressions.

## 4) Improve safety and trust UX

Users adopt recovery tools only when they trust them.

Near-term UX improvements:
- Add clear non-zero exit code mapping (e.g., clean=0, damage=2, usage=64, runtime=1).
- Provide machine-readable output mode for `verify` and `audit` (JSON or JSONL).
- Print explicit "next command" suggestions after damage detection.
- Add dry-run preview wherever data could be removed (`prune --delete` already gated by flag; keep that strict).

## 5) Package and distribution

Make onboarding trivial:
- Provide tagged releases with prebuilt binaries for Linux/macOS.
- Add package recipes (Homebrew tap + Debian package or static tarball).
- Include checksum/signature files for artifacts.

Success metric:
- First-run install + demo in <5 minutes without compiling.

## 6) Observability and diagnostics

Operational confidence requires diagnosability:
- Structured logs (timestamp + command + target + result + error code).
- `ivault doctor` command for environment checks (permissions, directory layout, binary versions).
- Optional verbose flag across commands for incident debugging.

## 7) Product boundary and versioning

Define what is "stable":
- Stable CLI contract for core commands (`seal`, `verify`, `restore`, `latest`, `view`, `push`).
- Semantic versioning and changelog discipline.
- Migration notes when storage formats evolve.

## 8) 30/60/90-day execution roadmap

### First 30 days
- Fix CLI/docs mismatch.
- Add CI build + integration smoke tests.
- Document one gold-path workflow.

### 60 days
- Add machine-readable outputs + exit code contract.
- Ship first versioned release artifacts.
- Add `make test` with meaningful coverage.

### 90 days
- Add doctor/diagnostics.
- Harden edge cases (symlinks, long paths, partial failures).
- Publish adoption-focused guide ("backup tool replacement", "creator archive workflow").

## 9) Practical viability scorecard

Track weekly:
- Build reliability: % successful CI runs on main.
- Restore reliability: % green restore scenarios in integration suite.
- Time-to-first-success: median minutes from clone to successful verify/restore demo.
- Regression rate: defects found after release per week.
- User trust signal: % users who run second seal after first successful restore.

---

If these steps are executed in order, WolverineOS can be viable as a **trust-first local integrity + context system** with clear differentiation from generic backup tools.
