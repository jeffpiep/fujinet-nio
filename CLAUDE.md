# FujiNet-NIO — Claude Code Instructions

## Primary Reference

**Read this first:** `docs/developer_onboarding.md`

It covers architecture, build setup, available profiles, console interaction,
CLI testing tools, and coding standards.

## Build (POSIX)

```bash
# One-time submodule init
git submodule update --init --recursive --force

# Build a POSIX profile (lists profiles: ./build.sh -p -S)
./build.sh -p fujibus-rs232-debug      # build
./build.sh -cp fujibus-rs232-debug     # clean + build

# Run the built binary (with auto-restart on exit)
cd build/fujibus-rs232-debug && ./run-fujinet-nio
```

## Architecture (brief)

```
Channel (byte pipe: PTY / SerialPort / USB-CDC)
    ↓
Transport (FujiBus + SLIP framing)
    ↓
Core (IOService → IODeviceManager → VirtualDevices)
```

- Channels: `src/platform/posix/channel_factory.cpp`
- Transport: `src/lib/fujibus_transport.cpp`
- Build profile routing: `src/lib/build_profile.cpp`
- Bootstrap / wiring: `src/lib/bootstrap.cpp`

## Coding Standards

- C++20, `std::unique_ptr` for ownership, no raw `new/delete`
- No `#ifdef` outside platform/profile factories
- Platform differences isolated in `src/platform/`
- Protocol logic in `src/lib/` and `include/fujinet/io/`
- After adding/moving source files: run `./scripts/update_cmake_sources.py`

## Git Workflow

This repo is used as a git submodule from the parent `amiga-fujinet` repo.

- Always work on a feature branch (never commit directly to `master`)
- Stage specific files only — no `git add .`
- Commit here first; the parent repo then updates its submodule pointer
