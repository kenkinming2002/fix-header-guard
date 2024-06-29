# fix-header-guard
A tool to fix c/c++ header guard automatically based off of directory structure.

## Build
```sh
$ make
```

## Example
```sh
$ find BASE/include -name "*.h" | xargs fix-header-guard -b BASE
```
