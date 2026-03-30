# Bunny Hood Tweaks for Majora's Mask: Recompiled

This is a mod for Majora's Mask: Recompiled that is focused on making the bunny hood more seamless to use.

### Features
* Bunny hood no longer needs to be kept on C buttons to use it
* Keep speed boost while wearing other masks
* Apply speed boost while running forward and Z targetting
* Apply speed boost to non-human forms
* Draw bunny hood on non-human forms
* Allow bunny hood to be freely taken on and off while transformed
* Allow bunny hood to be toggled on and off in the pause menu
* Bunny ears are no longer completely stiff while standing still
* Draws a border around bunny hood in the pause menu when enabled

### API
This mod disables the game's default bunny hood implementation, so mods that add functionality based on the bunny hood being equipped will generally be incompatible with this mod. For these situations, an API has been provided that is intended to be used with the optional dependency system.

```toml
optional_dependencies = [
    "yazmt_mm_bunnyhoodtweaks:0.2.1"
]
```

This mod currently provides a single exported function:

```c
// Returns true if the passed in player's run speed is being modified by BunnyHoodTweaks, false otherwise.
RECOMP_IMPORT("yazmt_mm_bunnyhoodtweaks", bool BunnyHoodTweaks_isPlayerRunSpeedModified(PlayState *play, Player *player));
```

### Writing mods
See [this document](https://hackmd.io/fMDiGEJ9TBSjomuZZOgzNg) for an explanation of the modding framework, including how to write function patches and perform interop between different mods.

### Tools
You'll need to install `clang` and `make` to build this template.
* On Windows, using [chocolatey](https://chocolatey.org/) to install both is recommended. The packages are `llvm` and `make` respectively.
  * The LLVM 19.1.0 [llvm-project](https://github.com/llvm/llvm-project) release binary, which is also what chocolatey provides, does not support MIPS correctly. The solution is to install 18.1.8 instead, which can be done in chocolatey by specifying `--version 18.1.8` or by downloading the 18.1.8 release directly.
* On Linux, these can both be installed using your distro's package manager. You may also need to install your distro's package for the `lld` linker. On Debian/Ubuntu based distros this will be the `lld` package.
* On MacOS, these can both be installed using Homebrew. Apple clang won't work, as you need a mips target for building the mod code.

On Linux and MacOS, you'll need to also ensure that you have the `zip` utility installed.

You'll also need to grab a build of the `RecompModTool` utility from the releases of [N64Recomp](https://github.com/N64Recomp/N64Recomp). You can also build it yourself from that repo if desired.

### Building
* First, run `make` (with an optional job count) to build the mod code itself.
* Next, run the `RecompModTool` utility with `mod.toml` as the first argument and the build dir (`build` in the case of this template) as the second argument.
  * This will produce your mod's `.nrm` file in the build folder.
  * If you're on MacOS, you may need to specify the path to the `clang` and `ld.lld` binaries using the `CC` and `LD` environment variables, respectively.

### Updating the Majora's Mask Decompilation Submodule
Mods can also be made with newer versions of the Majora's Mask decompilation instead of the commit targeted by this repo's submodule.
To update the commit of the decompilation that you're targeting, follow these steps:
* Build the [N64Recomp](https://github.com/N64Recomp/N64Recomp) repo and copy the N64Recomp executable to the root of this repository.
  * Make sure you pass `KEEP_MDEBUG=1` to `make` when building the decomp in order to keep debug information. This must be done from a clean build if you have built the decomp already without `KEEP_MDEBUG=1`.
* Build the version of the Majora's Mask decompilation that you want to update to and copy the resulting .elf file to the root of this repository.
* Update the `mm-decomp` submodule in your clone of this repo to point to the commit you built in the previous step.
* Run `N64Recomp generate_symbols.toml --dump-context`
* Rename `dump.toml` and `data_dump.toml` to `mm.us.rev1.syms.toml` and `mm.us.rev1.datasyms.toml` respectively.
  * Place both files in the `Zelda64RecompSyms` folder.
* Try building.
  * If it succeeds, you're done.
  * If it fails due to a missing header, create an empty header file in the `include/dummy_headers` folder, with the same path.
    * For example, if it complains that `assets/objects/object_cow/object_cow.h` is missing, create an empty `include/dummy_headers/objects/object_cow.h` file.
  * If RecompModTool fails due to a function "being marked as a patch but not existing in the original ROM", it's likely that function you're patching was renamed in the Majora's Mask decompilation.
    * Find the relevant function in the map file for the old decomp commit, then go to that address in the new map file, and update the reference to this function in your code with the new name.
