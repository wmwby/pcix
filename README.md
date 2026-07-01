# pcix

PCI device exploration tool for Linux.

## Features

- `mem show` - Display memory address ranges allocated to a PCI device
- `mem read` - Read data from a PCI device's memory-mapped I/O (MMIO) region
- `mem write` - Write data to a PCI device's MMIO region (destructive)

## Building

```bash
make
```

## Usage

### mem show

Display all memory address ranges the host allocated to a PCI device.

```bash
# Show memory resources for a PCI device
pcix mem show -s 0000:01:00.0

# Or with short BDF format
pcix mem show -s 01:00.0
```

64-bit BARs (BAR0+BAR1 combined) are displayed as a single merged range, e.g. `BAR0/1: ...`.

### mem read

Read data from a PCI device BAR via `/dev/mem` + `mmap`. **Requires root.**

```bash
# Read 16 bytes from BAR0 at offset 0x100, display as bytes (default)
pcix mem read -s 01:00.0 --bar 0 --offset 0x100 --size 16

# Read an address range, display as 32-bit dwords
pcix mem read -s 01:00.0 --bar 0 --range 0x100-0x13f -d

# Read the entire BAR as 16-bit words
pcix mem read -s 01:00.0 --bar 0 --all -w
```

Options:

| Option | Description |
|--------|-------------|
| `-s <bdf>` | PCI device BDF (required) |
| `--bar <n>` | BAR number 0-5 (required) |
| `--offset <addr>` | Starting offset (hex) |
| `--size <n>` | Number of bytes to read (decimal) |
| `--range <s-e>` | Address range, e.g. `0x100-0x13f` |
| `--all` | Read entire BAR |
| `-b` | Display as bytes (default) |
| `-w` | Display as 16-bit words |
| `-d` | Display as 32-bit dwords |

One of `--offset+--size`, `--range`, or `--all` is required.

#### 64-bit BAR rules

A 64-bit BAR occupies two consecutive registers (e.g. BAR0 + BAR1). Only the
primary BAR number may be passed to `--bar`:

- ✅ Allowed: `--bar 0`, `--bar 2`, `--bar 4`
- ❌ Forbidden: `--bar 1`, `--bar 3`, `--bar 5` (high half of a 64-bit pair)

Accessing a forbidden BAR reports:

```
Error: BAR1 is part of a 64-bit BAR pair with BAR0. Use --bar 0 instead.
```

### mem write

Write data to a PCI device BAR via `/dev/mem` + `mmap` (opened read/write).
**Requires root. Destructive — writing the wrong value to the wrong register
can crash or hang the device, or the whole system. Know what you are writing
before you run it.**

```bash
# Write a 32-bit value to BAR0 at offset 0x40 (width from -d)
pcix mem write -s 01:00.0 --bar 0 --offset 0x40 --value 0x80000000 -d

# Write a 16-bit value, skip the confirmation prompt
pcix mem write -s 01:00.0 --bar 0 --offset 0x40 --value 0x1234 -w --yes

# Write a block of raw bytes from a hex string
pcix mem write -s 01:00.0 --bar 0 --offset 0x100 --hex deadbeefcafebabe

# Write the contents of a file
pcix mem write -s 01:00.0 --bar 0 --offset 0x0 --file payload.bin
```

By default `mem write` prints a summary and asks for confirmation (`y/N`),
then writes, then reads back the written range and prints it so you can
confirm the write landed. `--yes` skips the prompt; `--no-verify` skips the
read-back.

Options:

| Option | Description |
|--------|-------------|
| `-s <bdf>` | PCI device BDF (required) |
| `--bar <n>` | BAR number 0-5 (required) |
| `--offset <addr>` | Starting offset within the BAR, hex (required) |
| `--value <hex>` | Single register value (hex). Width set by `-b`/`-w`/`-d` |
| `--file <path>` | Write the contents of this file |
| `--hex <hexstr>` | Write raw bytes from a hex string (e.g. `deadbeef`) |
| `-b` | `--value` width = 8-bit (default) |
| `-w` | `--value` width = 16-bit |
| `-d` | `--value` width = 32-bit |
| `--yes`, `-y` | Skip the confirmation prompt |
| `--no-verify` | Do not read back and print after writing |

Exactly one of `--value`, `--file`, or `--hex` is required. `-b`/`-w`/`-d`
only apply to `--value`.

`--value` is encoded little-endian into the chosen width, so writing
`0x1234 -w` then reading back with `-w` shows `1234` (matching the
little-endian register convention `mem read` displays).

The 64-bit BAR rules described under [`mem read`](#64-bit-bar-rules) apply to
`mem write` as well.

### General

```bash
# Show help
pcix -h
pcix mem read -h
pcix mem write -h
```

## BDF Format

The tool supports two BDF (Bus:Device.Function) formats:
- Short: `BB:DD.F` (e.g., `01:00.0`)
- Full: `DDDD:BB:DD.F` (e.g., `0000:01:00.0`)

## Requirements

- Linux with sysfs support
- gcc compiler
- make build system
- Root privileges for `mem read` and `mem write` (access to `/dev/mem`)

## License

MIT
