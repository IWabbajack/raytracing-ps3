# Raytracing in One Weekend - PS3 Port

This project is a PlayStation 3 (PS3) port of the well-known "Raytracing in one weekend".

## Prerequisites

This project relies on the PS3's development toolchain, as well as PS3Load to deploy generated executables. 

- First, you'll need to set up the [ps3toolchain](https://github.com/ps3dev/ps3toolchain). This set of tools will ensure that your environment is correctly set up to compile and build executables for the PS3.

- Second, you'll need to install [PS3Load](https://store.brewology.com/ahomebrew.php?brewid=42) onto your PS3. This will allow you to load and run homebrew applications that you've compiled with the aforementioned `ps3toolchain`.

## Building

Given that you've correctly set up the prerequisites, building the project is straightforward. Simply run the make command in the root directory of the project.

```
$ make
```

This will generate a SELF file, which can be loaded on the PS3.

## Deployment

Take the compiled SELF file and load it onto your PS3 using PS3Load.

:x
