# vc4vbl - vertical blank probe for the Raspberry Pi 3

`vc4vbl` checks whether the VideoCore IV of a Raspberry Pi 3 can give Emu68 a vertical blank
interrupt, as the Pi 4 already does with VideoCore.card. It does not need a new driver, and
you do not have to reboot. It prints plain text that you send back to us.

## What you need

- A PiStorm with a **Raspberry Pi 3 B or 3 B+** running Emu68 (the version you use every day).
  On any other board the tool prints its identification and stops, without touching anything.
- Your usual Picasso96 setup. It does not matter which version, the tool reports it.
- The `vc4vbl` file copied anywhere, for example `RAM:`.

## The three steps

Run them from a Shell, one after the other. Each one writes its own report file.

### Step 1: read only (no risk)

```
vc4vbl >RAM:vc4vbl-1.txt
```

This reads the board model, the library versions, the device tree and a few hardware
registers. It changes nothing.

### Step 2: watch (low risk, the machine pauses for about 4 seconds)

```
vc4vbl WATCH SECS=2 >RAM:vc4vbl-2.txt
```

The tool watches the display hardware twice for 2 seconds. During that time multitasking is
stopped and the mouse pointer does not move. That is expected. The tool puts every setting
back as it found it.

### Step 3: real interrupt (small risk of a hang, save your work first)

```
vc4vbl IRQ SECS=2 >RAM:vc4vbl-3.txt
```

The tool installs a real interrupt for 2 seconds, then removes it. It has a limit that
switches the interrupt off if it runs away. In the worst case the machine freezes and you
have to reset it. Nothing is written to disk and nothing is changed for the next boot.

## What to send back

The three files `RAM:vc4vbl-1.txt`, `RAM:vc4vbl-2.txt` and `RAM:vc4vbl-3.txt`, plus:

- your Pi model (3 B or 3 B+) and PiStorm model;
- the screen mode you were using;
- for step 3, whether anything strange happened (freeze, flicker, pointer stuck).

If step 2 or step 3 freezes the machine, tell us at which step. That is a useful result too.

## Options

| Option | Meaning |
|---|---|
| `SECS=n` | duration of each test in seconds, 1 to 10 (default 2) |
| `WATCH` | step 2 |
| `IRQ` | step 3 |

`WATCH` and `IRQ` can be given together. Without any option, only step 1 runs.
