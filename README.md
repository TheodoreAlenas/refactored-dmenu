# Refactored DMenu

## P.S.

An early 2023 project I regret going about.
It turns out that it's better to rebuild something at this scale.
Also I went back to the default appearance of DMenu
and I don't look like the screenshots below anymore.
If you want to continue this project for yourself,
you're more than welcome to do so,
but I won't touch it or use it.

## Original README content

No matter how hard I looked around,
nobody seems to have cleaned DMenu up for me.
Perhaps we should do it together.

I use DMenu for everything.

![image](glyph-dmenu.png)

DMenu can be inline input, auto complete input,
and output.

![image](confirm-dmenu.png)

However, if I wanted to modify its source code
I'd have to clean it up myself.

```c
if ((i += (lines > 0) ? bh : textw_clamp(prev->left->text, n)) > n * columns)
  break;
```

```c
    XFree(info);
  } else
#endif
  {
```

[DMenu](https://tools.suckless.org/dmenu/)
is an amazing project, but not made with
extensibility in mind. The way it's made
gives me an itch, and the way people use it
hurts my soul.

Well I didn't sit down and start cleaning,
but as I added new features
it started to get cleaner.

```c
int
main(int argc, char *argv[])
{
  int fast = handleargs(argc, argv);
  initxwin();
  considerbsdfail();
  int num_of_lines = grabandreadandgetnumlines(fast);
  setup(num_of_lines);
  run();
  return 1; /* unreachable */
}
```

The dream is that one day it's so clean that
one can add any feature within a reasonable
amount of time. I wish this was the standard
for all software to be honest. Currently there
are roadblocks such as making the number of
columns depend on the amount of input.

This is my first repository and I'm inexperienced.
Please let me know of anything I'm doing wrong.

## Priorities

- Separate into files until the includes are clean
- Make the code testable, it has been a need
- Remove all the globals

## On the other hand

- The Xinerama support has been removed
- BSD compatibility hasn't been tested
- Arguments are incompatible for now
- There is no reliable manual, for now
- DMenu patches won't work

The point is that you'll be able to modify it
by hand, by writing more code like I wished to.
One would argue that the point of DMenu was to be
easy to modify from the beginning,
but that's not been my experience.

## Usage

```sh
git clone https://github.com/TheodoreAlenas/refactored-dmenu
make
./dmenu --help  # see an overview of the commands
mv dmenu ~/.local/bin/dmenu  # Sorry! I'll fix 'make install'
```
