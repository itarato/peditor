Peditor
-------

Toy text editor.

[![asciicast](https://asciinema.org/a/LP3tSFwa2FxruDQ7nQ33PCw6F.svg)](https://asciinema.org/a/LP3tSFwa2FxruDQ7nQ33PCw6F)


### Compile

```bash
make clean
EXTRA_FLAGS="-DVERBOSE" make
```

### Test

```bash
make clean
make runtests
```

### Use

File operations:

- Open empty editor: `./pedit`
- Open file: `./pedit <FILENAME>`
- Save: `CTRL` + `s`
- Save as: `CTRL` + `o`

Editor management:

- Exit: `CTRL` + `q`

Text editing:

- Move: arrows
- Word jump: `CTRL` + left/right
- Vertical offsetting: `CTRL` + up/down
- Word delete: `CTRL` + backspace
- OS clipboard: OS bindings
- Internal clipboard: `CTRL` + c/v
- Undo/redo: `CTRL` + z/r
- Line delete: `CTRL` + d
- Selection: `SHIFT` + arrows
