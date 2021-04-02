Bitrot 0.7
----------

This is an early version of what will become Bitrot 1.0.
As the code has been rewritten there might be unforeseen bugs, so please [report](https://github.com/grejppi/bitrot/issues) any new bugs you encounter! 😊

Before building, the DPF submodule needs to be initialized
```
git submodule update --init
```

## Dependencies

For Debian-based Distros, you can use the `dpf/.travis/install.sh` script to install all required dependencies (including Windows cross-compilation).
For all other environments, the packages should be similarly named.

## How to build

On Linux, if the native dependencies are installed, a `make` should be enough

## Windows builds
Install the cross-compilation dependencies (i.e. the mingw packages)
and then the easiest way is to use the DPF Travis CI scripts

```
./dpf/.travis/script-win64.sh
```
