# FAQ

**Q: Why does makepkg skip the interactive setup?**  
A: Package builds run in isolated environments without terminal access. Interactive prompts would cause the build to fail.

**Q: Can I use code formatting without git hooks?**  
A: Yes! Use `make format` to format code manually anytime.

**Q: What happens if I don't have clang or npm?**  
A: The build will continue normally. Formatting is completely optional for building the project.
