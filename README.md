# Click on Steps

Like the [Click Between Frames mod](https://github.com/theyareonit/Click-Between-Frames) by syzzi, but without the physics bypass.

In practice, you gain the following benefits over CBF:

- No physics/trigger bugs, including in platformer mode
- Higher precision inputs (up to 240 tps) with no performance loss
- Improved bot compatibility
- Improved support for other input devices
- Theoretically leaderboard safe (identical to playing on 240fps). Don't take my word on that.

Unlike CBF, this mod _cannot_ click between steps.
Which is to say, your input precision is fixed at 240 tps.
Unlike CBF, players who typically play the game at 240fps will not receive any benefits from this mod.
This mod is made for players on lower end devices, or devices that don't perform well without VSync.

## Details

The following section is intended to explain the mod to help those who wish to reimplement the mod themselves, or wish to evaluate if this mod would count as cheating.

Thanks to the replay system, the game already supports making player inputs in between steps.
However, there is no logic to match external player inputs to steps (and handle them correspondingly).
At a very high level, the mod performs these changes to achieve that:

1. Adding timestamps to the game input callbacks (this is most of the mod's functionality)
2. Modifying the physics input processing method to only process inputs that have happened "during" that step

This is similar to how other games with high precision timings operate. The structure of this mod is designed to make it easy to port to new platforms/versions of the game. For more details, it might be easier to just read the code.

On normal platforms (aka, not Windows), step 1 is very trivial.
Most of the difficulty comes from not being RobTop, who can do things like add parameters to methods.
This is what the code in `async.cpp` and `input.cpp` work around. `input.cpp` adds custom code to pass the timestamp from the event dispatchers to the `UILayer`. `async.cpp` then pulls that timestamp from the event and passes it to the `GJBaseGameLayer`.
Platform specific hooks are used to pull timestamps to the input callbacks, which then gets injected into the event dispatchers.

On Windows, step 1 is accomplished by separating the game into a rendering thread and a 1000hz input thread.
Separating inputs from the rendering thread allows for receiving multiple inputs while the main update is processing, which can then be timestamped.
Unfortunately, this also means input precision is only as precise as the input thread is.
While this puts Windows at a disadvantage compared to other platforms, the fixed 240tps rate makes this very hard, if not impossible, to notice.
A benefit of this approach (as opposed to continuously waiting for window messages in a new window) is that the controller now works!
However, the chance of accidentally breaking the game by calling a method on the wrong thread is very high, as the game was never designed to handle this.

Step 2 is done by `main.cpp`. An input, with timestamp, is received from our modified `UILayer`. The mod then takes this timestamp and determines when it happened, relative to the beginning of the frame. This input, and its timestamp, is stored inside a queue. During each step, the game checks if any inputs are queued and need processing. We add code just before this check, which calculates the current time of the step and determines which inputs from the mod's input queue should be handled. Those inputs are then moved to the game's input queue, to be processed by the game. This means that inputs made by the mod internally look identical to inputs made without the mod.

In practice, the difference between vanilla GD running at 60hz and a game running this mod is that vanilla GD handles all queued inputs at the beginning of a frame (aka, step 0) while this can run queued inputs at the beginning of a step. On 240hz, each frame contains exactly one step, which is why the mod has no effect for players on that framerate.

For those who need a fancy diagram:

![Comparison between different game configurations and how the steps respond to inputs](/input_comparison.drawio.svg)

To make the comparison easier, the fancy diagram skips a lot of detail. Sorry.

## Special Thanks

- [mat](https://github.com/matcool) for the name and Windows testing
- [ninXout](https://github.com/ninXout) for testing
- [syzzi](https://github.com/theyareonit) for the original mod
