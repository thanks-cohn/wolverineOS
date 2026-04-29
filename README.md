██╗    ██╗ ██████╗ ██╗     ██╗   ██╗███████╗██████╗ ██╗███╗   ██╗███████╗
██║    ██║██╔═══██╗██║     ██║   ██║██╔════╝██╔══██╗██║████╗  ██║██╔════╝
██║ █╗ ██║██║   ██║██║     ██║   ██║█████╗  ██████╔╝██║██╔██╗ ██║█████╗  
██║███╗██║██║   ██║██║     ██║   ██║██╔══╝  ██╔══██╗██║██║╚██╗██║██╔══╝  
╚███╔███╔╝╚██████╔╝███████╗╚██████╔╝███████╗██║  ██║██║██║ ╚████║███████╗
 ╚══╝╚══╝  ╚═════╝ ╚══════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝╚══════╝

WolverineOS — Portable Machine. Baremetal Philosophy.
░░ WHAT IS THIS ░░

WolverineOS is not a framework.
It is not a library.
It is not a pile of scripts.

It is a machine.

A system where:

folders are units of life
config is the brain
logs are memory
binaries are muscle

You clone it.
You build it.
It runs.

No hidden state. No dependency maze. No guessing.

░░ CORE PRINCIPLES ░░
Portable — runs anywhere with gcc + make
Local-first — no cloud required
Inspectible — everything is plain text or C
Deterministic — what you build is what you run
State-aware — memory lives alongside code
Crash-safe — logs tell you exactly what happened
░░ QUICK START ░░

Clone and build:

git clone https://github.com/thanks-cohn/wolverineOS.git
cd wolverineOS
make clean
make

Run:

./ivault
./menu
./imeta doctor

If that works, the machine is alive.

░░ COMPONENTS ░░
ivault

Core system interface.
Handles sealing, restore, pruning, and system state.

menu

Interactive entry point.
Lightweight navigation layer.

imeta

Metadata engine.
Scans, indexes, binds, and verifies system structure.

imeta-watchd

Background watcher.
Maintains system awareness in real time.

░░ PROJECT STRUCTURE ░░
src/
  cli/        → user-facing commands
  core/       → system logic
  include/    → headers

modules/
  imeta/      → metadata subsystem

.wolverine/   → system memory (runtime state)
.imeta/       → metadata storage
░░ BUILD ░░
make clean
make

Requirements:

gcc
make
Linux environment
░░ PHILOSOPHY ░░

Most software separates code from reality.

WolverineOS does not.

This system can be used two ways:

Source Mode
Track only code. Clean, minimal, portable.
Machine Mode
Track everything — logs, state, memory.
Git becomes a time machine.

Choose intentionally.

░░ CURRENT STATE ░░
Build: ✅ working
CLI tools: ✅ functional
Metadata system: ✅ active
Runtime memory: evolving
Interface: minimal, growing
░░ NEXT STEPS ░░
unified wolverine command
snapshot / publish system
traversal UX (number-based navigation)
structured logging expansion
GUI layer (optional, not required)
░░ CONTRIBUTING ░░

If you understand what this is trying to become, you’re already part of it.

Keep things:

simple
explicit
inspectable
fast

No magic.

░░ FINAL NOTE ░░

This is not trying to be everything.

It is trying to be inevitable.

A system you can drop anywhere
and it just… works.
