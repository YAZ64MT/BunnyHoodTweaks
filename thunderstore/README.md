# Bunny Hood Tweaks
This is a mod for Majora's Mask: Recompiled that is focused on making the bunny hood more seamless to use.

## Features
* Bunny hood no longer needs to be kept on C buttons to use it
* Keep speed boost while wearing other masks
* Apply speed boost while running forward and Z targetting
* Apply speed boost to non-human forms
* Draw bunny hood on non-human forms
* Allow bunny hood to be freely taken on and off while transformed
* Allow bunny hood to be toggled on and off in the pause menu
* Bunny ears are no longer completely stiff while standing still

Most of these features can be configured to fine-tune your experience.

## API
This mod disables the game's default bunny hood implementation, so mods that 
add functionality based on the bunny hood being equipped will generally be 
incompatible with this mod. For these situations, an API has been provided 
that is intended to be used with the optional dependency system.

```toml
optional_dependencies = [
    "yazmt_mm_bunnyhoodtweaks:0.1.0"
]
```

This mod currently provides a single exported function:

```c
// Returns true if the passed in player's run speed is being modified by BunnyHoodTweaks, false otherwise.
RECOMP_IMPORT("yazmt_mm_bunnyhoodtweaks", bool BunnyHoodTweaks_isPlayerRunSpeedModified(PlayState *play, Player *player));
```
