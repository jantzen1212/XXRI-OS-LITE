# Login screen

`xxri-login` shows a password screen instead of the text prompt when
automatic login is off (the installer's "ask for login", Settings > General
> "Log in automatically", or the `noautologin` boot code). With autologin on,
and on the live ISO, nothing changes.

## Flow

tty1's getty runs `/sbin/autologin`, which logs root in once per boot. Root's
`.profile` sees `noautologin`; if the user's `~/.profile` and `~/.xsession`
both mention `xxri-login` (homes made before 2.7 don't) and X is wanted, it
drops `/tmp/.xxri-login` and logs the user in. The user's `.profile` then
`exec`s `startx`, so there is never a shell on tty1. `.xsession` starts X,
sets the resolution and wallpaper and runs `xxri-login` before the window
manager, compositor or dock. If that exits non-zero, X is killed and getty's
text prompt takes over. After the password the session starts as usual and
`.xsession` waits for X, because it is now tty1's session leader and
returning would hang up the desktop.

Log out (`xxri-desktop restart-session`) writes `noautologin` again, so the
next pass comes back here. Before this, a logout with autologin off left a
root shell.

## Password

`/etc/shadow` is root:staff 0660 on Tiny Core and the user is in staff, so
the program reads its own account with `getspnam()` and checks it with
`crypt()`, comparing in constant time. An empty password means an empty
field; `!` and `*` never match. A wrong password locks the field for 2
seconds. If the shadow file can't be read the program exits at start, so X
ends and the text prompt asks instead of leaving a screen nobody can pass.

It does not stop someone booting another medium, or anyone who already has a
shell as the user: the account keeps Tiny Core's passwordless sudo and owns
`~/.xsession`. If X dies and the user signs in at the text prompt, the login
screen asks once more.

## Screen

Wallpaper from the root window, darker towards the bottom; logo top left;
keyboard layout, battery, Wi-Fi and clock top right, read from sysfs because
the backends are slow at boot; then `~/.face` (or the initial on the brand
gradient), the name from the GECOS field, the password field and a line for
Caps Lock or a wrong password; Shut Down, Restart and Sleep at the bottom,
through `xxri-power`. Sleep only shows if `/sys/power/state` has `mem`. It is
white on the wallpaper in both Light and Dark mode.

## Tested

On the development host only: the program was compiled on its own and run on
a virtual X server with the real wallpaper tool (wrong password, Caps Lock,
right password gives exit 0); the password check was run against fake
shadow files with an MD5 hash, empty, locked and unreadable cases; the
profile flow ran under the device busybox with stand-ins for `login` and
`startx`.

Not tested: the PTXdist build and `-lcrypt` against Tiny Core's libcrypt, the
i586 binary, Xorg or Xfbdev, a real boot, the keyboard grab without a window
manager, `~/.face`, the battery and Wi-Fi icons on a laptop, suspend, and
Log out on a booted system.
