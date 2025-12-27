# Contributing to PhysicsCoin

Thank you for your interest in contributing!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a branch for your feature

## Building

```bash
make clean && make
./test_consensus  # Should pass 10/10
```

## Code Style

- C11 standard
- 4-space indentation
- `snake_case` for functions
- `PascalCase` for types
- Comments for non-obvious logic

## Testing

All changes must pass:
```bash
./test_consensus
./p2p_demo
./consensus_demo
```

## Pull Requests

1. Describe what you changed
2. Reference any issues
3. Ensure tests pass
4. Keep commits focused

## Areas for Contribution

- [ ] GPU acceleration (ROCm/HIP)
- [ ] Web wallet frontend
- [ ] Mobile SDK
- [ ] Additional tests
- [ ] Documentation improvements

## License

MIT License - see LICENSE file
