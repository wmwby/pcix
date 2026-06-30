# pcix

PCI device exploration tool for Linux.

## Features

- `mem show` - Display memory address ranges allocated to a PCI device

## Building

```bash
make
```

## Usage

```bash
# Show memory resources for a PCI device
sudo pcix mem show -s 0000:01:00.0

# Or with short BDF format
pcix mem show -s 01:00.0

# Show help
pcix -h
```

## BDF Format

The tool supports two BDF (Bus:Device.Function) formats:
- Short: `DD:BB:DD.F` (e.g., `01:00.0`)
- Full: `DDDD:DD:DD.F` (e.g., `0000:01:00.0`)

## Requirements

- Linux with sysfs support
- gcc compiler
- make build system

## License

MIT
