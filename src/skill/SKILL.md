---
name: numbrero
description: >
  Evaluate plain-text variable calculation sheets, convert units, and inspect
  saved Numbrero scratchpad sheets from the command line without opening a GUI.
---

# Numbrero CLI

Numbrero evaluates plain-text math sheets line by line with variables, percentages,
math functions, block aggregates (`sum`, `avg`, `count`, `prev`), and unit conversions.

## Commands

```bash
# Evaluate an inline sheet or expression
numbrero eval -e $'apples = 3\noranges = 5\nfruits = apples + oranges'

# Evaluate a file as machine-readable JSON
numbrero eval budget.txt --json

# Pipe expressions from stdin
printf '5 km in mi\n72 F in C\n' | numbrero eval

# Validate a sheet (exits 0 if valid, 1 with errors on stderr)
numbrero check budget.txt --json

# List saved scratchpad sheets
numbrero sheets --json

# Open files in the GUI
numbrero open budget.txt
```
