```
██╗    ██╗ ██████╗ ██╗     ██╗   ██╗███████╗██████╗ ██╗███╗   ██╗███████╗
██║    ██║██╔═══██╗██║     ██║   ██║██╔════╝██╔══██╗██║████╗  ██║██╔════╝
██║ █╗ ██║██║   ██║██║     ██║   ██║█████╗  ██████╔╝██║██╔██╗ ██║█████╗  
██║███╗██║██║   ██║██║     ██║   ██║██╔══╝  ██╔══██╗██║██║╚██╗██║██╔══╝  
╚███╔███╔╝╚██████╔╝███████╗╚██████╔╝███████╗██║  ██║██║██║ ╚████║███████╗
 ╚══╝╚══╝  ╚═════╝ ╚══════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝╚══════╝
```

# WolverineOS

> Portable Machine · Baremetal Philosophy

---

## ░░ WHAT THIS IS ░░

WolverineOS is not a framework.  
Not a library.  
Not a pile of scripts.

**It is a machine.**

You clone it.  
You build it.  
It runs.

No hidden state.  
No dependency maze.  
No guessing.

---

## ░░ BIG BANG TEST ░░

Run this. Break it. Bring it back.

```bash
make clean
make
```

```bash
mkdir -p demo
for i in $(seq 1 40); do
  echo "file $i" > demo/file_$i.txt
done
```

```bash
cp demo/file_1.txt demo/file_1.jpg
cp demo/file_2.txt demo/file_2.pdf
cp demo/file_3.txt demo/file_3.log
cp demo/file_4.txt demo/file_4.conf
cp demo/file_5.txt demo/file_5.data
```

```bash
./ivault seal demo
```

```bash
echo "corrupted" > demo/file_5.txt
rm demo/file_10.txt
rm demo/file_20.txt
rm demo/file_30.txt
echo "junk" > demo/file_1.jpg
```

```bash
./ivault verify demo
```

```bash
./ivault restore demo
```

```bash
ls demo
```

If it all comes back — you get it.

---

## ░░ MEMORY SYSTEM (THE DIFFERENCE) ░░

Now give the files meaning.

```bash
./ivault tag demo/file_1.jpg "manga | panel | clean"
```

```bash
./ivault note demo/file_1.jpg "good scan"
```

```bash
./ivault summary demo/file_1.jpg "chapter page image"
```

```bash
./ivault custom demo/file_1.jpg source "planetes | omnibus"
```

Compile it:

```bash
./ivault push
```

View it:

```bash
./ivault view demo/file_1.jpg
```

You’ll see structured JSON.

Not logs. Not noise.

**Meaning.**

---

## ░░ FOLDER TAGGING ░░

```bash
./ivault tag demo "archive | important"
```

```bash
./ivault tag demo "archive | important" --nested
```

Direct vs full-tree tagging.

---

## ░░ QUICK START ░░

```bash
git clone https://github.com/thanks-cohn/wolverineOS.git
```

```bash
cd wolverineOS
```

```bash
make clean
make
```

```bash
./ivault
./menu
```

---

## ░░ COMMAND CHEAT SHEET ░░

### Vault

```bash
./ivault seal <folder>
./ivault latest
./ivault verify <folder> "vaults/<timestamp>"
./ivault restore <folder> "vaults/<timestamp>"
./ivault prune <folder>
```

### Memory

```bash
./ivault tag <file|folder> "a | b | c"
./ivault note <file|folder> "text"
./ivault summary <file|folder> "one line identity"
./ivault custom <file|folder> key "value"
./ivault push
./ivault view <file|folder>
```

### Navigation

```bash
./menu
```

```
number = forward
b      = back
q      = quit
h      = help
```

---

## ░░ CORE PRINCIPLES ░░

Portable — gcc + make  
Local-first — no cloud  
Inspectable — plain text + C  
Deterministic — no hidden behavior  
State-aware — memory beside code  
Crash-safe — logs explain everything  

---

## ░░ DESIGN LAW ░░

The machine should never surprise you.

Build commands do not destroy user data.

Memory is plain text.

Bad JSON is rejected. Not hidden.

The system must be usable:

tired  
late  
under pressure  

---

## ░░ PHILOSOPHY ░░

Most tools store files.

WolverineOS stores **meaning**.

- tag → structure  
- note → human thought  
- summary → identity  
- custom → extension  
- push → truth enforcement  
- JSONL → clean memory over time  

You don’t just restore files.

You restore **context**.

---

## ░░ CURRENT STATE ░░

Build: working  
Core: stable  
Memory system: live  
Interface: minimal  

---

## ░░ NEXT STEPS ░░

wolverine command  
snapshot / publish  
number-based traversal  
expanded logs  
optional GUI  

---

## ░░ FINAL NOTE ░░

Click.  
Drop.  
Trust.

A system you can place anywhere…

build…

and it just works.
